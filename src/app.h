#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <optional>
#include <memory>

#include <mutex>
#include <atomic>
#include <thread>

#include "environ.h"
#include "app_schema.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>

namespace app {

extern const char *DEFAULT_ENV_FILEPATH;
extern const char *DEFAULT_APP_FILEPATH;
extern const char *DEFAULT_APPS_FILEPATH;

struct EnvParams {
    std::string root;
    std::string username;
};

environment_t create_env_from_cfg(environment_t &orig, EnvConfig &cfg, EnvParams &params);

class ManagedConfig 
{
public:
    ManagedConfig();
    ManagedConfig(AppConfig &cfg);
    inline AppConfig &GetConfig() { return m_cfg; }
    inline AppConfig &GetUndirtiedConfig() { return m_undirtied_cfg; }
    inline void SetDirtyFlag() { m_is_dirty = true; }
    inline bool IsDirty() const { return m_is_dirty; }
    bool RevertChanges();
    bool ApplyChanges();
private:
    AppConfig m_cfg;
    AppConfig m_undirtied_cfg;
    bool m_is_dirty;
};

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

class AppProcess 
{
private:
    std::atomic<bool> m_is_running;
    std::unique_ptr<std::thread> m_thread;
    HANDLE m_handle_read_std_out = NULL;
    HANDLE m_handle_read_std_err = NULL;

    std::string m_label;

    ScrollingBuffer m_buffer;
public:
    AppProcess(AppConfig &app_cfg, environment_t &orig);
    ~AppProcess();
    inline const std::string &GetName() const { return m_label; }
    inline bool GetIsRunning() const { return m_is_running; }
    void ListenForChanges(); // listen for changes to the process's status
    ScrollingBuffer& GetBuffer() { return m_buffer; }
private:
    void ListenerThread();
};

typedef std::list<std::shared_ptr<ManagedConfig>> config_list_t;

class App 
{
public:
    std::string m_app_filepath;

    std::list<std::string> m_runtime_errors;
    std::list<std::string> m_runtime_warnings;
    std::vector<std::unique_ptr<AppProcess>> m_processes;
private:
    environment_t m_parent_env;
    // for handling the list view of configs when we add/remove items
    config_list_t m_configs;
    config_list_t m_undirtied_configs;
    bool m_is_config_list_dirtied;

    // single instance that we preload with default for our app factory
    ManagedConfig m_default_app_config;
public:
    App();
    App(const std::string &app_filepath);
    inline auto &GetConfigs() { return m_configs; }
    inline bool GetIsConfligListDirtied() { return m_is_config_list_dirtied; }
    inline auto &GetCreatorConfig() { return m_default_app_config; }
    bool RevertConfigListChanges();
    bool ApplyConfigListChanges();
    config_list_t::iterator RemoveConfigFromList(config_list_t::iterator &it);
    void AddConfigToList(AppConfig &cfg);

    bool open_app_config(const std::string &app_filepath);
    void launch_app(AppConfig &app);
    void save_configs();
};

}