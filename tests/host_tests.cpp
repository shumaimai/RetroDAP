#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../src/core/flac_decoder.h"
#include "../src/core/library_scanner.h"
#include "../src/core/player_core.h"
#include "../src/core/ring_buffer.h"

using retrodap::FlacDecoder;
using retrodap::LibraryScanner;
using retrodap::SPSCRingBuffer;
using retrodap::TrackMetadata;

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " << #condition << std::endl; \
        ++failures; \
    } \
} while (0)

static std::vector<int16_t> readPcmFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    input.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(input.tellg());
    input.seekg(0, std::ios::beg);
    std::vector<int16_t> result(size / 2);
    input.read(reinterpret_cast<char*>(&result[0]), size);
    return result;
}

static void testMetadata(const std::string& fixture) {
    TrackMetadata metadata;
    CHECK(FlacDecoder::readMetadata(fixture, metadata));
    CHECK(metadata.sampleRate == 44100);
    CHECK(metadata.channels == 1);
    CHECK(metadata.title == "Sine Test");
    CHECK(metadata.artist == "RetroDAP");
    CHECK(metadata.album == "Host Tests");
    CHECK(metadata.trackNumber == 3);
    CHECK(metadata.totalFrames == 88200);
    CHECK(metadata.durationSeconds > 1.99 && metadata.durationSeconds < 2.01);
    CHECK(!metadata.artwork.empty());
}

static void testDecodeAndSeek(const std::string& fixture, const std::string& referencePath) {
    FlacDecoder decoder;
    CHECK(decoder.open(fixture));
    CHECK(decoder.sampleRate() == 44100);
    CHECK(decoder.channels() == 1);

    std::vector<int16_t> decoded;
    int16_t chunk[1024];
    uint64_t frames = 0;
    while ((frames = decoder.readFramesS16(chunk, 1024)) != 0) {
        decoded.insert(decoded.end(), chunk, chunk + frames);
    }
    std::vector<int16_t> reference = readPcmFile(referencePath);
    CHECK(decoded.size() * 2 == reference.size());
    size_t compareFrames = decoded.size() < reference.size() / 2 ? decoded.size() : reference.size() / 2;
    size_t mismatch = 0;
    for (size_t i = 0; i < compareFrames; ++i) {
        if (decoded[i] != reference[i * 2] || decoded[i] != reference[i * 2 + 1]) {
            ++mismatch;
        }
    }
    CHECK(mismatch == 0);

    CHECK(decoder.seekToFrame(22050));
    int16_t sample = 0;
    CHECK(decoder.readFramesS16(&sample, 1) == 1);
    CHECK(sample == reference[22050 * 2]);
    CHECK(decoder.currentFrame() >= 22051);
}

static void testRingBuffer() {
    SPSCRingBuffer ring(1024);
    CHECK(ring.capacity() >= 1023);
    std::vector<uint8_t> input(2048);
    std::vector<uint8_t> output(2048);
    for (size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<uint8_t>((i * 37 + 11) & 0xff);
    }

    size_t firstWrite = ring.write(&input[0], 900);
    CHECK(firstWrite == 900);
    CHECK(ring.read(&output[0], 500) == 500);
    CHECK(memcmp(&output[0], &input[0], 500) == 0);
    size_t secondWrite = ring.write(&input[900], 900);
    CHECK(secondWrite > 0);
    size_t totalRead = 500;
    while (ring.availableRead() != 0 && totalRead < output.size()) {
        totalRead += ring.read(&output[totalRead], output.size() - totalRead);
    }
    for (size_t i = 500; i < 500 + secondWrite; ++i) {
        if (output[i] != input[i]) {
            CHECK(false);
            break;
        }
    }
    ring.reset();
    CHECK(ring.availableRead() == 0);
}

static void testLibraryScan(const std::string& root) {
    std::vector<std::string> files = LibraryScanner::scanFlacFiles(root);
    CHECK(files.size() >= 2);
    std::vector<TrackMetadata> metadata = LibraryScanner::scanMetadata(root);
    CHECK(metadata.size() == files.size());

    RDLibrary* library = rdap_library_scan(root.c_str());
    CHECK(library != NULL);
    bool foundArtwork = false;
    for (size_t i = 0; library != NULL && i < rdap_library_count(library); ++i) {
        RDTrackInfo info;
        if (!rdap_library_info(library, i, &info) || strcmp(info.title, "Sine Test") != 0) {
            continue;
        }
        CHECK(info.artworkSize > 0);
        std::vector<uint8_t> artwork(info.artworkSize);
        CHECK(rdap_library_artwork(library, i, &artwork[0], artwork.size()) == info.artworkSize);
        CHECK(artwork.size() > 8 && artwork[0] == 0x89 && artwork[1] == 'P');
        foundArtwork = true;
    }
    CHECK(foundArtwork);
    rdap_library_destroy(library);
}

static size_t renderFrames(RDEngine* engine, size_t frames) {
    int16_t scratch[128 * 2];
    size_t rendered = 0;
    for (int i = 0; i < 100 && rendered < frames; ++i) {
        rendered += rdap_engine_render(engine, scratch, 128);
        usleep(10000);
    }
    return rendered;
}

static void testPlayerCore(const std::string& hires) {
    const char* queue[] = { hires.c_str() };
    RDEngine* engine = rdap_engine_create(256 * 1024);
    CHECK(engine != NULL);
    CHECK(rdap_engine_set_queue(engine, queue, 1));
    CHECK(rdap_engine_queue_count(engine) == 1);
    CHECK(rdap_engine_play_index(engine, 0));

    bool nonzero = false;
    int16_t output[512 * 2];
    for (int i = 0; i < 80 && !nonzero; ++i) {
        usleep(5000);
        memset(output, 0, sizeof(output));
        rdap_engine_render(engine, output, 512);
        for (size_t j = 0; j < sizeof(output) / sizeof(output[0]); ++j) {
            if (output[j] != 0) {
                nonzero = true;
                break;
            }
        }
    }
    CHECK(nonzero);
    CHECK(rdap_engine_output_sample_rate(engine) == 44100);
    CHECK(rdap_engine_is_playing(engine));

    RDTrackInfo info;
    CHECK(rdap_engine_current_info(engine, &info));
    CHECK(info.sampleRate == 96000);
    CHECK(info.outputSampleRate == 44100);

    rdap_engine_seek_seconds(engine, 0.25);
    usleep(50000);
    CHECK(rdap_engine_position_seconds(engine) >= 0.24);
    rdap_engine_pause(engine);
    memset(output, 1, sizeof(output));
    CHECK(rdap_engine_render(engine, output, 128) == 0);
    bool allZero = true;
    for (size_t j = 0; j < 128 * 2; ++j) {
        if (output[j] != 0) {
            allZero = false;
        }
    }
    CHECK(allZero);
    rdap_engine_resume(engine);
    CHECK(renderFrames(engine, 128) > 0);

    int16_t drain[128 * 2];
    for (int i = 0; i < 300 && rdap_engine_is_playing(engine); ++i) {
        rdap_engine_render(engine, drain, 128);
        usleep(2000);
    }
    CHECK(!rdap_engine_is_playing(engine));
    rdap_engine_seek_seconds(engine, 0.25);
    usleep(20000);
    rdap_engine_resume(engine);
    CHECK(renderFrames(engine, 128) > 0);
    CHECK(rdap_engine_position_seconds(engine) >= 0.24);
    rdap_engine_destroy(engine);
}

int main(int argc, char** argv) {
    std::string fixtureRoot = argc > 1 ? argv[1] : "tests/fixtures";
    std::string fixture = fixtureRoot + "/test.flac";
    std::string reference = fixtureRoot + "/ref-sine.pcm";
    std::string hires = fixtureRoot + "/hires.flac";

    testMetadata(fixture);
    testDecodeAndSeek(fixture, reference);
    testRingBuffer();
    testLibraryScan(fixtureRoot);
    testPlayerCore(hires);

    if (failures != 0) {
        std::cerr << failures << " host test failure(s)" << std::endl;
        return 1;
    }
    std::cout << "All host core tests passed" << std::endl;
    return 0;
}
