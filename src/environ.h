#pragma once
#include <string>
#include <unordered_map>

namespace app {

#define WIN32_LEAN_AND_MEAN
#include <tchar.h>

typedef std::basic_string<TCHAR> tstring; // Generally convenient
typedef std::unordered_map<tstring, tstring> environment_t;

environment_t get_env();
tstring create_env_string(environment_t &env);

}