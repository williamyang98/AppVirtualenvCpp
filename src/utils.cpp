#include <string>
#include <stdexcept>
#include <optional>

#include "utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#pragma comment(lib, "mincore")

namespace app::utility {

std::string wide_string_to_string(const std::wstring& wide_string)
{
    if (wide_string.empty())
    {
        return "";
    }

    const auto size_needed = WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte() failed: " + std::to_string(size_needed));
    }

    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wide_string.at(0), (int)wide_string.size(), &result.at(0), size_needed, nullptr, nullptr);
    return result;
}

// copy and paste a buffer to the clipboard
// https://stackoverflow.com/questions/1264137/how-to-copy-string-to-clipboard-in-c
void CopyToClipboard(const char *buffer, size_t length) {
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, length);
    memcpy(GlobalLock(hMem), buffer, length);
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

}
