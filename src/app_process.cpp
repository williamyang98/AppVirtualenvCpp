#include <string>
#include <sstream>
#include <filesystem>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app.h"
#include "app_schema.h"
#include "environ.h"
#include "file_loading.h"

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

ScrollingBuffer::ScrollingBuffer(int max_size) 
: m_max_size(max_size)
{
    m_buffer = new char[max_size+1];
    m_buffer[max_size] = '\0';
    m_curr_size = 0;
    m_curr_write_index = 0;
}

ScrollingBuffer::~ScrollingBuffer() {
    delete[] m_buffer;
}

void ScrollingBuffer::WriteBytes(const char *rd_buf, const int size) {
    auto &j = m_curr_write_index;
    for (int i = 0; i < size; i++) {
        m_buffer[j] = rd_buf[i];
        j = (j+1) % m_max_size;
    }
    m_curr_size += size;
    if (m_curr_size > m_max_size) {
        m_curr_size = m_max_size;
    }
}

AppProcess::AppProcess(AppConfig &app_cfg, environment_t &orig)
: m_buffer(10000)
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
    
    m_is_running = rv;

    if (rv) {
        ResumeThread(process_info.hThread);
        CloseHandle(process_info.hThread);
    }

    CloseHandle(startup_info.hStdInput);
    CloseHandle(startup_info.hStdOutput);
    CloseHandle(startup_info.hStdError);

    // startup the listener thread    
    m_thread = std::make_unique<std::thread>([this]() {
        ListenerThread();
    });
}

void AppProcess::ListenerThread() {
    DWORD dwRead; 
    constexpr int BUFSIZE = 128;
    CHAR chBuf[BUFSIZE]; 
    BOOL bSuccess = FALSE;

    auto get_pipe_count = [](HANDLE pipe) -> DWORD {
        DWORD result;
        PeekNamedPipe(pipe, 0, 0, 0, &result, 0);
        return result;
    };

    // return true if the pipe is broken
    auto read_from_pipe = [this, &chBuf, &BUFSIZE, &dwRead](HANDLE pipe) {
        bool is_success = ReadFile(pipe, chBuf, BUFSIZE, &dwRead, NULL);
        if((!is_success) || (dwRead == 0)) {
            return true;
        }

        // thread safe write to buffer
        // TODO: implement a scrolling fixed sized buffer
        // learn how to use virtualmemory functions to create a memory mapped circular buffer
        {
            auto lock = std::scoped_lock(m_buffer_mutex);
            m_buffer.WriteBytes(chBuf, dwRead);
        }

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
        Sleep(33);
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

}