#pragma once

#include <atomic>

namespace app {

// scrolling buffer that uses a memory mapped circular buffer
// uses two adjacent virtual memory pages which point to the same underlying physical memory
// this makes circular buffer logic simpler - no need to prevent overrun
class ScrollingBuffer 
{
private:
    char *m_ring_buffer;
    char *m_ring_buffer_mirror;
    const unsigned int m_max_size = 0x10000;
    std::atomic<size_t> m_curr_size;
    size_t m_curr_write_index;
    std::atomic<size_t> m_curr_read_index;
public:
    ScrollingBuffer();
    ~ScrollingBuffer();
    inline char *GetReadBuffer()  { return &m_ring_buffer[m_curr_read_index]; }
    inline char *GetWriteBuffer() { return &m_ring_buffer[m_curr_write_index]; }
    inline size_t GetReadSize() { return m_curr_size; }
    inline size_t GetMaxSize() { return m_max_size; }
    void IncrementIndex(const size_t size);
};

}
