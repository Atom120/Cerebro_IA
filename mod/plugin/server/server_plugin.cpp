#define _CRT_SECURE_NO_WARNINGS 1
#include "server_plugin.h"
#include "movement_hook.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

typedef void (*MsgFn)(const char* fmt, ...);
static MsgFn s_pfnMsg = nullptr;

ApertureRunnerServerPlugin& ApertureRunnerServerPlugin::Instance() {
    static ApertureRunnerServerPlugin instance;
    return instance;
}

const char* ApertureRunnerServerPlugin::GetPluginDescription() {
    return "ApertureRunner: Mirror's Edge movement in Portal 1 (testchmb_a_00)";
}

void ApertureRunnerServerPlugin::LogMsg(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

#ifdef _WIN32
    if (s_pfnMsg) {
        s_pfnMsg("%s", buffer);
    }
    OutputDebugStringA(buffer);
#endif
    std::printf("%s", buffer);
}

bool ApertureRunnerServerPlugin::Load(source::CreateInterfaceFn /*interfaceFactory*/, source::CreateInterfaceFn gameServerFactory) {
    if (is_loaded_) return true;

#ifdef _WIN32
    if (!s_pfnMsg) {
        HMODULE hTier0 = GetModuleHandleA("tier0.dll");
        if (hTier0) {
            s_pfnMsg = reinterpret_cast<MsgFn>(GetProcAddress(hTier0, "Msg"));
        }
    }
#endif

    LogMsg(
        "\n======================================================\n"
        " [ApertureRunner] Plugin VSP cargado con exito.\n"
        " Target Engine : Portal 1 (Build %u, Win32/x86)\n"
        " Estado Inicial: ar_enable = 0 (passthrough)\n"
        "======================================================\n\n",
        kPortalBuildId);

    // Z2/Z3: Obtener GameMovement001 y enganchar ProcessMovement en passthrough
    if (gameServerFactory) {
        int returnCode = 0;
        void* iface = gameServerFactory(source::kGameMovementVersion, &returnCode);
        if (iface && returnCode == 0) {
            auto* pGM = static_cast<source::IGameMovement*>(iface);
            if (MovementHook::Install(pGM)) {
                LogMsg("[ApertureRunner] IGameMovement::ProcessMovement hookeado exitosamente en passthrough (Hito H1).\n");
            } else {
                LogMsg("[ApertureRunner] AVISO: No se pudo instalar el hook en GameMovement001.\n");
            }
        } else {
            LogMsg("[ApertureRunner] AVISO: GameMovement001 no disponible en gameServerFactory.\n");
        }
    }

    is_loaded_ = true;
    return true;
}

void ApertureRunnerServerPlugin::Unload() {
    if (!is_loaded_) return;

    if (MovementHook::IsHooked()) {
        MovementHook::Uninstall();
        LogMsg("[ApertureRunner] VTable de IGameMovement restaurada al original.\n");
    }

    LogMsg("[ApertureRunner] Plugin descargado limpiamente.\n");
    is_loaded_ = false;
}

void ApertureRunnerServerPlugin::LevelInit(const char* pMapName) {
    if (pMapName) {
        std::strncpy(current_map_, pMapName, sizeof(current_map_) - 1);
        current_map_[sizeof(current_map_) - 1] = '\0';
        LogMsg("[ApertureRunner] LevelInit: %s\n", current_map_);
    }
}

void ApertureRunnerServerPlugin::ServerActivate(void* /*pEdictList*/, int /*edictCount*/, int /*clientMax*/) {
    LogMsg("[ApertureRunner] ServerActivate listo en mapa %s\n", current_map_);
}

void ApertureRunnerServerPlugin::GameFrame(bool /*simulating*/) {
}

void ApertureRunnerServerPlugin::LevelShutdown() {
    LogMsg("[ApertureRunner] LevelShutdown en mapa %s\n", current_map_);
    current_map_[0] = '\0';
}

void ApertureRunnerServerPlugin::ClientActive(void* /*pEntity*/) {
}

void ApertureRunnerServerPlugin::ClientDisconnect(void* /*pEntity*/) {
}

void ApertureRunnerServerPlugin::ClientPutInServer(void* /*pEntity*/, const char* /*playername*/) {
}

void ApertureRunnerServerPlugin::SetCommandClient(int /*index*/) {
}

void ApertureRunnerServerPlugin::ClientSettingsChanged(void* /*pEdict*/) {
}

source::PluginResult ApertureRunnerServerPlugin::ClientConnect(bool* /*bAllowConnect*/, void* /*pEntity*/, const char* /*pszName*/, const char* /*pszAddress*/, char* /*reject*/, int /*maxrejectlen*/) {
    return source::kPluginContinue;
}

source::PluginResult ApertureRunnerServerPlugin::ClientCommand(void* /*pEntity*/, const void* /*args*/) {
    return source::kPluginContinue;
}

source::PluginResult ApertureRunnerServerPlugin::NetworkIDValidated(const char* /*pszUserName*/, const char* /*pszNetworkID*/) {
    return source::kPluginContinue;
}

void ApertureRunnerServerPlugin::OnQueryCvarValueFinished(int /*iCookie*/, void* /*pPlayerEntity*/, source::QueryCvarValueStatus /*eStatus*/, const char* /*pCvarName*/, const char* /*pCvarValue*/) {
}

void ApertureRunnerServerPlugin::OnEdictAllocated(void* /*edict*/) {
}

void ApertureRunnerServerPlugin::OnEdictFreed(const void* /*edict*/) {
}

}  // namespace ar
