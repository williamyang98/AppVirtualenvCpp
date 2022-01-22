#include <string>
#include <sstream>
#include <filesystem>
#include <ranges>

#include <fmt/core.h>
#include <spdlog/spdlog.h>

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

extern const char *DEFAULT_ENV_FILEPATH = "./res/default_env.json";
extern const char *DEFAULT_APP_FILEPATH = "./res/default_app.json";
extern const char *DEFAULT_APPS_FILEPATH = "./res/apps.json";

environment_t create_env_from_cfg(environment_t &orig, EnvConfig &cfg, EnvParams &params) {
    environment_t env;

    auto fill_params = [&params](std::string &v) {
        return fmt::format(v, 
            fmt::arg("root", params.root),
            fmt::arg("username", params.username));
    };

    auto create_directory = [](const std::string &s_in) {
        try {
            fs::create_directories(fs::path(s_in));
        } catch (std::exception &ex) {
            spdlog::warn(fmt::format("Failed to create directory ({}): ({})", s_in, ex.what()));
        }
    };

    // directories
    for (auto &[k,v]: cfg.env_directories) {
        auto dir = fill_params(v);
        // pass absolute directory to environment
        env.insert({k, fs::absolute(dir).string() });
        create_directory(dir);
    }

    for (auto &v: cfg.seed_directories) {
        auto dir = fill_params(v);
        create_directory(dir);
    }

    // variables
    for (auto &[k,v]: cfg.override_variables) {
        env.insert({k, fill_params(v)});
    }

    for (auto &k: cfg.pass_through_variables) {
        if (!orig.contains(k)) {
            continue;
        }
        auto &v = orig.at(k);
        env.insert({k, fill_params(v)});
    }

    return env;
}

App::App() {
    m_is_config_list_dirtied = false;
    m_parent_env = get_env();

    // load default app config
    auto app_doc_res = load_document_from_filename(DEFAULT_APP_FILEPATH);
    if (!app_doc_res) {
        m_runtime_errors.push_back(fmt::format("Failed to retrieve default app configuration file ({})", DEFAULT_APP_FILEPATH));
        return;
    }

    auto app_doc = std::move(app_doc_res.value());
    if (!validate_document(app_doc, DEFAULT_APP_SCHEMA)) {
        m_runtime_errors.push_back(std::string("Failed to validate default app config schema"));
        return;
    }

    auto default_app_cfg = load_app_config(app_doc);
    m_default_app_config = ManagedConfig(default_app_cfg);
}

App::App(const std::string &app_filepath)
: App() 
{
    open_app_config(app_filepath);
}


bool App::open_app_config(const std::string &app_filepath) {
    auto apps_doc_res = load_document_from_filename(app_filepath.c_str());
    if (!apps_doc_res) {
        m_runtime_warnings.push_back(fmt::format("Failed to read apps file ({})", app_filepath));
        return false;
    }

    auto apps_doc = std::move(apps_doc_res.value());
    if (!validate_document(apps_doc, APPS_SCHEMA)) {
        m_runtime_warnings.push_back(std::string("Failed to validate apps schema"));
        return false;
    }

    auto cfgs = load_app_configs(apps_doc);
    m_configs.clear();
    m_app_filepath = app_filepath;
    for (auto &cfg: cfgs) {
        auto managed_config = std::make_shared<ManagedConfig>(cfg);
        m_configs.push_back(managed_config);
    }

    m_undirtied_configs = m_configs;
    m_is_config_list_dirtied = false;

    return true;
}

void App::launch_app(AppConfig &app) {
    try {
        auto process_ptr = std::make_unique<AppProcess>(app, m_parent_env);
        m_processes.push_back(std::move(process_ptr));
    } catch (std::exception &ex) {
        m_runtime_warnings.push_back(ex.what());
    }
}

void App::save_configs() {
    // nothing to save
    if (m_configs.size() == 0) {
        return;
    }

    auto cfgs = m_undirtied_configs | std::views::transform([](std::shared_ptr<ManagedConfig> &cfg) {
        return std::reference_wrapper(cfg->GetUndirtiedConfig());
    });
    auto doc = create_app_configs_doc(cfgs);
    if (!write_document_to_file(m_app_filepath.c_str(), doc)) {
        m_runtime_warnings.push_back(fmt::format("Failed to save configs to {}", m_app_filepath));
    }
}

bool App::RevertConfigListChanges() {
    if (!m_is_config_list_dirtied) {
        return false;
    }
    m_configs = m_undirtied_configs;
    m_is_config_list_dirtied = false;
    return true;
}

bool App::ApplyConfigListChanges() {
    if (!m_is_config_list_dirtied) {
        return false;
    }
    m_undirtied_configs = m_configs;
    m_is_config_list_dirtied = false;
    return true;
}

config_list_t::iterator App::RemoveConfigFromList(config_list_t::iterator &it) {
    auto new_it = m_configs.erase(it);
    m_is_config_list_dirtied = true;
    return new_it;
}

void App::AddConfigToList(AppConfig &cfg) {
    m_configs.push_back(std::make_shared<ManagedConfig>(cfg));
    m_is_config_list_dirtied = true;
}

ManagedConfig::ManagedConfig() {
    m_is_dirty = false;
}

ManagedConfig::ManagedConfig(AppConfig &cfg) {
    m_cfg = cfg;
    m_undirtied_cfg = cfg;
    m_is_dirty = false;
}

bool ManagedConfig::RevertChanges() {
    if (!IsDirty()) {
        return false;
    }

    m_cfg = m_undirtied_cfg;
    m_is_dirty = false;
    return true;
}

bool ManagedConfig::ApplyChanges() {
    if (!IsDirty()) {
        return false;
    }

    m_undirtied_cfg = m_cfg;
    m_is_dirty = false;
    return true;
}

}