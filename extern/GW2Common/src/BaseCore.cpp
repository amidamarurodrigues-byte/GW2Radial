#include "BaseCore.h"

#include <CommCtrl.h>
#include <IconFontCppHeaders/IconsFontAwesome5.h>
#include <d3d11_4.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/imgui.h>
#include <shellapi.h>

#include "GFXSettings.h"
#include "Graphics.h"
#include "ImGuiPopup.h"
#include "ShaderManager.h"
#include "UpdateCheck.h"
#include <baseresource.h>

void BaseCore::Init(HMODULE dll) {
    LogInfo("This is {} {}", GetAddonName(), GetAddonVersionString());

    auto osVer = GetOSVersion();
    if(osVer.dwOSVersionInfoSize == sizeof(RTL_OSVERSIONINFOW))
        LogInfo("Operating system version {}.{}.{}", osVer.dwMajorVersion, osVer.dwMinorVersion, osVer.dwBuildNumber);
    else
        LogWarn("Could not identify operating system version.");

    LogInfo("CPU is {}", GetCpuInfo());

    // No exception handler is installed here: when running under Nexus, its crash handler is in charge.

    GetBaseCore().InternalInit(dll);

    // Joined on shutdown (rather than detached) so that it can never outlive the DLL when the addon is unloaded at runtime.
    GetBaseCore().watchdog_ = std::jthread([](std::stop_token stopToken) {
        using namespace std::chrono_literals;
        for(i32 i = 0; i < 50; i++) {
            if(stopToken.stop_requested())
                return;
            std::this_thread::sleep_for(100ms);
        }

        if(!GetBaseCore().swapChainInitialized()) {
            LogWarn(
                "Graphics hooks not called after 5 seconds, possible interference detected. Listing currently loaded modules for debugging "
                "purposes:");
            LogCurrentModules();
        }
    });
}

void BaseCore::Shutdown() {
    GetBaseCore().InternalShutdown();

    g_singletonManagerInstance.Shutdown();
}

void BaseCore::OnInputLanguageChange() {
    if(!active_)
        return;

    LogInfo("Input language change detected, reloading...");
    SettingsMenu::i().OnInputLanguageChange();

    languageChangeEvent_();
}

UINT BaseCore::GetDpiForWindow(HWND hwnd) {
    if(getDpiForWindow_)
        return getDpiForWindow_(hwnd);
    else
        return 96;
}

void BaseCore::InternalInit(HMODULE dll) {
    dllModule_ = dll;

    {
        wchar_t fn[MAX_PATH];
        GetModuleFileName(dllModule_, fn, MAX_PATH);

        addonDirectory_ = std::filesystem::path(fn).remove_filename();
    }

    user32_ = LoadLibrary(L"User32.dll");
    if(user32_)
        getDpiForWindow_ = (GetDpiForWindow_t)GetProcAddress(user32_, "GetDpiForWindow");

#ifdef _DEBUG
    LogInfo("Installing CRT report hook...");
    i32 rv = _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, CRTReportHook);
    if(rv < 0)
        LogWarn("CRT report hook install failed: {}", rv);
#endif

    imguiContext_ = ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;

    InnerInternalInit();
    UpdateCheck::init(GetGithubRepoSubUrl());
}

void BaseCore::InternalShutdown() {
    ready_ = false;
    watchdog_.request_stop();
    if(watchdog_.joinable())
        watchdog_.join();

    InnerShutdown();

#ifdef _DEBUG
    _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, CRTReportHook);
#endif

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    device_.Reset();
    context_.Reset();
    swc_.Reset();
    backBufferRTV_.Reset();
    annotations_.Reset();
    active_ = false;

    gameWindow_ = nullptr;
    getDpiForWindow_ = nullptr;
    if(user32_)
        FreeLibrary(user32_);
    user32_ = nullptr;
}

void BaseCore::OnFocusLost() { Input::i().OnFocusLost(); }

void BaseCore::OnFocus() {
    ShaderManager::i().ReloadAll();

    Input::i().OnFocus();
}

bool BaseCore::ProcessWindowMessage(UINT& msg, WPARAM& wParam, LPARAM& lParam) {
    // Everything below depends on the swap chain/ImGui being fully set up.
    if(!ready_)
        return false;

    if(msg == WM_KILLFOCUS)
        OnFocusLost();
    else if(msg == WM_SETFOCUS)
        OnFocus();
    else if(auto rval = OnInput(msg, wParam, lParam); rval)
        return true;
    else if(Input::i().OnInput(msg, wParam, lParam))
        return true;

    // Whatever's left should be sent to the game
    return false;
}

// The back buffer view is created and released within a single frame (see Draw) so that we never hold a reference to the
// back buffer while the game resizes the swap chain; ResizeBuffers fails if any such reference is outstanding.
void BaseCore::UpdateBackBuffer() {
    ComPtr<ID3D11Texture2D> backbuffer;
    if(FAILED(swc_->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf()))))
        return;

    D3D11_TEXTURE2D_DESC desc;
    backbuffer->GetDesc(&desc);
    screenWidth_ = desc.Width;
    screenHeight_ = desc.Height;

    device_->CreateRenderTargetView(backbuffer.Get(), nullptr, backBufferRTV_.ReleaseAndGetAddressOf());
}

bool BaseCore::CheckForConflictingModule(const char* name, const char* message) {
    if(GetModuleHandleA(name)) {
        DisplayErrorPopup(std::format("Error: A known application was found to be interfering with the game. {}", message).c_str());
        return true;
    }

    return false;
}

void BaseCore::PostCreateSwapChain(HWND hwnd, ID3D11Device* device, IDXGISwapChain* swc) {
    swapChainInitialized_ = true;
    ComPtr<IDXGIDevice> dxgiDevice;
    swc->GetDevice(IID_PPV_ARGS(&dxgiDevice));
    if(dxgiDevice) {
        ComPtr<IDXGIAdapter> adapter;
        dxgiDevice->GetAdapter(&adapter);

        if(adapter) {
            DXGI_ADAPTER_DESC desc;
            adapter->GetDesc(&desc);

            LogInfo(L"Graphics adapter is {}", desc.Description);
        }
    }
    gameWindow_ = hwnd;

    device_ = device;
    device_->GetImmediateContext(&context_);
    swc_ = swc;

    RenderDocCapture::Init(device_);

    context_->QueryInterface(annotations_.ReleaseAndGetAddressOf());

    // Sets up the screen size; the render target view itself only lives for the duration of a frame.
    UpdateBackBuffer();
    backBufferRTV_.Reset();

    firstFrame_ = true;

    ShaderManager::init(device_, GetShaderArchiveID(), dllModule_, GetShaderDirectory());

    UpdateCheck::i().CheckForUpdates();

    InnerInitPreImGui();

    // Init ImGui
    auto& imio = ImGui::GetIO();
    imio.IniFilename = nullptr;
    imio.IniSavingRate = 1.0f;
    auto fontCfg = ImFontConfig();
    fontCfg.FontDataOwnedByAtlas = false;

    constexpr f32 fontSize = 25.f;
    constexpr f32 fontSizeMono = 18.f;
    constexpr f32 fontSizeIcon = fontSize * 2.f / 3.f;

    const auto iconData = LoadResource(dllModule_, IDR_FONT_ICON);
    auto appendIcons = [&] {
        if(iconData.data()) {
            auto iconFontCfg = fontCfg;
            iconFontCfg.GlyphMinAdvanceX = fontSizeIcon;
            iconFontCfg.MergeMode = true;
            iconFontCfg.PixelSnapH = true;
            static const ImWchar iconRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            imio.Fonts->AddFontFromMemoryTTF(iconData.data(), i32(iconData.size_bytes()), fontSizeIcon, &iconFontCfg, iconRange);
        }
    };

    if(const auto data = LoadResource(dllModule_, IDR_FONT); data.data()) {
        font_ = imio.Fonts->AddFontFromMemoryTTF(data.data(), i32(data.size_bytes()), fontSize, &fontCfg);
        appendIcons();
    }
    if(const auto data = LoadResource(dllModule_, IDR_FONT_BLACK); data.data()) {
        fontBold_ = imio.Fonts->AddFontFromMemoryTTF(data.data(), i32(data.size_bytes()), fontSize, &fontCfg);
        appendIcons();
    }
    if(const auto data = LoadResource(dllModule_, IDR_FONT_ITALIC); data.data())
        fontItalic_ = imio.Fonts->AddFontFromMemoryTTF(data.data(), i32(data.size_bytes()), fontSize, &fontCfg);
    if(const auto data = LoadResource(dllModule_, IDR_FONT_MONO); data.data())
        fontMono_ = imio.Fonts->AddFontFromMemoryTTF(data.data(), i32(data.size_bytes()), fontSizeMono, &fontCfg);

    if(font_)
        imio.FontDefault = font_;
    else
        imio.Fonts->AddFontDefault();

    InnerInitPreFontImGui();

    ImGui_ImplWin32_Init(gameWindow_);
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());

    InnerInitPostImGui();

    // Window messages are only processed from this point on.
    ready_ = true;

#ifdef _DEBUG
    ID3D11Debug* d3dDebug = nullptr;
    if(SUCCEEDED(device_->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3dDebug))) {
        ID3D11InfoQueue* d3dInfoQueue = nullptr;
        if(SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3dInfoQueue))) {
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            // d3dInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);

            D3D11_MESSAGE_ID hide[] = {
                D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
            };

            D3D11_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = UINT(std::size(hide));
            filter.DenyList.pIDList = hide;
            d3dInfoQueue->AddStorageFilterEntries(&filter);
            d3dInfoQueue->Release();
        }
        d3dDebug->Release();
    }
#endif
}

void BaseCore::DisplayErrorPopup(const char* message) { errorPopupMessages_.push_back(message); }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void BaseCore::Draw() {
    if(!active_)
        return;

    UpdateBackBuffer();
    if(!backBufferRTV_)
        return;

    if(annotations_)
        annotations_->BeginEvent(GetAddonNameW());

    StateBackupD3D11 d3dstate;
    BackupD3D11State(context_.Get(), d3dstate);

    context_->OMSetRenderTargets(1, backBufferRTV_.GetAddressOf(), nullptr);

    {
        std::lock_guard guard(imguiInputMutex_);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    // This is the closest we have to a reliable "update" function, so use it as one
    Update();

    if(firstFrame_) {
        firstFrame_ = false;
    }
    else {
        {
            std::lock_guard guard(imguiInputMutex_);
            auto& imguiInputs = Input::i().imguiInputs();
            Input::DelayedImguiInput dii;
            while (imguiInputs.try_pop(dii)) {
                ImGui_ImplWin32_WndProcHandler(gameWindow_, dii.msg, dii.wParam, dii.lParam);
            }
        }

        // Setup viewport
        D3D11_VIEWPORT vp;
        memset(&vp, 0, sizeof(D3D11_VIEWPORT));
        vp.Width = FLOAT(screenWidth_);
        vp.Height = FLOAT(screenHeight_);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = vp.TopLeftY = 0;
        context_->RSSetViewports(1, &vp);

        InnerDraw();

        if(!errorPopupID_) {
            errorPopupTitle_ = std::format("{} Error", GetAddonName());
            errorPopupID_ = ImGui::GetID(errorPopupTitle_.c_str());
        }

        if(!errorPopupMessages_.empty() && !ImGui::IsPopupOpen(errorPopupTitle_.c_str()))
            ImGui::OpenPopup(errorPopupID_);

        ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * 0.5f, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(screenWidth() * 0.25f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.f, 30.f));
        if(ImGui::BeginPopupModal(errorPopupTitle_.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushTextWrapPos();

            if(errorPopupMessages_.size() == 1)
                ImGui::Text("An error has occurred: %s\nAddon stability and functionality may be degraded.",
                            errorPopupMessages_[0].c_str());
            else {
                ImGui::TextUnformatted("Multiple errors have occurred:");
                for(const auto& s : errorPopupMessages_) {
                    ImGui::Bullet();
                    ImGui::TextUnformatted(s.c_str());
                }
                ImGui::TextUnformatted("Addon stability and functionality may be degraded.");
            }

            ImGui::Dummy(ImVec2(1.f, 30.f));

            constexpr ImVec2 buttonSize(100.f, 0.f);
            const f32 width = ImGui::GetWindowSize().x;
            const f32 centrePositionForButton = (width - buttonSize.x) / 2;
            ImGui::SetCursorPosX(centrePositionForButton);

            if(ImGui::Button("OK", buttonSize)) {
                errorPopupMessages_.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        SettingsMenu::i().Draw();
        Log::i().Draw();

        if(!INIConfigurationFile::i().lastSaveError().empty() && INIConfigurationFile::i().lastSaveErrorChanged())
            ImGuiPopup("Configuration could not be saved!")
                .Position({ 0.5f, 0.45f })
                .Size({ 0.35f, 0.2f })
                .Display(
                    [&](const ImVec2&) {
                        ImGui::Text("Could not save addon configuration. Reason given was:");
                        ImGui::TextWrapped(INIConfigurationFile::i().lastSaveError().c_str());
                    },
                    []() { INIConfigurationFile::i().lastSaveErrorChanged(false); });
        if(!JSONConfigurationFile::i().lastSaveError().empty() && JSONConfigurationFile::i().lastSaveErrorChanged())
            ImGuiPopup("Configuration could not be saved!")
                .Position({ 0.5f, 0.45f })
                .Size({ 0.35f, 0.2f })
                .Display(
                    [&](const ImVec2&) {
                        ImGui::Text("Could not save addon configuration. Reason given was:");
                        ImGui::TextWrapped(JSONConfigurationFile::i().lastSaveError().c_str());
                    },
                    []() { JSONConfigurationFile::i().lastSaveErrorChanged(false); });

        if(UpdateCheck::i().updateAvailable() && !UpdateCheck::i().updateDismissed())
            ImGuiPopup("Update available!")
                .Position({ 0.5f, 0.45f })
                .Size({ 0.35f, 0.2f })
                .Display(
                    [&](const ImVec2& windowSize) {
                        ImGui::TextWrapped(std::format("A new version of {} has been released! "
                                                       "Please follow the link below to look at the changes and download the update. "
                                                       "Remember that you can always disable this version check in the settings.",
                                                       GetAddonName())
                                               .c_str());

                        ImGui::Spacing();
                        ImGui::SetCursorPosX(windowSize.x * 0.1f);

                        auto url = UpdateCheck::i().repoUrl(L"releases/latest");

                        if(ImGui::Button(utf8_encode(url).c_str(), ImVec2(windowSize.x * 0.8f, ImGui::GetFontSize() * 1.3f)))
                            ShellExecute(0, 0, url.c_str(), 0, 0, SW_SHOW);
                    },
                    []() { UpdateCheck::i().updateDismissed(true); });
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    RestoreD3D11State(context_.Get(), d3dstate);

    // Don't keep the back buffer alive past this frame, the game may resize it before the next one.
    backBufferRTV_.Reset();

    if(annotations_)
        annotations_->EndEvent();
}

void BaseCore::Update() {
    if(!active_)
        return;

    Input::i().OnUpdate();

    tickSkip_++;
    if(tickSkip_ >= TickSkipCount) {
        tickSkip_ -= TickSkipCount;
        MumbleLink::i().OnUpdate();
        InnerFrequentUpdate();
    }

    longTickSkip_++;
    if(longTickSkip_ >= LongTickSkipCount) {
        longTickSkip_ -= LongTickSkipCount;
        INIConfigurationFile::i().OnUpdate();
        GFXSettings::i().OnUpdate();
        UpdateCheck::i().CheckForUpdates();

        InnerInfrequentUpdate();
    }

    InnerUpdate();
}
