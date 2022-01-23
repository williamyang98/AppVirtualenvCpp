#pragma once

#include <string>
#include <vector>
#include <list>
#include <memory>

#include "app_schema.h"
#include "app_process.h"
#include "environ.h"

namespace app {

extern const char *DEFAULT_ENV_FILEPATH;
extern const char *DEFAULT_APP_FILEPATH;
extern const char *DEFAULT_APPS_FILEPATH;

class ManagedConfigList;

class ManagedConfig 
{
public:
    enum Status {
        NONE      = 0,
        CHANGED   = 1<<0,
        UNTRACKED = 1<<1,
    };
private:
    Status m_status;
    bool m_is_pending_delete;
    AppConfig m_cfg;
    AppConfig m_unchanged_cfg;
    ManagedConfigList *m_parent;
public:
    ManagedConfig();
    ManagedConfig(AppConfig &cfg);
    ManagedConfig(AppConfig &cfg, ManagedConfigList *parent);
    inline AppConfig &GetConfig() { return m_cfg; }
    inline AppConfig &GetUnchangedConfig() { return m_unchanged_cfg; }
    inline bool IsPendingDelete() { return m_is_pending_delete; }
    void SetIsPendingDelete(bool is_delete);
    inline Status GetStatus() const { return m_status; }
    void SetStatus(Status status);
    bool RevertChanges();
    bool ApplyChanges();
};

typedef std::list<std::shared_ptr<ManagedConfig>> config_list_t;

class ManagedConfigList
{
private:
    config_list_t m_configs;
    bool m_is_pending_save;
public:
    ManagedConfigList();
    inline config_list_t &GetConfigs() { return m_configs; }
    inline bool IsPendingSave() const { return m_is_pending_save; }
    bool IsDirty() const;
    bool RevertChanges();
    bool ApplyChanges();
    void Clear();
    void Add(AppConfig &cfg);
    void CommitSave();

    // remove move and copy constructors since this breaks the reference a 
    // child managed config would have to the managed config list
    ManagedConfigList(ManagedConfigList &) = delete;
    ManagedConfigList(ManagedConfigList &&) = delete;
    ManagedConfigList& operator=(const ManagedConfigList &) = delete;
    ManagedConfigList& operator=(ManagedConfigList &&) = delete;
private:
    friend class ManagedConfig;
    inline void SetPendingSave() { m_is_pending_save = true; }
};

class App 
{
public:
    std::string m_app_filepath;
    std::list<std::string> m_runtime_errors;
    std::list<std::string> m_runtime_warnings;
    std::vector<std::unique_ptr<AppProcess>> m_processes;
    ManagedConfigList m_managed_configs;
private:
    environment_t m_parent_env;
    // single instance that we preload with default for our app factory
    ManagedConfig m_default_app_config;
public:
    App();
    App(const std::string &app_filepath);
    inline auto &GetCreatorConfig() { return m_default_app_config; }
    bool open_app_config(const std::string &app_filepath);
    void launch_app(AppConfig &app);
    void save_configs();
};

}