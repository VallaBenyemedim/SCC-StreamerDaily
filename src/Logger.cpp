#include "Logger.hpp"

#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <share.h>

namespace Logger {
    namespace {
        std::mutex Mutex;
        FILE* File = nullptr;
        std::wstring LogPath;
    }

    void Initialize(HMODULE module) {
        wchar_t buffer[32768]{};
        const auto length = GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
        std::filesystem::path path(length ? std::wstring(buffer, length) : L"SCC-StreamerDaily.dll");
        path.replace_extension(L".log");
        LogPath = path.wstring();
        File = _wfsopen(LogPath.c_str(), L"wb", _SH_DENYNO);
        Write("SCC for Streamer Daily v0.2 baslatildi");
    }

    void Write(const char* format, ...) {
        std::scoped_lock lock(Mutex);
        if (!File) return;
        SYSTEMTIME time{};
        GetLocalTime(&time);
        std::fprintf(File, "[%02u:%02u:%02u.%03u] ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
        va_list arguments;
        va_start(arguments, format);
        std::vfprintf(File, format, arguments);
        va_end(arguments);
        std::fputc('\n', File);
        std::fflush(File);
    }

    std::wstring Path() { return LogPath; }

    void Shutdown() {
        std::scoped_lock lock(Mutex);
        if (File) {
            std::fclose(File);
            File = nullptr;
        }
    }
}

