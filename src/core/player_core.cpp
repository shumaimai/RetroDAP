#include "player_core.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "flac_decoder.h"
#include "library_scanner.h"
#include "ring_buffer.h"

using retrodap::FlacDecoder;
using retrodap::LibraryScanner;
using retrodap::SPSCRingBuffer;
using retrodap::TrackMetadata;

struct RDEngine {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    bool threadStarted;

    std::vector<std::string> queue;
    SPSCRingBuffer ring;

    volatile int exitThread;
    volatile int playing;
    volatile int pendingIndex;
    volatile int seekPending;
    volatile double seekSeconds;
    volatile int currentIndex;
    volatile unsigned int outputSampleRate;
    volatile uint32_t producedFrames;
    volatile uint32_t consumedFrames;
    volatile uint32_t trackStartProducedFrames;

    TrackMetadata currentMetadata;

    explicit RDEngine(size_t ringBytes)
        : threadStarted(false), ring(ringBytes), exitThread(0), playing(0),
          pendingIndex(-1), seekPending(0), seekSeconds(0.0), currentIndex(-1),
          outputSampleRate(44100), producedFrames(0), consumedFrames(0), trackStartProducedFrames(0) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&condition, NULL);
    }
};

static void copyString(char* destination, size_t capacity, const std::string& source) {
    if (capacity == 0) {
        return;
    }
    size_t count = std::min(capacity - 1, source.size());
    memcpy(destination, source.data(), count);
    destination[count] = '\0';
}

static void fillTrackInfo(const TrackMetadata& metadata, RDTrackInfo* info, int queueIndex, unsigned int outputRate) {
    memset(info, 0, sizeof(*info));
    copyString(info->path, sizeof(info->path), metadata.path);
    copyString(info->title, sizeof(info->title), metadata.title);
    copyString(info->artist, sizeof(info->artist), metadata.artist);
    copyString(info->album, sizeof(info->album), metadata.album);
    copyString(info->artworkMimeType, sizeof(info->artworkMimeType), metadata.artworkMimeType);
    info->trackNumber = metadata.trackNumber;
    info->sampleRate = metadata.sampleRate;
    info->bitsPerSample = metadata.bitsPerSample;
    info->channels = metadata.channels;
    info->outputSampleRate = outputRate;
    info->totalFrames = metadata.totalFrames;
    info->durationSeconds = metadata.durationSeconds;
    info->artworkSize = metadata.artworkSize;
    info->queueIndex = queueIndex;
}

static unsigned int chooseOutputRate(unsigned int sourceRate) {
    if (sourceRate == 44100 || sourceRate == 48000) {
        return sourceRate;
    }
    return 44100;
}

static void convertToStereo(const int16_t* input, size_t frames, unsigned int channels, std::vector<int16_t>& output) {
    output.resize(frames * 2);
    if (channels == 1) {
        for (size_t i = 0; i < frames; ++i) {
            output[i * 2] = input[i];
            output[i * 2 + 1] = input[i];
        }
    } else {
        for (size_t i = 0; i < frames; ++i) {
            output[i * 2] = input[i * channels];
            output[i * 2 + 1] = input[i * channels + 1];
        }
    }
}

static void resampleNearest(const std::vector<int16_t>& input, unsigned int inputRate,
    unsigned int outputRate, double& position, std::vector<int16_t>& output) {
    output.clear();
    if (inputRate == outputRate || input.empty()) {
        output = input;
        position = 0.0;
        return;
    }

    double ratio = static_cast<double>(inputRate) / static_cast<double>(outputRate);
    size_t inputFrames = input.size() / 2;
    while (position < static_cast<double>(inputFrames)) {
        size_t index = static_cast<size_t>(position);
        if (index >= inputFrames) {
            index = inputFrames - 1;
        }
        output.push_back(input[index * 2]);
        output.push_back(input[index * 2 + 1]);
        position += ratio;
    }
    position -= static_cast<double>(inputFrames);
    if (position < 0.0) {
        position = 0.0;
    }
}

static void updateCurrentMetadata(RDEngine* engine, const TrackMetadata& metadata, unsigned int outputRate, int index) {
    pthread_mutex_lock(&engine->mutex);
    engine->currentMetadata = metadata;
    engine->currentIndex = index;
    engine->outputSampleRate = outputRate;
    pthread_mutex_unlock(&engine->mutex);
}

static bool handleSeekIfRequested(RDEngine* engine, FlacDecoder& decoder) {
    bool shouldSeek = false;
    double seconds = 0.0;
    pthread_mutex_lock(&engine->mutex);
    if (engine->seekPending) {
        shouldSeek = true;
        seconds = engine->seekSeconds;
        engine->seekPending = 0;
    }
    pthread_mutex_unlock(&engine->mutex);

    if (!shouldSeek) {
        return false;
    }

    uint64_t frame = 0;
    if (decoder.sampleRate() != 0 && seconds > 0.0) {
        frame = static_cast<uint64_t>(seconds * decoder.sampleRate());
    }
    decoder.seekToFrame(frame);
    engine->ring.reset();
    engine->producedFrames = 0;
    engine->consumedFrames = 0;
    engine->trackStartProducedFrames = 0;
    return true;
}

static void* decodeThreadMain(void* userData) {
    RDEngine* engine = static_cast<RDEngine*>(userData);

    while (!engine->exitThread) {
        pthread_mutex_lock(&engine->mutex);
        while (!engine->exitThread && (!engine->playing || engine->queue.empty())) {
            pthread_cond_wait(&engine->condition, &engine->mutex);
        }
        if (engine->exitThread) {
            pthread_mutex_unlock(&engine->mutex);
            break;
        }

        int index = engine->pendingIndex >= 0 ? engine->pendingIndex : engine->currentIndex;
        if (index < 0) {
            index = 0;
        }
        if (index >= static_cast<int>(engine->queue.size())) {
            index = static_cast<int>(engine->queue.size()) - 1;
        }
        engine->pendingIndex = -1;
        std::string path = engine->queue[static_cast<size_t>(index)];
        pthread_mutex_unlock(&engine->mutex);

        FlacDecoder decoder;
        if (!decoder.open(path)) {
            pthread_mutex_lock(&engine->mutex);
            if (index + 1 < static_cast<int>(engine->queue.size())) {
                engine->pendingIndex = index + 1;
            } else {
                engine->playing = 0;
            }
            pthread_mutex_unlock(&engine->mutex);
            continue;
        }

        TrackMetadata metadata;
        if (!FlacDecoder::readMetadata(path, metadata)) {
            metadata.path = path;
            metadata.sampleRate = decoder.sampleRate();
            metadata.bitsPerSample = decoder.bitsPerSample();
            metadata.channels = decoder.channels();
            metadata.totalFrames = decoder.totalFrames();
            metadata.durationSeconds = decoder.sampleRate() == 0 ? 0.0 :
                static_cast<double>(decoder.totalFrames()) / decoder.sampleRate();
        }
        unsigned int inputRate = decoder.sampleRate();
        unsigned int outputRate = chooseOutputRate(inputRate);
        unsigned int channels = std::max(1u, decoder.channels());
        engine->trackStartProducedFrames = engine->producedFrames;
        updateCurrentMetadata(engine, metadata, outputRate, index);

        bool restart = false;
        bool endOfStream = false;
        double resamplePosition = 0.0;
        const size_t inputFrameChunk = 1024;
        std::vector<int16_t> raw(inputFrameChunk * channels);
        std::vector<int16_t> stereo;
        std::vector<int16_t> converted;

        while (!engine->exitThread && !restart) {
            pthread_mutex_lock(&engine->mutex);
            bool hasPendingIndex = engine->pendingIndex >= 0;
            pthread_mutex_unlock(&engine->mutex);
            if (hasPendingIndex) {
                engine->ring.reset();
                engine->producedFrames = 0;
                engine->consumedFrames = 0;
                engine->trackStartProducedFrames = 0;
                restart = true;
                break;
            }

            if (handleSeekIfRequested(engine, decoder)) {
                endOfStream = false;
                resamplePosition = 0.0;
            }

            if (endOfStream) {
                if (engine->ring.availableRead() == 0) {
                    engine->playing = 0;
                }
                usleep(5000);
                continue;
            }

            if (!engine->playing) {
                usleep(10000);
                continue;
            }

            size_t writableFrames = engine->ring.availableWrite() / 4;
            if (writableFrames < 256) {
                usleep(3000);
                continue;
            }

            uint64_t framesRead = decoder.readFramesS16(&raw[0], inputFrameChunk);
            if (framesRead == 0) {
                pthread_mutex_lock(&engine->mutex);
                if (index + 1 < static_cast<int>(engine->queue.size())) {
                    engine->pendingIndex = index + 1;
                    restart = true;
                } else {
                    endOfStream = true;
                }
                pthread_mutex_unlock(&engine->mutex);
                continue;
            }

            convertToStereo(&raw[0], static_cast<size_t>(framesRead), channels, stereo);
            resampleNearest(stereo, inputRate, outputRate, resamplePosition, converted);
            size_t outputFrames = converted.size() / 2;
            size_t framesToWrite = std::min(outputFrames, engine->ring.availableWrite() / 4);
            if (framesToWrite == 0) {
                continue;
            }
            size_t bytesToWrite = framesToWrite * 4;
            size_t written = engine->ring.write(&converted[0], bytesToWrite);
            engine->producedFrames += static_cast<uint32_t>(written / 4);
        }
        decoder.close();
    }
    return NULL;
}

RDEngine* rdap_engine_create(size_t ringBufferBytes) {
    RDEngine* engine = new RDEngine(ringBufferBytes == 0 ? 1024 * 1024 : ringBufferBytes);
    if (pthread_create(&engine->thread, NULL, decodeThreadMain, engine) == 0) {
        engine->threadStarted = true;
    } else {
        delete engine;
        return NULL;
    }
    return engine;
}

void rdap_engine_destroy(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    pthread_mutex_lock(&engine->mutex);
    engine->exitThread = 1;
    pthread_cond_signal(&engine->condition);
    pthread_mutex_unlock(&engine->mutex);
    if (engine->threadStarted) {
        pthread_join(engine->thread, NULL);
    }
    pthread_cond_destroy(&engine->condition);
    pthread_mutex_destroy(&engine->mutex);
    delete engine;
}

int rdap_engine_set_queue(RDEngine* engine, const char** paths, size_t count) {
    if (engine == NULL || (paths == NULL && count != 0)) {
        return 0;
    }
    pthread_mutex_lock(&engine->mutex);
    engine->queue.clear();
    for (size_t i = 0; i < count; ++i) {
        if (paths[i] != NULL) {
            engine->queue.push_back(paths[i]);
        }
    }
    engine->playing = 0;
    engine->pendingIndex = -1;
    engine->seekPending = 0;
    engine->currentIndex = engine->queue.empty() ? -1 : 0;
    pthread_mutex_unlock(&engine->mutex);
    engine->ring.reset();
    engine->producedFrames = 0;
    engine->consumedFrames = 0;
    engine->trackStartProducedFrames = 0;
    return 1;
}

int rdap_engine_play_index(RDEngine* engine, size_t index) {
    if (engine == NULL) {
        return 0;
    }
    pthread_mutex_lock(&engine->mutex);
    if (index >= engine->queue.size()) {
        pthread_mutex_unlock(&engine->mutex);
        return 0;
    }
    engine->pendingIndex = static_cast<int>(index);
    engine->seekPending = 0;
    engine->playing = 1;
    pthread_cond_signal(&engine->condition);
    pthread_mutex_unlock(&engine->mutex);
    return 1;
}

void rdap_engine_pause(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    engine->playing = 0;
}

void rdap_engine_resume(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    pthread_mutex_lock(&engine->mutex);
    if (!engine->queue.empty() && engine->currentIndex >= 0) {
        engine->playing = 1;
        pthread_cond_signal(&engine->condition);
    }
    pthread_mutex_unlock(&engine->mutex);
}

void rdap_engine_stop(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    engine->playing = 0;
    engine->ring.reset();
    engine->producedFrames = 0;
    engine->consumedFrames = 0;
    engine->trackStartProducedFrames = 0;
}

void rdap_engine_next(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    pthread_mutex_lock(&engine->mutex);
    if (!engine->queue.empty()) {
        int base = engine->pendingIndex >= 0 ? engine->pendingIndex : engine->currentIndex;
        if (base < 0) {
            base = 0;
        }
        engine->pendingIndex = (base + 1) % static_cast<int>(engine->queue.size());
        engine->playing = 1;
        pthread_cond_signal(&engine->condition);
    }
    pthread_mutex_unlock(&engine->mutex);
}

void rdap_engine_previous(RDEngine* engine) {
    if (engine == NULL) {
        return;
    }
    pthread_mutex_lock(&engine->mutex);
    if (!engine->queue.empty()) {
        int base = engine->pendingIndex >= 0 ? engine->pendingIndex : engine->currentIndex;
        if (base <= 0) {
            base = static_cast<int>(engine->queue.size());
        }
        engine->pendingIndex = (base - 1) % static_cast<int>(engine->queue.size());
        engine->playing = 1;
        pthread_cond_signal(&engine->condition);
    }
    pthread_mutex_unlock(&engine->mutex);
}

void rdap_engine_seek_seconds(RDEngine* engine, double seconds) {
    if (engine == NULL || seconds < 0.0) {
        return;
    }
    pthread_mutex_lock(&engine->mutex);
    engine->seekSeconds = seconds;
    engine->seekPending = 1;
    pthread_cond_signal(&engine->condition);
    pthread_mutex_unlock(&engine->mutex);
}

size_t rdap_engine_render(RDEngine* engine, int16_t* output, size_t frameCount) {
    if (engine == NULL || output == NULL || frameCount == 0) {
        return 0;
    }
    if (!engine->playing) {
        memset(output, 0, frameCount * 4);
        return 0;
    }
    size_t bytesRequested = frameCount * 4;
    size_t bytesRead = engine->ring.read(output, bytesRequested);
    if (bytesRead < bytesRequested) {
        memset(reinterpret_cast<uint8_t*>(output) + bytesRead, 0, bytesRequested - bytesRead);
    }
    engine->consumedFrames += static_cast<uint32_t>(bytesRead / 4);
    return bytesRead / 4;
}

int rdap_engine_is_playing(RDEngine* engine) {
    return engine != NULL && engine->playing;
}

size_t rdap_engine_queue_count(RDEngine* engine) {
    if (engine == NULL) {
        return 0;
    }
    pthread_mutex_lock(&engine->mutex);
    size_t count = engine->queue.size();
    pthread_mutex_unlock(&engine->mutex);
    return count;
}

int rdap_engine_current_index(RDEngine* engine) {
    return engine == NULL ? -1 : engine->currentIndex;
}

unsigned int rdap_engine_output_sample_rate(RDEngine* engine) {
    return engine == NULL ? 44100 : engine->outputSampleRate;
}

double rdap_engine_position_seconds(RDEngine* engine) {
    if (engine == NULL || engine->outputSampleRate == 0) {
        return 0.0;
    }
    uint32_t consumed = engine->consumedFrames;
    uint32_t start = engine->trackStartProducedFrames;
    int32_t elapsedFrames = static_cast<int32_t>(consumed - start);
    if (elapsedFrames < 0) {
        return 0.0;
    }
    return static_cast<double>(elapsedFrames) / engine->outputSampleRate;
}

int rdap_engine_current_info(RDEngine* engine, RDTrackInfo* info) {
    if (engine == NULL || info == NULL) {
        return 0;
    }
    pthread_mutex_lock(&engine->mutex);
    if (engine->currentIndex < 0) {
        pthread_mutex_unlock(&engine->mutex);
        return 0;
    }
    fillTrackInfo(engine->currentMetadata, info, engine->currentIndex, engine->outputSampleRate);
    pthread_mutex_unlock(&engine->mutex);
    return 1;
}

size_t rdap_engine_current_artwork(RDEngine* engine, void* output, size_t capacity) {
    if (engine == NULL) {
        return 0;
    }
    pthread_mutex_lock(&engine->mutex);
    size_t size = engine->currentMetadata.artwork.size();
    if (output != NULL && capacity != 0) {
        size_t count = std::min(size, capacity);
        memcpy(output, &engine->currentMetadata.artwork[0], count);
    }
    pthread_mutex_unlock(&engine->mutex);
    return size;
}

struct RDLibrary {
    std::vector<TrackMetadata> tracks;
};

RDLibrary* rdap_library_scan(const char* rootPath) {
    if (rootPath == NULL) {
        return NULL;
    }
    RDLibrary* library = new RDLibrary;
    library->tracks = LibraryScanner::scanMetadata(rootPath);
    return library;
}

void rdap_library_destroy(RDLibrary* library) {
    delete library;
}

size_t rdap_library_count(RDLibrary* library) {
    return library == NULL ? 0 : library->tracks.size();
}

int rdap_library_info(RDLibrary* library, size_t index, RDTrackInfo* info) {
    if (library == NULL || info == NULL || index >= library->tracks.size()) {
        return 0;
    }
    fillTrackInfo(library->tracks[index], info, static_cast<int>(index), chooseOutputRate(library->tracks[index].sampleRate));
    return 1;
}

size_t rdap_library_artwork(RDLibrary* library, size_t index, void* output, size_t capacity) {
    if (library == NULL || index >= library->tracks.size()) {
        return 0;
    }
    size_t size = library->tracks[index].artworkSize;
    if (output != NULL && capacity != 0 && size != 0) {
        TrackMetadata fullMetadata;
        if (FlacDecoder::readMetadata(library->tracks[index].path, fullMetadata) && !fullMetadata.artwork.empty()) {
            memcpy(output, &fullMetadata.artwork[0], std::min(fullMetadata.artwork.size(), capacity));
        }
    }
    return size;
}
