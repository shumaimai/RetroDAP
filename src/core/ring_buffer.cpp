#include "ring_buffer.h"

#include <algorithm>
#include <cstring>
#include <sched.h>

namespace retrodap {

static inline void memoryBarrier() {
    __sync_synchronize();
}

SPSCRingBuffer::SPSCRingBuffer(size_t capacityBytes)
    : buffer_(roundPowerOfTwo(capacityBytes)), readIndex_(0), writeIndex_(0),
      activeReaders_(0), activeWriters_(0), resetInProgress_(0) {
}

size_t SPSCRingBuffer::roundPowerOfTwo(size_t value) {
    size_t result = 1024;
    while (result < value && result < (static_cast<size_t>(1) << 30)) {
        result <<= 1;
    }
    return result;
}

size_t SPSCRingBuffer::capacity() const {
    return buffer_.size() - 1;
}

size_t SPSCRingBuffer::availableRead() const {
    uint32_t writeIndex = writeIndex_;
    memoryBarrier();
    uint32_t readIndex = readIndex_;
    return (writeIndex - readIndex) & static_cast<uint32_t>(buffer_.size() - 1);
}

size_t SPSCRingBuffer::availableWrite() const {
    return capacity() - availableRead();
}

size_t SPSCRingBuffer::write(const void* data, size_t bytes) {
    if (resetInProgress_) {
        return 0;
    }
    __sync_fetch_and_add(&activeWriters_, 1);
    memoryBarrier();
    if (resetInProgress_) {
        __sync_fetch_and_sub(&activeWriters_, 1);
        return 0;
    }

    const uint8_t* source = static_cast<const uint8_t*>(data);
    size_t count = std::min(bytes, availableWrite());
    uint32_t writeIndex = writeIndex_;
    uint32_t mask = static_cast<uint32_t>(buffer_.size() - 1);

    size_t size = buffer_.size();
    size_t first = std::min(count, size - writeIndex);
    memcpy(&buffer_[writeIndex], source, first);
    if (count > first) {
        memcpy(&buffer_[0], source + first, count - first);
    }
    memoryBarrier();
    writeIndex_ = (writeIndex + static_cast<uint32_t>(count)) & mask;
    memoryBarrier();
    __sync_fetch_and_sub(&activeWriters_, 1);
    return count;
}

size_t SPSCRingBuffer::read(void* data, size_t bytes) {
    if (resetInProgress_) {
        return 0;
    }
    __sync_fetch_and_add(&activeReaders_, 1);
    memoryBarrier();
    if (resetInProgress_) {
        __sync_fetch_and_sub(&activeReaders_, 1);
        return 0;
    }

    uint8_t* destination = static_cast<uint8_t*>(data);
    size_t count = std::min(bytes, availableRead());
    uint32_t readIndex = readIndex_;
    uint32_t mask = static_cast<uint32_t>(buffer_.size() - 1);

    size_t size = buffer_.size();
    size_t first = std::min(count, size - readIndex);
    memcpy(destination, &buffer_[readIndex], first);
    if (count > first) {
        memcpy(destination + first, &buffer_[0], count - first);
    }
    memoryBarrier();
    readIndex_ = (readIndex + static_cast<uint32_t>(count)) & mask;
    memoryBarrier();
    __sync_fetch_and_sub(&activeReaders_, 1);
    return count;
}

void SPSCRingBuffer::reset() {
    while (__sync_val_compare_and_swap(&resetInProgress_, 0, 1) != 0) {
        sched_yield();
    }
    memoryBarrier();
    while (activeReaders_ != 0 || activeWriters_ != 0) {
        sched_yield();
    }
    readIndex_ = 0;
    writeIndex_ = 0;
    memoryBarrier();
    resetInProgress_ = 0;
    memoryBarrier();
}

} // namespace retrodap
