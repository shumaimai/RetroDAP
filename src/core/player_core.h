#ifndef RETRODAP_PLAYER_CORE_H
#define RETRODAP_PLAYER_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RDEngine RDEngine;
typedef struct RDLibrary RDLibrary;

typedef struct RDTrackInfo {
    char path[1024];
    char title[256];
    char artist[256];
    char album[256];
    char artworkMimeType[64];
    unsigned int trackNumber;
    unsigned int sampleRate;
    unsigned int bitsPerSample;
    unsigned int channels;
    unsigned int outputSampleRate;
    uint64_t totalFrames;
    double durationSeconds;
    size_t artworkSize;
    int queueIndex;
} RDTrackInfo;

RDEngine* rdap_engine_create(size_t ringBufferBytes);
void rdap_engine_destroy(RDEngine* engine);

int rdap_engine_set_queue(RDEngine* engine, const char** paths, size_t count);
int rdap_engine_play_index(RDEngine* engine, size_t index);
void rdap_engine_pause(RDEngine* engine);
void rdap_engine_resume(RDEngine* engine);
void rdap_engine_stop(RDEngine* engine);
void rdap_engine_next(RDEngine* engine);
void rdap_engine_previous(RDEngine* engine);
void rdap_engine_seek_seconds(RDEngine* engine, double seconds);

/* Called from the CoreAudio render thread. Writes interleaved stereo s16 PCM. */
size_t rdap_engine_render(RDEngine* engine, int16_t* output, size_t frameCount);

int rdap_engine_is_playing(RDEngine* engine);
size_t rdap_engine_queue_count(RDEngine* engine);
int rdap_engine_current_index(RDEngine* engine);
unsigned int rdap_engine_output_sample_rate(RDEngine* engine);
double rdap_engine_position_seconds(RDEngine* engine);
int rdap_engine_current_info(RDEngine* engine, RDTrackInfo* info);
size_t rdap_engine_current_artwork(RDEngine* engine, void* output, size_t capacity);

RDLibrary* rdap_library_scan(const char* rootPath);
void rdap_library_destroy(RDLibrary* library);
size_t rdap_library_count(RDLibrary* library);
int rdap_library_info(RDLibrary* library, size_t index, RDTrackInfo* info);
size_t rdap_library_artwork(RDLibrary* library, size_t index, void* output, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
