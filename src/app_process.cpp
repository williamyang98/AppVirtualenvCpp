#include <string>
#include <sstream>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app.h"
#include "app_schema.h"
#include "environ.h"
#include "file_loading.h"
#include "utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

#pragma comment(lib, "user32.lib")

namespace app {

namespace fs = std::filesystem;

template <typename T>
void warn_and_throw(T x) {
    spdlog::warn(x);
    throw std::runtime_error(x);
}

ScrollingBuffer::ScrollingBuffer() 
{
    m_curr_size = 0;
    m_curr_write_index = 0;
    m_curr_read_index = 0;
    m_ring_buffer = (char *)(utility::CreateRingBuffer(m_max_size, (void **)(&m_ring_buffer_mirror)));

    if (m_ring_buffer == NULL) {
        throw std::runtime_error("Failed to allocate circular buffer pages for scrolling buffer");
    }
}

ScrollingBuffer::~ScrollingBuffer() {
    // virtualalloc2 circular buffer page: https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2
    // unmapviewoffile page: https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile
    // unmap the ring buffers
    UnmapViewOfFile(m_ring_buffer);
    UnmapViewOfFile(m_ring_buffer_mirror);
}

void ScrollingBuffer::IncrementIndex(const size_t size) {
    m_curr_write_index = (m_curr_write_index + size) % m_max_size;
    m_curr_size += size;

    // overhang detection
    if (m_curr_size > m_max_size) {
        m_curr_size = m_max_size;
        m_curr_read_index = m_curr_write_index;
    }

}

AppProcess::AppProcess(AppConfig &app_cfg, environment_t &orig)
{
    // create params to generate our environment data structure
    EnvParams params;
    std::string cwd_path_str; // TODO: we let the user define this manually?

    {
        // setup environment parameters
        fs::path root = fs::path(app_cfg.env_parent_dir) / app_cfg.env_name;
        fs::path exec_path = fs::path(app_cfg.exec_path);
        cwd_path_str = std::move(fs::path(exec_path).remove_filename().string());

        params.root = root.string();
        params.username = app_cfg.username;
    }

    // load the environment config
    const auto env_filepath = app_cfg.env_config_path;
    auto env_doc_res = load_document_from_filename(env_filepath.c_str());
    if (!env_doc_res) {
        throw std::runtime_error(fmt::format("Failed to retrieve default environment file ({})", env_filepath));
    }

    auto env_doc = std::move(env_doc_res.value());
    if (!validate_document(env_doc, ENV_SCHEMA)) {
        throw std::runtime_error(std::string("Failed to validate default environment schema"));
    }

    auto env_cfg = load_env_config(env_doc);
    environment_t env = create_env_from_cfg(orig, env_cfg, params);
    auto env_str = create_env_string(env);

    // initialise descriptors for process
    m_label = app_cfg.name;

    // Set the bInheritHandle flag so pipe handles are inherited. 
    SECURITY_ATTRIBUTES security_attr = {sizeof(security_attr)};
    security_attr.bInheritHandle = TRUE; 

    // setup win32 process parameters
    PROCESS_INFORMATION process_info = {0};

    STARTUPINFO startup_info = {sizeof(startup_info)};
    startup_info.dwFlags = STARTF_USESTDHANDLES;

    // TODO: free pipes if we fail somewhere along this?
    if (!CreatePipe(&startup_info.hStdInput, &startup_info.hStdInput, &security_attr, 0)) {
        warn_and_throw("Failed to create child pipe on stdin");
    }
        
    if (!CreatePipe(&m_handle_read_std_out, &startup_info.hStdOutput, &security_attr, 0)) {
        warn_and_throw("Failed to create child pipe on stdout");
    }

    if (!CreatePipe(&m_handle_read_std_err, &startup_info.hStdError, &security_attr, 0)) {
        warn_and_throw("Failed to create child pipe on stderr");
    }

    if (!SetHandleInformation(m_handle_read_std_err, HANDLE_FLAG_INHERIT, 0)) {
        warn_and_throw("Failed to set handle information on std_err_rd");
    }

    if (!SetHandleInformation(m_handle_read_std_out, HANDLE_FLAG_INHERIT, 0)) {
        warn_and_throw("Failed to set handle information on std_out_rd");
    }


    bool is_inherit_handles = TRUE;
    DWORD dw_flags = CREATE_SUSPENDED;

    auto args_str = fmt::format("\"{}\" {}", app_cfg.exec_path, app_cfg.args);

    // create the process
    bool rv = CreateProcessA(
        app_cfg.exec_path.c_str(),
        args_str.data(),
        NULL, NULL,
        is_inherit_handles, dw_flags,
        env_str.data(),
        cwd_path_str.c_str(),
        &startup_info, &process_info);
    
    if (!rv) {
        throw std::runtime_error(fmt::format("Failed to start application ({})", app_cfg.exec_path));
    }
    
    m_is_running = true;

    ResumeThread(process_info.hThread);
    CloseHandle(process_info.hThread);
    m_handle_process = process_info.hProcess;

    CloseHandle(startup_info.hStdInput);
    CloseHandle(startup_info.hStdOutput);
    CloseHandle(startup_info.hStdError);

    // startup the listener thread    
    m_thread = std::make_unique<std::thread>([this]() {
        ListenerThread();
    });
}

void AppProcess::ListenerThread() {
    BOOL bSuccess = FALSE;

    auto get_pipe_count = [](HANDLE pipe) -> DWORD {
        DWORD result;
        PeekNamedPipe(pipe, 0, 0, 0, &result, 0);
        return result;
    };

    // return true if the pipe is broken
    auto read_from_pipe = [this] (HANDLE pipe) {
        DWORD dwRead = 0;
        bool is_success = ReadFile(pipe, m_buffer.GetWriteBuffer(), m_buffer.GetMaxSize(), &dwRead, NULL);
        if((!is_success) || (dwRead == 0)) {
            return true;
        }

        // update the circular buffer to point in the right location
        m_buffer.IncrementIndex(dwRead);

        return false;
    };

    bool is_pipe_broken = false;
    DWORD total_pending;

    while (m_is_running) 
    { 
        while ((total_pending = get_pipe_count(m_handle_read_std_out)) && !is_pipe_broken) {
            is_pipe_broken = is_pipe_broken || read_from_pipe(m_handle_read_std_out); 
        }
        while ((total_pending = get_pipe_count(m_handle_read_std_err)) && !is_pipe_broken) {
            is_pipe_broken = is_pipe_broken || read_from_pipe(m_handle_read_std_err); 
        }
        Sleep(16);
        if (is_pipe_broken) {
            break;
        }
    } 
    m_is_running = false;
}

AppProcess::~AppProcess() {
    m_is_running = false;
    // TODO: cleanup the thread somehow
    m_thread->detach();
}

void AppProcess::Terminate() {
    TerminateProcess(m_handle_process, 0);
}

}