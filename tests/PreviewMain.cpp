#include "AppState.hpp"
#include "Config.hpp"
#include "Menu.hpp"

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <Windows.h>
#include <d3d11.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string_view>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using Microsoft::WRL::ComPtr;

namespace {
    ComPtr<ID3D11Device> Device;
    ComPtr<ID3D11DeviceContext> Context;
    ComPtr<IDXGISwapChain> SwapChain;
    ComPtr<ID3D11RenderTargetView> RenderTarget;

    void Check(HRESULT result, const char* message) {
        if (FAILED(result)) throw std::runtime_error(message);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) return 1;
        if (message == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_KEYMENU) return 0;
        return DefWindowProcW(window, message, wParam, lParam);
    }

    void CreateTarget() {
        ComPtr<ID3D11Texture2D> buffer;
        Check(SwapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)), "back buffer");
        Check(Device->CreateRenderTargetView(buffer.Get(), nullptr, &RenderTarget), "render target");
    }

    void SavePng(const std::filesystem::path& path) {
        std::filesystem::create_directories(path.parent_path());
        ComPtr<ID3D11Texture2D> buffer;
        Check(SwapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)), "capture buffer");
        D3D11_TEXTURE2D_DESC description{};
        buffer->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> readback;
        Check(Device->CreateTexture2D(&description, nullptr, &readback), "readback texture");
        Context->CopyResource(readback.Get(), buffer.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(Context->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped), "readback map");

        ComPtr<IWICImagingFactory> factory;
        Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "wic factory");
        ComPtr<IWICStream> stream;
        Check(factory->CreateStream(&stream), "wic stream");
        Check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "png path");
        ComPtr<IWICBitmapEncoder> encoder;
        Check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "png encoder");
        Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "png init");
        ComPtr<IWICBitmapFrameEncode> frame;
        Check(encoder->CreateNewFrame(&frame, nullptr), "png frame");
        Check(frame->Initialize(nullptr), "frame init");
        Check(frame->SetSize(description.Width, description.Height), "frame size");
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        Check(frame->SetPixelFormat(&format), "frame format");
        ComPtr<IWICBitmap> bitmap;
        Check(factory->CreateBitmapFromMemory(description.Width, description.Height, GUID_WICPixelFormat32bppRGBA,
            mapped.RowPitch, mapped.RowPitch * description.Height, static_cast<BYTE*>(mapped.pData), &bitmap), "wic bitmap");
        ComPtr<IWICFormatConverter> converter;
        Check(factory->CreateFormatConverter(&converter), "wic converter");
        Check(converter->Initialize(bitmap.Get(), format, WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom), "wic conversion");
        Check(frame->WriteSource(converter.Get(), nullptr), "png pixels");
        Check(frame->Commit(), "frame commit");
        Check(encoder->Commit(), "encoder commit");
        Context->Unmap(readback.Get(), 0);
    }

    void SeedPreview(int page) {
        AppState::Options = {};
        AppState::Options.StartPage = std::clamp(page, 0, 5);
        AppState::Options.LockHealth = true;
        AppState::Options.HealthTarget = 1.0f;
        AppState::Options.LockEnergy = true;
        AppState::Options.SpeedEnabled = true;
        AppState::Options.SpeedMultiplier = 2.25f;
        AppState::Options.StreamBoost = true;
        AppState::Options.MinimumExtraViewers = 2400;
        AppState::Live = {};
        AppState::Live.MonoReady = true;
        AppState::Live.PlayerReady = true;
        AppState::Live.Money = 126450.0f;
        AppState::Live.Health = 0.84f;
        AppState::Live.Energy = 0.72f;
        AppState::Live.Need = 0.61f;
        AppState::Live.Happiness = 0.93f;
        AppState::Live.WalkSpeed = 6.75f;
        AppState::Live.Followers = 18420;
        AppState::Live.Subscribers = 1260;
        AppState::Live.SkillPoints = 37;
        AppState::Live.Experience = 8420;
        AppState::Live.Level = 12;
        AppState::Live.ExtraViewers = 2840;
        AppState::Live.Status = "Oyuncu verisi canli";
        AppState::MenuOpen.store(true);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HWND window = nullptr;
    try {
        Check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "com init");
        int argumentCount = 0;
        auto** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
        std::filesystem::path output = L"artifacts\\menu-preview.png";
        int page = 0;
        for (int index = 1; index < argumentCount; ++index) {
            if (std::wstring_view(arguments[index]) == L"--output" && index + 1 < argumentCount) output = arguments[++index];
            else if (std::wstring_view(arguments[index]) == L"--page" && index + 1 < argumentCount) page = _wtoi(arguments[++index]);
        }
        LocalFree(arguments);
        output = std::filesystem::absolute(output);
        SeedPreview(page);
        Config::Initialize(GetModuleHandleW(nullptr));

        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW windowClass{sizeof(windowClass)};
        windowClass.style = CS_CLASSDC;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = L"SCC.StreamerDaily.MenuPreview";
        if (!RegisterClassExW(&windowClass)) throw std::runtime_error("window class");
        window = CreateWindowExW(0, windowClass.lpszClassName, L"SCC — Streamer Daily Menu Preview", WS_OVERLAPPEDWINDOW,
            0, 0, 1280, 800, nullptr, nullptr, instance, nullptr);
        if (!window) throw std::runtime_error("window");

        DXGI_SWAP_CHAIN_DESC swapDescription{};
        swapDescription.BufferCount = 2;
        swapDescription.BufferDesc.Width = 1280;
        swapDescription.BufferDesc.Height = 800;
        swapDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDescription.OutputWindow = window;
        swapDescription.SampleDesc.Count = 1;
        swapDescription.Windowed = TRUE;
        swapDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &swapDescription, &SwapChain, &Device, nullptr, &Context);
        if (FAILED(result)) result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &swapDescription, &SwapChain, &Device, nullptr, &Context);
        Check(result, "directx");
        CreateTarget();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        Menu::Initialize();
        if (!ImGui_ImplWin32_Init(window)) throw std::runtime_error("imgui win32");
        if (!ImGui_ImplDX11_Init(Device.Get(), Context.Get())) throw std::runtime_error("imgui dx11");

        for (int frame = 0; frame < 24; ++frame) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
            ImGui::NewFrame();
            Menu::Draw();
            ImGui::Render();
            const float clear[4]{0.010f, 0.012f, 0.018f, 1.0f};
            auto* target = RenderTarget.Get();
            Context->OMSetRenderTargets(1, &target, nullptr);
            Context->ClearRenderTargetView(target, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            Check(SwapChain->Present(0, 0), "present");
        }
        SavePng(output);

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        RenderTarget.Reset(); SwapChain.Reset(); Context.Reset(); Device.Reset();
        DestroyWindow(window);
        UnregisterClassW(windowClass.lpszClassName, instance);
        CoUninitialize();
        return 0;
    } catch (...) {
        if (window) DestroyWindow(window);
        return 1;
    }
}
