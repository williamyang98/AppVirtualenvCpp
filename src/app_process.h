#pragma once

#include <atomic>
#include <thread>
#include <memory>

#include "environ.h"
#include "app_schema.h"
#include "scrolling_buffer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

namespace app {

// creates a process with the specified environment and app configuration
// attaches a thread with a scrolling buffer to read from it
class AppProcess 
{
public:
    enum State { RUNNING, TERMINATING, TERMINATED };
private:
    std::atomic<State> m_state;
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
    inline State GetState() const { return m_state; }
    void ListenForChanges(); // listen for changes to the process's status
    ScrollingBuffer& GetBuffer() { return m_buffer; }
    void Terminate();
private:
    void ListenerThread();
};

}
