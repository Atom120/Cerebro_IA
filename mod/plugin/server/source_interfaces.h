#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "ar/units.h"

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

// ---------------------------------------------------------------------------------------
// Source Engine Trace definitions (EngineTraceServer003 / IEngineTrace)
// ---------------------------------------------------------------------------------------

struct alignas(16) VectorAligned {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr VectorAligned() = default;
    constexpr VectorAligned(float x_, float y_, float z_, float w_ = 0.0f) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr explicit VectorAligned(const Vec3& v) : x(v.x), y(v.y), z(v.z), w(0.0f) {}

    void Init(float x_ = 0.0f, float y_ = 0.0f, float z_ = 0.0f) {
        x = x_;
        y = y_;
        z = z_;
        w = 0.0f;
    }
};

// Source SDK 2013 public/cmodel.h. CS:GO added m_pWorldAxisTransform before the bools; Portal 1 does
// not have it (verified in build 19017868: with it, traces go through the floor).
struct Ray_t {
    VectorAligned m_Start;
    VectorAligned m_Delta;
    VectorAligned m_StartOffset;
    VectorAligned m_Extents;
    bool m_IsRay = true;
    bool m_IsSwept = false;

    void Init(const Vec3& start, const Vec3& end) {
        m_Delta = VectorAligned(end.x - start.x, end.y - start.y, end.z - start.z);
        m_IsSwept = ((m_Delta.x * m_Delta.x + m_Delta.y * m_Delta.y + m_Delta.z * m_Delta.z) != 0.0f);
        m_Extents.Init();
        m_IsRay = true;
        m_StartOffset.Init();
        m_Start = VectorAligned(start);
    }

    void Init(const Vec3& start, const Vec3& end, const Vec3& mins, const Vec3& maxs) {
        m_Delta = VectorAligned(end.x - start.x, end.y - start.y, end.z - start.z);
        m_IsSwept = ((m_Delta.x * m_Delta.x + m_Delta.y * m_Delta.y + m_Delta.z * m_Delta.z) != 0.0f);
        m_Extents = VectorAligned((maxs.x - mins.x) * 0.5f,
                                  (maxs.y - mins.y) * 0.5f,
                                  (maxs.z - mins.z) * 0.5f);
        m_IsRay = ((m_Extents.x * m_Extents.x + m_Extents.y * m_Extents.y + m_Extents.z * m_Extents.z) < 1e-6f);
        m_StartOffset = VectorAligned((mins.x + maxs.x) * 0.5f,
                                      (mins.y + maxs.y) * 0.5f,
                                      (mins.z + maxs.z) * 0.5f);
        m_Start = VectorAligned(start.x + m_StartOffset.x,
                                start.y + m_StartOffset.y,
                                start.z + m_StartOffset.z);
        m_StartOffset.x = -m_StartOffset.x;
        m_StartOffset.y = -m_StartOffset.y;
        m_StartOffset.z = -m_StartOffset.z;
    }
};

struct cplane_t {
    float normal[3] = {};
    float dist = 0.0f;
    uint8_t type = 0;
    uint8_t signbits = 0;
    uint8_t pad[2] = {};
};

struct csurface_t {
    const char* name = nullptr;
    short surfaceProps = 0;
    unsigned short flags = 0;
};

struct CBaseTrace {
    float startpos[3] = {};
    float endpos[3] = {};
    cplane_t plane;
    float fraction = 1.0f;
    int contents = 0;
    unsigned short dispFlags = 0;
    bool allsolid = false;
    bool startsolid = false;
};

struct trace_t : public CBaseTrace {
    float fractionleftsolid = 0.0f;
    csurface_t surface;
    int hitgroup = 0;
    short physicsbone = 0;
    unsigned short worldSurfaceIndex = 0;
    void* m_pEnt = nullptr;
    int hitbox = 0;
};

enum TraceType_t : int {
    TRACE_EVERYTHING = 0,
    TRACE_WORLD_ONLY,
    TRACE_ENTITIES_ONLY,
    TRACE_EVERYTHING_FILTER_PROPS,
};

class ITraceFilter {
public:
    virtual bool ShouldHitEntity(void* pEntity, int contentsMask) = 0;
    virtual TraceType_t GetTraceType() const = 0;
};

// Source SDK 2013 public/engine/IEngineTrace.h, only the first 5 slots (TraceRay is slot 4). CS:GO
// added GetPointContents_WorldOnly and a contentsMask argument; Portal 1 has neither (verified in
// build 19017868: calling slot 5 raises an access violation).
class IEngineTrace {
public:
    virtual int GetPointContents(const float* vecAbsPosition, void** ppEntity = nullptr) = 0;
    virtual int GetPointContents_Collideable(void* pCollide, const float* vecAbsPosition) = 0;
    virtual void ClipRayToEntity(const Ray_t& ray, unsigned int fMask, void* pEnt, trace_t* pTrace) = 0;
    virtual void ClipRayToCollideable(const Ray_t& ray, unsigned int fMask, void* pCollide, trace_t* pTrace) = 0;
    virtual void TraceRay(const Ray_t& ray, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace) = 0;
};

inline constexpr const char* kEngineTraceServerVersion = "EngineTraceServer003";

// Layout checks against Source SDK 2013 (32-bit). A mismatch silently breaks every trace.
static_assert(offsetof(Ray_t, m_IsRay) == 64 && offsetof(Ray_t, m_IsSwept) == 65, "Ray_t must match SDK 2013");
static_assert(offsetof(CBaseTrace, fraction) == 44 && offsetof(CBaseTrace, allsolid) == 54, "CBaseTrace must match SDK 2013");
static_assert(sizeof(void*) != 4 || offsetof(trace_t, m_pEnt) == 76, "trace_t must match SDK 2013 (x86)");

// Entity vtables used to read a hit entity's class name (Source SDK 2013, MSVC x86):
//   CBaseEntity -> IServerEntity -> IServerUnknown -> IHandleEntity:
//     0 ~IHandleEntity, 1 SetRefEHandle, 2 GetRefEHandle, 3 GetCollideable, 4 GetNetworkable, 5 GetBaseEntity
//   IServerNetworkable: 0 GetEntityHandle, 1 GetServerClass, 2 GetEdict, 3 GetClassName
inline constexpr int kServerUnknownGetCollideableSlot = 3;
inline constexpr int kServerUnknownGetNetworkableSlot = 4;
inline constexpr int kServerNetworkableGetClassNameSlot = 3;
// ICollideable: 10 GetCollisionOrigin (const Vector&), 11 GetCollisionAngles (const QAngle&).
inline constexpr int kCollideableGetOriginSlot = 10;
inline constexpr int kCollideableGetAnglesSlot = 11;

// ---------------------------------------------------------------------------------------
// Movement and entity data (Z5). Every offset verified in Portal build 19017868 (2026-11-04).
// ---------------------------------------------------------------------------------------

// game/shared/igamemovement.h CMoveData, the fields the mod reads or writes.
struct CMoveData {
    uint8_t m_bFirstRunOfFunctions_bits;  // two bool bitfields
    uint8_t pad0[3];
    uint32_t m_nPlayerHandle;
    int m_nImpulseCommand;
    float m_vecViewAngles[3];     // degrees (pitch, yaw, roll), +pitch looks down
    float m_vecAbsViewAngles[3];
    int m_nButtons;               // IN_* bits held
    int m_nOldButtons;            // IN_* bits of the previous command
    float m_flForwardMove;        // +-cl_forwardspeed (450)
    float m_flSideMove;           // +-cl_sidespeed (450), + = right
    float m_flUpMove;
    float m_flMaxSpeed;
    float m_flClientMaxSpeed;
    float m_vecVelocity[3];
    float m_vecAngles[3];
    float m_vecOldAngles[3];
    float m_outStepHeight;
    float m_outWishVel[3];
    float m_outJumpVel[3];
    float m_vecConstraintCenter[3];
    float m_flConstraintRadius;
    float m_flConstraintWidth;
    float m_flConstraintSpeedFactor;
    float m_vecAbsOrigin[3];      // player feet
};
static_assert(offsetof(CMoveData, m_vecViewAngles) == 12 && offsetof(CMoveData, m_nButtons) == 36 &&
                  offsetof(CMoveData, m_flForwardMove) == 44 && offsetof(CMoveData, m_vecVelocity) == 64 &&
                  offsetof(CMoveData, m_vecAbsOrigin) == 152,
              "CMoveData must match SDK 2013");

// public/in_buttons.h
inline constexpr int kInJump = 1 << 1;
inline constexpr int kInDuck = 1 << 2;
inline constexpr int kInForward = 1 << 3;
inline constexpr int kInReload = 1 << 13;
inline constexpr int kInWalk = 1 << 18;
inline constexpr float kForwardSpeed = 450.0f;  // cl_forwardspeed / cl_sidespeed

// public/edict.h edict_t (x86): 20 bytes, IServerUnknown* m_pUnk at +12.
inline constexpr int kEdictSize = 20;
inline constexpr int kEdictUnknownOffset = 12;

// tier1/convar.h CCommand: int m_nArgc at +0, const char* m_ppArgv[64] at +1032.
inline constexpr int kCommandArgvOffset = 1032;

// MASK_PLAYERSOLID = CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_PLAYERCLIP | CONTENTS_WINDOW | CONTENTS_MONSTER | CONTENTS_GRATE
inline constexpr unsigned int kMaskPlayerSolid = 0x201400Bu;

}  // namespace ar::source
