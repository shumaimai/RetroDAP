#ifndef RETRODAP_RING_BUFFER_H
#define RETRODAP_RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace retrodap {

class SPSCRingBuffer {
public:
    explicit SPSCRingBuffer(size_t capacityBytes = 2 * 1024 * 1024);

    size_t write(const void* data, size_t bytes);
    size_t read(void* data, size_t bytes);
    void reset();

    size_t availableRead() const;
    size_t availableWrite() const;
    size_t capacity() const;

private:
    static size_t roundPowerOfTwo(size_t value);

    std::vector<uint8_t> buffer_;
    volatile uint32_t readIndex_;
    volatile uint32_t writeIndex_;
    volatile int activeReaders_;
    volatile int resetInProgress_;
};

} // namespace retrodap

#endif
