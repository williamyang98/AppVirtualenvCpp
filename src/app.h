#pragma once

#include <string>
#include <vector>
#include <list>
#include <memory>

#include "app_schema.h"
#include "app_process.h"
#include "managed_config.h"
#include "environ.h"

namespace app {

extern const char *DEFAULT_ENV_FILEPATH;
extern const char *DEFAULT_APP_FILEPATH;
extern const char *DEFAULT_APPS_FILEPATH;

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
