#include "Graphics.hpp"

#include "AppState.hpp"
#include "Logger.hpp"
#include "Menu.hpp"
#include "MonoBridge.hpp"

#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
using Microsoft::WRL::ComPtr;

namespace Graphics {
    namespace {
        using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
        using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

        HWND GameWindow = nullptr;
        WNDPROC OriginalWindowProc = nullptr;
        PresentFn OriginalPresent = nullptr;
        ResizeFn OriginalResize = nullptr;
        std::vector<void*> HookTargets;
        std::atomic<bool> Stopping{false};
        std::atomic<bool> Installed{false};
        std::atomic<int> ActiveCalls{0};
        std::atomic<unsigned long long> FrameCount{0};
        std::mutex RenderMutex;

        struct RendererState {
            ImGuiContext* Context = nullptr;
            ComPtr<IDXGISwapChain> SwapChain;
            ComPtr<ID3D11Device> Device;
            ComPtr<ID3D11DeviceContext> DeviceContext;
            ComPtr<ID3D11RenderTargetView> RenderTarget;
            bool Win32Ready = false;
            bool Dx11Ready = false;
        } Renderer;

        struct CallScope {
            CallScope() { ActiveCalls.fetch_add(1); }
            ~CallScope() { ActiveCalls.fetch_sub(1); }
        };

        BOOL CALLBACK FindWindowCallback(HWND window, LPARAM data) {
            DWORD processId = 0;
            GetWindowThreadProcessId(window, &processId);
            if (processId != GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
            wchar_t title[256]{};
            GetWindowTextW(window, title, static_cast<int>(std::size(title)));
            if (!title[0]) return TRUE;
            *reinterpret_cast<HWND*>(data) = window;
            return FALSE;
        }

        bool FindGameWindow() {
            GameWindow = nullptr;
            EnumWindows(FindWindowCallback, reinterpret_cast<LPARAM>(&GameWindow));
            return GameWindow != nullptr;
        }

        bool IsTarget(IDXGISwapChain* swapChain) {
            DXGI_SWAP_CHAIN_DESC description{};
            return swapChain && SUCCEEDED(swapChain->GetDesc(&description)) && description.OutputWindow == GameWindow;
        }

        bool IsInputMessage(UINT message) {
            return (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
                (message >= WM_KEYFIRST && message <= WM_KEYLAST) || message == WM_INPUT;
        }

        void InvalidateDx11() {
            if (Renderer.Context) {
                ImGui::SetCurrentContext(Renderer.Context);
                if (Renderer.Dx11Ready) ImGui_ImplDX11_Shutdown();
            }
            Renderer.Dx11Ready = false;
            Renderer.RenderTarget.Reset();
            Renderer.DeviceContext.Reset();
            Renderer.Device.Reset();
            Renderer.SwapChain.Reset();
        }

        LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            CallScope scope;
            if (!Stopping.load() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wParam == VK_INSERT) {
                if ((lParam & (1LL << 30)) == 0) {
                    const auto open = !AppState::MenuOpen.load();
                    AppState::MenuOpen.store(open);
                    if (Renderer.Context) {
                        ImGui::SetCurrentContext(Renderer.Context);
                        ImGui::GetIO().ClearInputKeys();
                        ImGui::GetIO().ClearInputMouse();
                    }
                    Logger::Write("Insert: menu %s", open ? "acik" : "kapali");
                }
                return 0;
            }
            if ((message == WM_KEYUP || message == WM_SYSKEYUP) && wParam == VK_INSERT) return 0;

            if (!Stopping.load() && AppState::MenuOpen.load() && Renderer.Context) {
                ImGui::SetCurrentContext(Renderer.Context);
                ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
                if (message == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT) {
                    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                    return TRUE;
                }
                if (message == WM_INPUT) return DefWindowProcW(window, message, wParam, lParam);
                if (IsInputMessage(message)) return 1;
            }
            return OriginalWindowProc ? CallWindowProcW(OriginalWindowProc, window, message, wParam, lParam)
                                      : DefWindowProcW(window, message, wParam, lParam);
        }

        void DestroyRenderer() {
            InvalidateDx11();
            if (OriginalWindowProc && GameWindow && IsWindow(GameWindow)) {
                SetWindowLongPtrW(GameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(OriginalWindowProc));
            }
            OriginalWindowProc = nullptr;
            if (Renderer.Context) {
                ImGui::SetCurrentContext(Renderer.Context);
                if (Renderer.Win32Ready) ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext(Renderer.Context);
            }
            Renderer = {};
        }

        bool CreateRenderer(IDXGISwapChain* swapChain) {
            Logger::Write("Renderer olusturuluyor");
            if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&Renderer.Device)))) return false;
            Logger::Write("Renderer: D3D11 device alindi");
            Renderer.Device->GetImmediateContext(&Renderer.DeviceContext);
            Logger::Write("Renderer: immediate context alindi");
            ComPtr<ID3D11Texture2D> backBuffer;
            if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) return false;
            Logger::Write("Renderer: backbuffer alindi");
            if (FAILED(Renderer.Device->CreateRenderTargetView(backBuffer.Get(), nullptr, &Renderer.RenderTarget))) return false;
            Logger::Write("Renderer: RTV olusturuldu");
            Renderer.SwapChain = swapChain;

            if (!Renderer.Context) return false;
            ImGui::SetCurrentContext(Renderer.Context);
            if (!Renderer.Win32Ready) {
                Renderer.Win32Ready = ImGui_ImplWin32_Init(GameWindow);
                Logger::Write("Renderer: Win32 backend sonucu %d", Renderer.Win32Ready ? 1 : 0);
            }
            Renderer.Dx11Ready = ImGui_ImplDX11_Init(Renderer.Device.Get(), Renderer.DeviceContext.Get());
            Logger::Write("Renderer: DX11 backend sonucu %d", Renderer.Dx11Ready ? 1 : 0);
            if (!Renderer.Win32Ready || !Renderer.Dx11Ready) {
                InvalidateDx11();
                return false;
            }
            if (!OriginalWindowProc) {
                SetLastError(0);
                OriginalWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(GameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
                Logger::Write("Renderer: WndProc sonucu %p hata %lu", OriginalWindowProc, GetLastError());
                if (!OriginalWindowProc && GetLastError() != 0) {
                    DestroyRenderer();
                    return false;
                }
            }
            Logger::Write("Dear ImGui DX11 hazir");
            return true;
        }

        void Render(IDXGISwapChain* swapChain) {
            if (Stopping.load() || !IsTarget(swapChain)) return;
            std::scoped_lock lock(RenderMutex);
            if (Renderer.SwapChain && Renderer.SwapChain.Get() != swapChain) InvalidateDx11();
            if (!Renderer.Dx11Ready && !CreateRenderer(swapChain)) return;
            ImGui::SetCurrentContext(Renderer.Context);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            auto& io = ImGui::GetIO();
            io.MouseDrawCursor = AppState::MenuOpen.load();
            if (AppState::MenuOpen.load()) ClipCursor(nullptr);
            ImGui::NewFrame();
            Menu::Draw();
            ImGui::Render();

            ComPtr<ID3D11RenderTargetView> previousTarget;
            ComPtr<ID3D11DepthStencilView> previousDepth;
            Renderer.DeviceContext->OMGetRenderTargets(1, &previousTarget, &previousDepth);
            auto* target = Renderer.RenderTarget.Get();
            Renderer.DeviceContext->OMSetRenderTargets(1, &target, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            auto* oldTarget = previousTarget.Get();
            Renderer.DeviceContext->OMSetRenderTargets(1, &oldTarget, previousDepth.Get());
            if (FrameCount.fetch_add(1) == 0) Logger::Write("Menu rendering active");
        }

        HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
            CallScope scope;
            try { if (Installed.load(std::memory_order_acquire)) Render(swapChain); }
            catch (const std::exception& exception) { Logger::Write("Render hatasi: %s", exception.what()); }
            catch (...) { Logger::Write("Render hatasi: bilinmeyen hata"); }
            return OriginalPresent(swapChain, syncInterval, flags);
        }

        HRESULT STDMETHODCALLTYPE HookResize(IDXGISwapChain* swapChain, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
            CallScope scope;
            if (Installed.load(std::memory_order_acquire) && !Stopping.load() && IsTarget(swapChain)) {
                std::scoped_lock lock(RenderMutex);
                InvalidateDx11();
            }
            return OriginalResize(swapChain, count, width, height, format, flags);
        }

        LRESULT CALLBACK DummyProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            return DefWindowProcW(window, message, wParam, lParam);
        }

        void AddHook(void* target, void* detour, void** original) {
            Logger::Write("Hook olusturuluyor: %p", target);
            if (MH_CreateHook(target, detour, original) != MH_OK) throw std::runtime_error("grafik hook olusturulamadi");
            HookTargets.push_back(target);
            Logger::Write("Hook hazir: %p", target);
        }

        void DiscoverHooks() {
            Logger::Write("DX11 probe baslatildi");
            const auto instance = GetModuleHandleW(nullptr);
            const wchar_t* className = L"StreamerDailyInternalProbe";
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = DummyProc;
            windowClass.hInstance = instance;
            windowClass.lpszClassName = className;
            RegisterClassW(&windowClass);
            const auto dummy = CreateWindowExW(0, className, L"", WS_OVERLAPPEDWINDOW,
                0, 0, 32, 32, nullptr, nullptr, instance, nullptr);
            if (!dummy) throw std::runtime_error("grafik probe penceresi acilamadi");
            Logger::Write("DX11 probe penceresi hazir");

            DXGI_SWAP_CHAIN_DESC description{};
            description.BufferDesc.Width = 32;
            description.BufferDesc.Height = 32;
            description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = 2;
            description.OutputWindow = dummy;
            description.Windowed = TRUE;
            description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            ComPtr<IDXGISwapChain> swapChain;
            ComPtr<ID3D11Device> device;
            ComPtr<ID3D11DeviceContext> context;
            auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &description, &swapChain, &device, nullptr, &context);
            if (FAILED(result)) {
                result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                    D3D11_SDK_VERSION, &description, &swapChain, &device, nullptr, &context);
            }
            if (FAILED(result)) {
                DestroyWindow(dummy);
                UnregisterClassW(className, instance);
                throw std::runtime_error("DX11 probe olusturulamadi");
            }
            Logger::Write("DX11 probe swapchain hazir");
            auto** table = *reinterpret_cast<void***>(swapChain.Get());
            AddHook(table[8], reinterpret_cast<void*>(HookPresent), reinterpret_cast<void**>(&OriginalPresent));
            AddHook(table[13], reinterpret_cast<void*>(HookResize), reinterpret_cast<void**>(&OriginalResize));
            swapChain.Reset();
            context.Reset();
            device.Reset();
            DestroyWindow(dummy);
            UnregisterClassW(className, instance);
            Logger::Write("DX11 probe tamamlandi");
        }
    }

    bool Install() {
        try {
            Stopping.store(false);
            Installed.store(false);
            Logger::Write("Grafik kurulumu basladi");
            if (!FindGameWindow()) throw std::runtime_error("oyun penceresi bulunamadi");
            Logger::Write("Oyun penceresi bulundu: %p", GameWindow);
            Logger::Write("ImGui context kurulum is parcaciginda hazirlaniyor");
            IMGUI_CHECKVERSION();
            Renderer.Context = ImGui::CreateContext();
            if (!Renderer.Context) throw std::runtime_error("ImGui context olusturulamadi");
            ImGui::SetCurrentContext(Renderer.Context);
            Menu::Initialize();
            Logger::Write("ImGui context ve menu stili hazir");
            const auto init = MH_Initialize();
            if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("MinHook baslatilamadi");
            Logger::Write("MinHook hazir");
            DiscoverHooks();
            Logger::Write("Grafik hooklari etkinlestiriliyor");
            if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) throw std::runtime_error("grafik hook etkinlestirilemedi");
            Logger::Write("DX11 Present ve ResizeBuffers baglandi");
            Installed.store(true, std::memory_order_release);
            return true;
        } catch (const std::exception& exception) {
            Logger::Write("Grafik kurulumu basarisiz: %s", exception.what());
            Uninstall();
            return false;
        }
    }

    void Uninstall() {
        Installed.store(false, std::memory_order_release);
        Stopping.store(true);
        for (auto* target : HookTargets) MH_DisableHook(target);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (ActiveCalls.load() > 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        {
            std::scoped_lock lock(RenderMutex);
            DestroyRenderer();
        }
        for (auto* target : HookTargets) MH_RemoveHook(target);
        HookTargets.clear();
        MH_Uninitialize();
        Logger::Write("Grafik ve giris baglantilari kaldirildi");
    }
}

