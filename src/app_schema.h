#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

namespace app {

extern rapidjson::SchemaDocument ENV_SCHEMA;
extern rapidjson::SchemaDocument APPS_SCHEMA;
extern rapidjson::SchemaDocument DEFAULT_APP_SCHEMA;

struct EnvConfig {
    std::unordered_map<std::string, std::string> env_directories;
    std::vector<std::string>                     seed_directories;
    std::unordered_map<std::string, std::string> override_variables;
    std::vector<std::string>                     pass_through_variables;
};

struct AppConfig {
    std::string name;
    std::string username;
    std::string exec_path;
    std::string exec_cwd;
    std::string args;
    std::string env_name;
    std::string env_config_path;
    std::string env_parent_dir;
};

EnvConfig load_env_config(rapidjson::Document &doc);
AppConfig load_app_config(rapidjson::Document &doc);
std::vector<AppConfig> load_app_configs(rapidjson::Document &doc);


rapidjson::Document create_env_config_doc(EnvConfig &cfg);

// define here so we get template initialisation
template <typename T>
rapidjson::Document create_app_configs_doc(T &configs) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("apps");
    writer.StartArray();
    for (AppConfig &cfg: configs) {
        writer.StartObject();
        writer.Key("name"); 
        writer.String(cfg.name.c_str());

        writer.Key("username"); 
        writer.String(cfg.username.c_str());

        writer.Key("exec_path"); 
        writer.String(cfg.exec_path.c_str());

        writer.Key("exec_cwd"); 
        writer.String(cfg.exec_cwd.c_str());
        
        writer.Key("args"); 
        writer.String(cfg.args.c_str());

        writer.Key("env_name"); 
        writer.String(cfg.env_name.c_str());

        writer.Key("env_config_path"); 
        writer.String(cfg.env_config_path.c_str());

        writer.Key("env_parent_dir"); 
        writer.String(cfg.env_parent_dir.c_str());

        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();

    rapidjson::Document doc;
    doc.Parse(sb.GetString());
    return doc;
}

bool validate_document(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc);

}