#include <NexusIntegration.h>

#include <Core.h>
#include <Nexus.h>
#include <NexusOptionsUI.h>
#include <Resource.h>
#include <SettingsMenu.h>
#include <Utility.h>
#include <Version.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shared_mutex>

extern std::ofstream g_logStream;

namespace NexusIntegration
{

namespace
{

// Nexus identifiers must be unique across addons, hence the prefix.
constexpr const char* KB_SETTINGS     = "KB_GW2RADIAL_SETTINGS";
constexpr const char* QA_SETTINGS     = "QA_GW2RADIAL_SETTINGS";
constexpr const char* TEX_ICON        = "TEX_GW2RADIAL_ICON";
constexpr const char* TEX_ICON_HOVER  = "TEX_GW2RADIAL_ICON_HOVER";

// Not on Raidcore, so this has to be an arbitrary integer nobody else is likely to use ("GW2R").
constexpr uint32_t    AddonSignature  = 0x47573252;

HMODULE               g_module = nullptr;
AddonAPI_t*           g_api    = nullptr;

std::unique_ptr<ConfigurationOption<bool>> g_hideQuickAccess;
bool                  g_quickAccessAdded = false;
bool                  g_swapChainInitialized = false;

// Callbacks arrive on the render and window threads while Nexus may unload us at any time. Callbacks hold this shared (and
// simply skip their work if shutdown holds it); Unload takes it exclusively, which waits for callbacks already running.
std::shared_mutex     g_lifetime;

void NexusLog(ELogLevel level, const char* message)
{
    if (g_api && g_api->Log)
        g_api->Log(level, "GW2Radial", message);
}

void OpenLogFile()
{
    // Keep the log next to the configuration, in <game>\addons\gw2radial.
    std::filesystem::path dir = g_api->Paths_GetAddonDirectory("gw2radial");
    std::error_code       ec;
    std::filesystem::create_directories(dir, ec);
    g_logStream = std::ofstream(dir / "gw2radial.log");
}

// Runs once per frame after Nexus finished drawing its own UI. We have our own ImGui context, so none of Nexus' state is
// touched; the core draws, then restores the D3D11 state it found.
void OnPostRender()
{
    std::shared_lock lock(g_lifetime, std::try_to_lock);
    if (!lock.owns_lock() || !g_api)
        return;

    auto& core = GetBaseCore();

    if (!g_swapChainInitialized)
    {
        auto* swapChain = static_cast<IDXGISwapChain*>(g_api->SwapChain);
        if (!swapChain)
            return;

        ComPtr<ID3D11Device> device;
        DXGI_SWAP_CHAIN_DESC desc;
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))) || FAILED(swapChain->GetDesc(&desc)))
        {
            NexusLog(LOGL_WARNING, "Could not retrieve D3D11 device from the swap chain, retrying next frame.");
            return;
        }

        g_swapChainInitialized = true;
        core.PostCreateSwapChain(desc.OutputWindow, device.Get(), swapChain);
    }

    core.Draw();
}

// Return value follows the Nexus convention: 0 means the message was consumed, anything else lets it continue.
UINT OnWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    std::shared_lock lock(g_lifetime, std::try_to_lock);
    if (!lock.owns_lock() || !g_api)
        return msg ? msg : 1;

    UINT   newMsg    = msg;
    WPARAM newWParam = wParam;
    LPARAM newLParam = lParam;

    if (GetBaseCore().ProcessWindowMessage(newMsg, newWParam, newLParam))
        return 0;

    // The core rewrites messages it injected itself (it posts them under private message IDs so that it can tell them
    // apart from real input) and the mouse position of clicks while a wheel is open. Nexus' callbacks cannot modify the
    // message in flight, so deliver the rewritten version straight to the game and swallow the original.
    if (newMsg != msg || newWParam != wParam || newLParam != lParam)
    {
        g_api->WndProc_SendToGameOnly(hWnd, newMsg, newWParam, newLParam);
        return 0;
    }

    return msg ? msg : 1;
}

void OnSettingsKeybind(const char* /*identifier*/, bool isRelease)
{
    std::shared_lock lock(g_lifetime, std::try_to_lock);
    if (!lock.owns_lock() || !g_api)
        return;

    if (!isRelease && GetBaseCore().ready())
        SettingsMenu::i().MakeVisible();
}

void ShowQuickAccess()
{
    if (g_quickAccessAdded)
        return;

    const auto icon      = LoadResource(g_module, IDR_NEXUS_ICON);
    const auto iconHover = LoadResource(g_module, IDR_NEXUS_ICON_HOVER);
    if (!icon.data() || !iconHover.data())
        return;

    g_api->Textures_GetOrCreateFromMemory(TEX_ICON, icon.data(), icon.size());
    g_api->Textures_GetOrCreateFromMemory(TEX_ICON_HOVER, iconHover.data(), iconHover.size());
    g_api->QuickAccess_Add(QA_SETTINGS, TEX_ICON, TEX_ICON_HOVER, KB_SETTINGS, "GW2Radial settings");
    g_quickAccessAdded = true;
}

void HideQuickAccess()
{
    if (!g_quickAccessAdded)
        return;

    g_api->QuickAccess_Remove(QA_SETTINGS);
    g_quickAccessAdded = false;
}

// Drawn by Nexus inside its options window (with Nexus' ImGui, hence the separate translation unit).
void OnOptionsRender()
{
    std::shared_lock lock(g_lifetime, std::try_to_lock);
    if (!lock.owns_lock() || !g_api || !g_hideQuickAccess)
        return;

    bool hide = g_hideQuickAccess->value();

    const auto actions = NexusOptionsUI::Draw(hide);

    if (actions.hideIconChanged)
    {
        g_hideQuickAccess->value(hide);
        hide ? HideQuickAccess() : ShowQuickAccess();
    }

    if (actions.openSettings && GetBaseCore().ready())
        SettingsMenu::i().MakeVisible();
}

void Load(AddonAPI_t* api)
{
    g_api                  = api;
    g_swapChainInitialized = false;
    OpenLogFile();

    GW2Radial::Core::Init(g_module);

    g_hideQuickAccess = std::make_unique<ConfigurationOption<bool>>("Hide quick access icon", "hide_quick_access_icon", "Nexus", false);

    NexusOptionsUI::Init(api->ImguiContext, api->ImguiMalloc, api->ImguiFree);

    api->GUI_Register(RT_PostRender, OnPostRender);
    api->GUI_Register(RT_OptionsRender, OnOptionsRender);
    api->WndProc_Register(OnWndProc);

    // No default binding: the settings window already opens with Shift+Alt+M through the addon's own keybind. This entry
    // exists so that it can be bound from Nexus' keybind menu and so that the quick access icon has something to invoke.
    api->InputBinds_RegisterWithString(KB_SETTINGS, OnSettingsKeybind, "(null)");

    if (!g_hideQuickAccess->value())
        ShowQuickAccess();

    NexusLog(LOGL_INFO, "Loaded.");
}

void Unload()
{
    HideQuickAccess();
    g_api->InputBinds_Deregister(KB_SETTINGS);
    g_api->WndProc_Deregister(OnWndProc);
    g_api->GUI_Deregister(OnOptionsRender);
    g_api->GUI_Deregister(OnPostRender);

    // Wait for callbacks that were already running; any later call finds the lock taken (or g_api cleared) and does nothing.
    std::unique_lock lock(g_lifetime);

    g_hideQuickAccess.reset();
    BaseCore::Shutdown();
    g_logStream.close();

    g_swapChainInitialized = false;
    g_api                  = nullptr;
}

} // namespace

void SetModule(HMODULE module)
{
    g_module = module;
}

AddonDefinition_t* GetDefinition()
{
    static AddonDefinition_t definition = []
    {
        const std::array<u64, 4> ver = { GIT_VER };

        AddonDefinition_t        d{};
        d.Signature   = AddonSignature;
        d.APIVersion  = NEXUS_API_VERSION;
        d.Name        = "GW2Radial";
        d.Version     = { static_cast<uint16_t>(ver[0]), static_cast<uint16_t>(ver[1]), static_cast<uint16_t>(ver[2]), static_cast<uint16_t>(ver[3]) };
        d.Author      = "Friendly0Fire (Nexus port)";
        d.Description = "Radial menu overlay to select mount, novelty and more on the fly.";
        d.Load        = Load;
        d.Unload      = Unload;
        // Can be loaded and unloaded at runtime: Unload fully tears down the core (see Unload).
        d.Flags       = AF_None;
        // The upstream releases ship the addon-loader build, so Nexus must not try to update from them.
        d.Provider    = UP_None;
        d.UpdateLink  = nullptr;
        return d;
    }();

    return &definition;
}

} // namespace NexusIntegration
