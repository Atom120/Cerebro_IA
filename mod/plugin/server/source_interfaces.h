#pragma once

#include <cstdint>

namespace ar::source {

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

enum PluginResult : int {
    kPluginContinue = 0,
    kPluginOverride,
    kPluginStop,
};

enum QueryCvarValueStatus : int {
    kValueIntact = 0,
    kCvarNotFound,
    kNotACvar,
    kCvarProtected,
};

// Exact binary interface matching Source 2013 ISERVERPLUGINCALLBACKS003 vtable
class IServerPluginCallbacks {
public:
    virtual bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) = 0;
    virtual void Unload() = 0;
    virtual void Pause() = 0;
    virtual void UnPause() = 0;
    virtual const char* GetPluginDescription() = 0;
    virtual void LevelInit(const char* pMapName) = 0;
    virtual void ServerActivate(void* pEdictList, int edictCount, int clientMax) = 0;
    virtual void GameFrame(bool simulating) = 0;
    virtual void LevelShutdown() = 0;
    virtual void ClientActive(void* pEntity) = 0;
    virtual void ClientDisconnect(void* pEntity) = 0;
    virtual void ClientPutInServer(void* pEntity, const char* playername) = 0;
    virtual void SetCommandClient(int index) = 0;
    virtual void ClientSettingsChanged(void* pEdict) = 0;
    virtual PluginResult ClientConnect(bool* bAllowConnect, void* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) = 0;
    virtual PluginResult ClientCommand(void* pEntity, const void* args) = 0;
    virtual PluginResult NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) = 0;
    virtual void OnQueryCvarValueFinished(int iCookie, void* pPlayerEntity, QueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) = 0;
    virtual void OnEdictAllocated(void* edict) = 0;
    virtual void OnEdictFreed(const void* edict) = 0;
};

inline constexpr const char* kServerPluginCallbacksVersion = "ISERVERPLUGINCALLBACKS003";

// Source 2013 SP IGameMovement interface (slot 0: destructor virtual, slot 1: ProcessMovement)
class IGameMovement {
public:
    virtual ~IGameMovement() {}
    virtual void ProcessMovement(void* pPlayer, void* pMove) = 0;
    virtual void StartTrackPredictionErrors(void* pPlayer) = 0;
    virtual void FinishTrackPredictionErrors(void* pPlayer) = 0;
    virtual void DiffPrint(const char* fmt, ...) = 0;
};

inline constexpr const char* kGameMovementVersion = "GameMovement001";

}  // namespace ar::source
