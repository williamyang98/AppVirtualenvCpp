#pragma once

#include <atomic>
#include <thread>
#include <memory>

#include "environ.h"
#include "app_schema.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

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

// creates a process with the specified environment and app configuration
// attaches a thread with a scrolling buffer to read from it
class AppProcess 
{
private:
    std::atomic<bool> m_is_running;
    std::unique_ptr<std::thread> m_thread;
    HANDLE m_handle_read_std_out = NULL;
    HANDLE m_handle_read_std_err = NULL;
    HANDLE m_handle_process = NULL;
    std::string m_label;
    ScrollingBuffer m_buffer;
public:
    AppProcess(AppConfig &app_cfg, environment_t &orig);
    ~AppProcess();
    inline const std::string &GetName() const { return m_label; }
    inline bool GetIsRunning() const { return m_is_running; }
    void ListenForChanges(); // listen for changes to the process's status
    ScrollingBuffer& GetBuffer() { return m_buffer; }
    void Terminate();
private:
    void ListenerThread();
};

}