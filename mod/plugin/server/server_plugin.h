#pragma once

#include "source_interfaces.h"
#include "server_trace_provider.h"
#include "ar/units.h"

namespace ar {

class ApertureRunnerServerPlugin final : public source::IServerPluginCallbacks {
public:
    static ApertureRunnerServerPlugin& Instance();

    bool Load(source::CreateInterfaceFn interfaceFactory, source::CreateInterfaceFn gameServerFactory) override;
    void Unload() override;
    void Pause() override {}
    void UnPause() override {}
    const char* GetPluginDescription() override;
    void LevelInit(const char* pMapName) override;
    void ServerActivate(void* pEdictList, int edictCount, int clientMax) override;
    void GameFrame(bool simulating) override;
    void LevelShutdown() override;
    void ClientActive(void* pEntity) override;
    void ClientDisconnect(void* pEntity) override;
    void ClientPutInServer(void* pEntity, const char* playername) override;
    void SetCommandClient(int index) override;
    void ClientSettingsChanged(void* pEdict) override;
    source::PluginResult ClientConnect(bool* bAllowConnect, void* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) override;
    source::PluginResult ClientCommand(void* pEntity, const void* args) override;
    source::PluginResult NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) override;
    void OnQueryCvarValueFinished(int iCookie, void* pPlayerEntity, source::QueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) override;
    void OnEdictAllocated(void* edict) override;
    void OnEdictFreed(const void* edict) override;

    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    ServerTraceProvider* GetTraceProvider() { return &trace_provider_; }
    const ServerTraceProvider* GetTraceProvider() const { return &trace_provider_; }

    // Enrutador de mensajes: imprime en consola de Source (tier0.dll Msg), OutputDebugString y stdout
    static void LogMsg(const char* fmt, ...);

private:
    ApertureRunnerServerPlugin() = default;
    ~ApertureRunnerServerPlugin() = default;
    ApertureRunnerServerPlugin(const ApertureRunnerServerPlugin&) = delete;
    ApertureRunnerServerPlugin& operator=(const ApertureRunnerServerPlugin&) = delete;

    bool enabled_ = false;
    bool is_loaded_ = false;
    char current_map_[64] = {};

    source::IEngineTrace* engine_trace_ = nullptr;
    ServerTraceProvider trace_provider_;
};

}  // namespace ar
