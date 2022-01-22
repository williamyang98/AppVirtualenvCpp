#include <string>
#include <sstream>
#include <memory>
#include <unordered_map>

#include "environ.h"

namespace app {

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <processenv.h>


environment_t get_env() {
    environment_t env;

    auto free_block = [](LPTCH p) { FreeEnvironmentStrings(p); };
    auto env_block = std::unique_ptr<TCHAR, decltype(free_block)>{
            GetEnvironmentStrings(), free_block};


    for (LPTCH i = env_block.get(); *i != '\0'; ++i) {
        std::basic_stringstream<TCHAR> key_ss;
        std::basic_stringstream<TCHAR> value_ss;

        for (; *i != '='; ++i) {
            key_ss << *i;
        }
        ++i;
        for (; *i != '\0'; ++i) {
            value_ss << *i;
        }
        env[key_ss.str()] = value_ss.str();
    }

    return env;
}

tstring create_env_string(environment_t &env) {
    std::basic_stringstream<TCHAR> ss;
    for (auto &[key, value]: env) {
        ss << key << (TCHAR)('=') << value << (TCHAR)('\0');
    }
    ss << (TCHAR)('\0');
    return ss.str();
}

}