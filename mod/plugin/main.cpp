#include "server/server_plugin.h"
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE /*hinstDLL*/, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(static_cast<HMODULE>(nullptr));
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
#endif

extern "C" __declspec(dllexport) void* CreateInterface(const char* pName, int* pReturnCode) {
    if (pName && std::strcmp(pName, ar::source::kServerPluginCallbacksVersion) == 0) {
        if (pReturnCode) *pReturnCode = 0;
        return static_cast<ar::source::IServerPluginCallbacks*>(&ar::ApertureRunnerServerPlugin::Instance());
    }
    if (pReturnCode) *pReturnCode = 1;
    return nullptr;
}
