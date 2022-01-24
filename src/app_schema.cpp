#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "app_schema.h"

namespace app {

rapidjson::SchemaDocument load_schema_from_cstr(const char *cstr) {
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(cstr);
    if (!ok) {
        spdlog::critical(fmt::format("JSON parse error: {} ({})\n", 
            rapidjson::GetParseError_En(ok.Code()), ok.Offset()));
#ifndef NDEBUG
        assert(!ok.IsError());
#else
        exit(EXIT_FAILURE);
#endif
    }
    auto schema = rapidjson::SchemaDocument(doc);
    return schema;
}

const char *ENV_SCHEMA_STR = 
R"({
    "title": "Environment file",
    "description": "Environment file",
    "type": "object",
    "properties": {
        "directories": {
            "type": "object"
        },
        "seed_directories" : {
            "type": "array",
            "items": {
                "type": "string"
            }
        }, 
        "override_variables": {
            "type": "object"
        },
        "pass_through_variables": {
            "type": "array",
            "items": {
                "type": "string"
            }
        }
    },
    "required": ["directories"]
})";

extern rapidjson::SchemaDocument ENV_SCHEMA = load_schema_from_cstr(ENV_SCHEMA_STR);

const char *APPS_SCHEMA_STR = 
R"({
    "title": "App file",
    "description": "App file",
    "type": "object",
    "properties": {
        "apps" : {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": { "type": "string" },
                    "username": { "type": "string" },
                    "exec_path": { "type": "string" },
                    "exec_cwd": { "type": "string" },
                    "args": { "type": "string" },
                    "env_name": { "type": "string" },
                    "env_config_path": { "type": "string" },
                    "env_parent_dir": { "type": "string" }
                },
                "required": [
                    "name", "username", "exec_path", "args", 
                    "env_name",
                    "env_config_path", "env_parent_dir"
                ]
            }
        }
    },
    "required": ["apps"]
})";


extern rapidjson::SchemaDocument APPS_SCHEMA = load_schema_from_cstr(APPS_SCHEMA_STR);

const char *DEFAULT_APP_SCHEMA_STR = 
R"({
    "title": "Default app file",
    "description": "Default app file",
    "type": "object",
    "properties": {
        "name": { "type": "string" },
        "username": { "type": "string" },
        "exec_path": { "type": "string" },
        "exec_cwd": { "type": "string" },
        "args": { "type": "string" },
        "env_name": { "type": "string" },
        "env_config_path": { "type": "string" },
        "env_parent_dir": { "type": "string" }
    }
})";

extern rapidjson::SchemaDocument DEFAULT_APP_SCHEMA = load_schema_from_cstr(DEFAULT_APP_SCHEMA_STR);

EnvConfig load_env_config(rapidjson::Document &doc) {
    EnvConfig cfg;

    if (doc.HasMember("directories")) {
        auto dirs = doc["directories"].GetObject();
        for (auto &[k,v]: dirs) {
            cfg.env_directories.insert({k.GetString(), v.GetString()});
        }
    }

    if (doc.HasMember("seed_directories")) {
        auto dirs = doc["seed_directories"].GetArray();
        for (auto &v: dirs) {
            cfg.seed_directories.push_back(v.GetString());
        }
    }

    if (doc.HasMember("override_variables")) {
        auto dirs = doc["override_variables"].GetObject();
        for (auto &[k,v]: dirs) {
            cfg.override_variables.insert({k.GetString(), v.GetString()});
        }
    }

    if (doc.HasMember("pass_through_variables")) {
        auto dirs = doc["pass_through_variables"].GetArray();
        for (auto &v: dirs) {
            cfg.pass_through_variables.push_back(v.GetString());
        }
    }

    return cfg;
}

AppConfig load_app_config(rapidjson::Document &doc) {
    auto load_default = [&doc](const char *key) {
        return doc.HasMember(key) ? doc[key].GetString() : "";
    };

    AppConfig cfg;
    cfg.name            = load_default("name");
    cfg.username        = load_default("username");
    cfg.exec_path       = load_default("exec_path");
    cfg.args            = load_default("args");
    cfg.env_name        = load_default("env_name");
    cfg.env_config_path = load_default("env_config_path");
    cfg.env_parent_dir  = load_default("env_parent_dir");
    return cfg;
}

std::vector<AppConfig> load_app_configs(rapidjson::Document &doc) {
    std::vector<AppConfig> cfgs;
    auto apps = doc["apps"].GetArray();
    auto load_default = [](rapidjson::Value &app, const char *key) {
        return app.HasMember(key) ? app[key].GetString() : "";
    };

    for (auto &app: apps) {
        AppConfig cfg;
        cfg.name            = app["name"].GetString();
        cfg.username        = app["username"].GetString();
        cfg.exec_path       = app["exec_path"].GetString();
        cfg.exec_cwd        = load_default(app, "exec_cwd");
        cfg.args            = app["args"].GetString();
        cfg.env_name        = app["env_name"].GetString();
        cfg.env_config_path = app["env_config_path"].GetString();
        cfg.env_parent_dir  = app["env_parent_dir"].GetString();

        cfgs.push_back(std::move(cfg));
    }
    return cfgs;
}

rapidjson::Document create_env_config_doc(EnvConfig &cfg) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("directories");
    writer.StartObject();
    for (auto &[k,v]: cfg.env_directories) {
        writer.Key(k.c_str());
        writer.String(v.c_str());
    }
    writer.EndObject();

    writer.Key("seed_directories");
    writer.StartArray();
    for (auto &v: cfg.seed_directories) {
        writer.String(v.c_str());
    }
    writer.EndArray();

    writer.Key("override_variables");
    writer.StartObject();
    for (auto &[k,v]: cfg.override_variables) {
        writer.Key(k.c_str());
        writer.String(v.c_str());
    }
    writer.EndObject();

    writer.Key("pass_through_variables");
    writer.StartArray();
    for (auto &v: cfg.pass_through_variables) {
        writer.String(v.c_str());
    }
    writer.EndArray();

    writer.EndObject(); 

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(sb.GetString());
    assert(!ok.IsError());
    return doc;

}

bool validate_document(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc) {
    rapidjson::SchemaValidator validator(schema_doc);
    if (!doc.Accept(validator)) {
        spdlog::error("Doc doesn't match schema");

        rapidjson::StringBuffer sb;
        validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
        spdlog::error(fmt::format("document pointer: {}", sb.GetString()));
        spdlog::error(fmt::format("error-type: {}", validator.GetInvalidSchemaKeyword()));
        sb.Clear();

        validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
        spdlog::error(fmt::format("schema pointer: {}", sb.GetString()));
        sb.Clear();

        return false;
    }

    return true;
}

}