#pragma once

#include <Windows.h>
#include <string>

namespace Config {
    void Initialize(HMODULE module);
    bool Load(std::string* error = nullptr);
    bool Save(std::string* error = nullptr);
    std::wstring Path();
}

