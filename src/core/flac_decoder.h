#ifndef RETRODAP_FLAC_DECODER_H
#define RETRODAP_FLAC_DECODER_H

#include <stdint.h>
#include <string>
#include <vector>

namespace retrodap {

struct TrackMetadata {
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    unsigned int trackNumber;
    unsigned int sampleRate;
    unsigned int bitsPerSample;
    unsigned int channels;
    uint64_t totalFrames;
    double durationSeconds;
    std::string artworkMimeType;
    size_t artworkSize;
    std::vector<uint8_t> artwork;

    TrackMetadata();
};

class FlacDecoder {
public:
    FlacDecoder();
    ~FlacDecoder();

    bool open(const std::string& path);
    void close();
    bool isOpen() const;

    uint64_t readFramesS16(int16_t* output, uint64_t frameCount);
    bool seekToFrame(uint64_t frameIndex);
    uint64_t currentFrame() const;

    unsigned int sampleRate() const;
    unsigned int channels() const;
    unsigned int bitsPerSample() const;
    uint64_t totalFrames() const;

    static bool readMetadata(const std::string& path, TrackMetadata& metadata);

private:
    FlacDecoder(const FlacDecoder&);
    FlacDecoder& operator=(const FlacDecoder&);

    void* decoder_;
};

} // namespace retrodap

#endif
