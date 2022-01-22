#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <processenv.h>

typedef std::basic_string<TCHAR> tstring; // Generally convenient
typedef std::unordered_map<tstring, tstring> environment_t;

environment_t get_env() {
    environment_t env;

    auto free_block = [](LPTCH p) { FreeEnvironmentStrings(p); };
    auto env_block = std::unique_ptr<TCHAR, decltype(free_block)>{
            GetEnvironmentStrings(), free_block};

    std::basic_stringstream<TCHAR> key_ss;
    std::basic_stringstream<TCHAR> value_ss;

    for (LPTCH i = env_block.get(); *i != '\0'; ++i) {
        for (; *i != '='; ++i) {
            key_ss << *i;
        }
        ++i;
        for (; *i != '\0'; ++i) {
            value_ss << *i;
        }
        env[key_ss.str()] = value_ss.str();
        key_ss.clear();
        key_ss.str("");
        value_ss.clear();
        value_ss.str("");
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

int main(int argc, char **argv) {
    //auto env = get_env();
    //for (auto &[key, value]: env) {
    //    std::cout << key << "=" << value << std::endl;
    //}

    environment_t custom_env;
    custom_env.insert({"OS", "My custom os"});
    custom_env.insert({"Custom Key", "Custom Value"});

    PROCESS_INFORMATION process_info;
    STARTUPINFO startup_info;
    SecureZeroMemory(&startup_info, sizeof(STARTUPINFO));
    startup_info.cb = sizeof(STARTUPINFO);
    
    std::basic_string<TCHAR> program_name = "./env_print.exe";
    std::basic_string<TCHAR> custom_env_str = create_env_string(custom_env);
    DWORD dwFlags = 0;

    CreateProcess(
        program_name.data(), 
        NULL, NULL, NULL, 
        true, dwFlags, 
        custom_env_str.data(),     // inherit parent's environment 
        NULL, 
        &startup_info, &process_info);

    WaitForSingleObject(process_info.hProcess, INFINITE);
    return 0;
}
