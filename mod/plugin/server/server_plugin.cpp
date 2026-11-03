#define _CRT_SECURE_NO_WARNINGS 1
#include "server_plugin.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

ApertureRunnerServerPlugin& ApertureRunnerServerPlugin::Instance() {
    static ApertureRunnerServerPlugin instance;
    return instance;
}

const char* ApertureRunnerServerPlugin::GetPluginDescription() {
    return "ApertureRunner: Mirror's Edge movement in Portal 1 (testchmb_a_00)";
}

bool ApertureRunnerServerPlugin::Load(source::CreateInterfaceFn /*interfaceFactory*/, source::CreateInterfaceFn /*gameServerFactory*/) {
    if (is_loaded_) return true;

    char banner[256];
    std::snprintf(banner, sizeof(banner),
        "\n======================================================\n"
        " [ApertureRunner] Plugin VSP cargado con exito.\n"
        " Target Engine : Portal 1 (Build %u, Win32/x86)\n"
        " Estado Inicial: ar_enable = 0 (passthrough)\n"
        "======================================================\n\n",
        kPortalBuildId);

#ifdef _WIN32
    OutputDebugStringA(banner);
#endif
    std::printf("%s", banner);

    is_loaded_ = true;
    return true;
}

void ApertureRunnerServerPlugin::Unload() {
    if (!is_loaded_) return;
    std::printf("[ApertureRunner] Plugin descargado.\n");
    is_loaded_ = false;
}

void ApertureRunnerServerPlugin::LevelInit(const char* pMapName) {
    if (pMapName) {
        std::strncpy(current_map_, pMapName, sizeof(current_map_) - 1);
        current_map_[sizeof(current_map_) - 1] = '\0';
        std::printf("[ApertureRunner] LevelInit: %s\n", current_map_);
    }
}

void ApertureRunnerServerPlugin::ServerActivate(void* /*pEdictList*/, int /*edictCount*/, int /*clientMax*/) {
    std::printf("[ApertureRunner] ServerActivate listo en mapa %s\n", current_map_);
}

void ApertureRunnerServerPlugin::GameFrame(bool /*simulating*/) {
}

void ApertureRunnerServerPlugin::LevelShutdown() {
    std::printf("[ApertureRunner] LevelShutdown en mapa %s\n", current_map_);
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
