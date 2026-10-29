#include "AppState.hpp"
#include "Config.hpp"
#include "Graphics.hpp"
#include "Logger.hpp"
#include "MonoBridge.hpp"

#include <Windows.h>

#include <chrono>
#include <string>
#include <thread>

namespace {
    DWORD WINAPI Worker(void* parameter) {
        const auto module = static_cast<HMODULE>(parameter);
        Logger::Initialize(module);
        Config::Initialize(module);
        Config::Load();

        const auto monoDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!GetModuleHandleW(L"mono-2.0-bdwgc.dll") && std::chrono::steady_clock::now() < monoDeadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!MonoBridge::Initialize() || !Graphics::Install()) {
            Logger::Write("Kurulum tamamlanamadi; DLL kaldiriliyor");
            Graphics::Uninstall();
            MonoBridge::Shutdown();
            Logger::Shutdown();
            FreeLibraryAndExitThread(module, 1);
        }

        const auto eventName = L"Local\\SCCStreamerDaily.Stop." + std::to_wstring(GetCurrentProcessId());
        const auto stopEvent = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
        Logger::Write("Tam kaldirma olayi hazir: PID %lu", GetCurrentProcessId());
        while (!AppState::UnloadRequested.load()) {
            MonoBridge::Tick();
            if (stopEvent && WaitForSingleObject(stopEvent, 33) == WAIT_OBJECT_0) break;
            if (!stopEvent) std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        Logger::Write("Kaldirma baslatildi");
        Graphics::Uninstall();
        MonoBridge::Shutdown();
        Config::Save();
        if (stopEvent) CloseHandle(stopEvent);
        Logger::Write("SCC for Streamer Daily temizlendi");
        Logger::Shutdown();
        FreeLibraryAndExitThread(module, 0);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (const auto thread = CreateThread(nullptr, 0, Worker, module, 0, nullptr)) CloseHandle(thread);
    }
    return TRUE;
}

