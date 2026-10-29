#pragma once

#include <Windows.h>
#include <string>

namespace Logger {
    void Initialize(HMODULE module);
    void Write(const char* format, ...);
    std::wstring Path();
    void Shutdown();
}

