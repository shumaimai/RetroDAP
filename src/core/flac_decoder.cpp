#include "flac_decoder.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "dr_flac.h"

namespace retrodap {

TrackMetadata::TrackMetadata()
    : trackNumber(0), sampleRate(0), bitsPerSample(0), channels(0),
      totalFrames(0), durationSeconds(0.0), artworkSize(0) {
}

static bool keyEquals(const char* text, size_t length, const char* key) {
    size_t keyLength = std::strlen(key);
    if (length <= keyLength || text[keyLength] != '=') {
        return false;
    }
    for (size_t i = 0; i < keyLength; ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != key[i]) {
            return false;
        }
    }
    return true;
}

static void copyCommentValue(const char* text, size_t length, const char* key, std::string& output) {
    if (!keyEquals(text, length, key)) {
        return;
    }
    size_t keyLength = std::strlen(key);
    output.assign(text + keyLength + 1, length - keyLength - 1);
}

static void metadataCallback(void* userData, drflac_metadata* metadata) {
    TrackMetadata* output = static_cast<TrackMetadata*>(userData);
    if (metadata->type == DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO) {
        output->sampleRate = metadata->data.streaminfo.sampleRate;
        output->bitsPerSample = metadata->data.streaminfo.bitsPerSample;
        output->channels = metadata->data.streaminfo.channels;
        output->totalFrames = metadata->data.streaminfo.totalPCMFrameCount;
        if (output->sampleRate != 0) {
            output->durationSeconds = static_cast<double>(output->totalFrames) / output->sampleRate;
        }
    } else if (metadata->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
        drflac_vorbis_comment_iterator iterator;
        drflac_init_vorbis_comment_iterator(&iterator,
            metadata->data.vorbis_comment.commentCount,
            metadata->data.vorbis_comment.pComments);
        drflac_uint32 length = 0;
        const char* rawComment = NULL;
        while ((rawComment = drflac_next_vorbis_comment(&iterator, &length)) != NULL) {
            copyCommentValue(rawComment, length, "title", output->title);
            copyCommentValue(rawComment, length, "artist", output->artist);
            copyCommentValue(rawComment, length, "album", output->album);
            std::string track;
            copyCommentValue(rawComment, length, "tracknumber", track);
            if (track.empty()) {
                copyCommentValue(rawComment, length, "track", track);
            }
            if (!track.empty()) {
                output->trackNumber = static_cast<unsigned int>(std::strtoul(track.c_str(), NULL, 10));
            }
        }
    } else if (metadata->type == DRFLAC_METADATA_BLOCK_TYPE_PICTURE) {
        output->artworkSize = metadata->data.picture.pictureDataSize;
        if (metadata->data.picture.mime != NULL && metadata->data.picture.mimeLength != 0) {
            output->artworkMimeType.assign(metadata->data.picture.mime, metadata->data.picture.mimeLength);
        }
        if (metadata->data.picture.pPictureData != NULL && metadata->data.picture.pictureDataSize != 0) {
            output->artwork.assign(metadata->data.picture.pPictureData,
                metadata->data.picture.pPictureData + metadata->data.picture.pictureDataSize);
        }
    }
}

FlacDecoder::FlacDecoder() : decoder_(NULL) {
}

FlacDecoder::~FlacDecoder() {
    close();
}

bool FlacDecoder::open(const std::string& path) {
    close();
    decoder_ = static_cast<void*>(drflac_open_file(path.c_str(), NULL));
    return decoder_ != NULL;
}

void FlacDecoder::close() {
    if (decoder_ != NULL) {
        drflac_close(static_cast<drflac*>(decoder_));
        decoder_ = NULL;
    }
}

bool FlacDecoder::isOpen() const {
    return decoder_ != NULL;
}

uint64_t FlacDecoder::readFramesS16(int16_t* output, uint64_t frameCount) {
    if (decoder_ == NULL) {
        return 0;
    }
    return drflac_read_pcm_frames_s16(static_cast<drflac*>(decoder_), frameCount, output);
}

bool FlacDecoder::seekToFrame(uint64_t frameIndex) {
    if (decoder_ == NULL) {
        return false;
    }
    return drflac_seek_to_pcm_frame(static_cast<drflac*>(decoder_), frameIndex) != 0;
}

uint64_t FlacDecoder::currentFrame() const {
    return decoder_ == NULL ? 0 : static_cast<drflac*>(decoder_)->currentPCMFrame;
}

unsigned int FlacDecoder::sampleRate() const {
    return decoder_ == NULL ? 0 : static_cast<drflac*>(decoder_)->sampleRate;
}

unsigned int FlacDecoder::channels() const {
    return decoder_ == NULL ? 0 : static_cast<drflac*>(decoder_)->channels;
}

unsigned int FlacDecoder::bitsPerSample() const {
    return decoder_ == NULL ? 0 : static_cast<drflac*>(decoder_)->bitsPerSample;
}

uint64_t FlacDecoder::totalFrames() const {
    return decoder_ == NULL ? 0 : static_cast<drflac*>(decoder_)->totalPCMFrameCount;
}

bool FlacDecoder::readMetadata(const std::string& path, TrackMetadata& metadata) {
    metadata = TrackMetadata();
    metadata.path = path;
    drflac* decoder = drflac_open_file_with_metadata(path.c_str(), metadataCallback, &metadata, NULL);
    if (decoder == NULL) {
        return false;
    }
    if (metadata.sampleRate == 0) {
        metadata.sampleRate = decoder->sampleRate;
        metadata.bitsPerSample = decoder->bitsPerSample;
        metadata.channels = decoder->channels;
        metadata.totalFrames = decoder->totalPCMFrameCount;
        if (metadata.sampleRate != 0) {
            metadata.durationSeconds = static_cast<double>(metadata.totalFrames) / metadata.sampleRate;
        }
    }
    if (metadata.title.empty()) {
        size_t slash = path.find_last_of('/');
        size_t dot = path.find_last_of('.');
        std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
        if (dot != std::string::npos && dot > slash) {
            base = base.substr(0, dot - (slash == std::string::npos ? 0 : slash + 1));
        }
        metadata.title = base;
    }
    drflac_close(decoder);
    return true;
}

} // namespace retrodap
