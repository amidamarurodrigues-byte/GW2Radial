#include <Core.h>
#include <Main.h>
#include <NexusIntegration.h>
#include <Nexus.h>
#include <Version.h>

const char* GetAddonName()
{
    return ADDON_NAME;
}

const wchar_t* GetAddonNameW()
{
    return TEXT(ADDON_NAME);
}

const char* GetAddonVersionString()
{
    return GIT_VER_STR;
}

u64 GetAddonVersion()
{
    static std::array<u64, 4> ver = { GIT_VER };
    return (ver[0] << 16 * 3) + (ver[1] << 16 * 2) + (ver[2] << 16 * 1) + (ver[3] << 16 * 0);
}

BaseCore& GetBaseCore()
{
    return GW2Radial::Core::i();
}

std::ofstream g_logStream;

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    return NexusIntegration::GetDefinition();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        NexusIntegration::SetModule(hModule);
    }

    return TRUE;
}
