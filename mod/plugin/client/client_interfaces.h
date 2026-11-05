// Source 2013 Singleplayer (32-bit x86) client.dll interfaces, CMoveData layout and verified offsets
// for Portal 1 build 19017868 (Task G1).
// Owner: Gonzalo Robles (mod/plugin/client/).
#pragma once

#include <cstdint>

#include "ar/faith_movement.h"
#include "ar/units.h"

namespace ar::client {

using CreateInterfaceFn = void* (*)(const char* name, int* return_code);

// Official CreateInterface names in portal/bin/client.dll (build 19017868)
inline constexpr const char* kClientGameMovementVersion = "GameMovement001";
inline constexpr const char* kBaseClientDllVersion = "VClient017";
inline constexpr const char* kClientPredictionVersion = "VClientPrediction001";
inline constexpr const char* kClientEntityListVersion = "VClientEntityList003";

// Source Engine button bitmasks (in_buttons.h)
enum SourceButtons : int32_t {
    kInAttack = (1 << 0),
    kInJump = (1 << 1),
    kInDuck = (1 << 2),
    kInForward = (1 << 3),
    kInBack = (1 << 4),
    kInUse = (1 << 5),
    kInMoveLeft = (1 << 9),
    kInMoveRight = (1 << 10),
    kInAttack2 = (1 << 11),
    kInReload = (1 << 13),
    kInSpeed = (1 << 17),
    kInWalk = (1 << 18),
};

// Verified fallback RVAs in portal/bin/client.dll for Portal 1 build 19017868
// (PE TimeDateStamp = 0x68362D89). Prefer CreateInterface above whenever possible.
namespace build_19017868_rva {
inline constexpr uint32_t kExpectedClientTimestamp = 0x68362D89u;
inline constexpr uintptr_t kMainViewOrigin = 0x0050F484u;      // Vec3 (12 bytes)
inline constexpr uintptr_t kMainViewAngles = 0x0050F490u;      // Vec3/QAngle (12 bytes)
inline constexpr uintptr_t kLocalPlayerPtr = 0x004D78F0u;      // C_Portal_Player*
inline constexpr uintptr_t kPortalOriginOffset = 0x0000028Cu;  // C_Prop_Portal::m_vecOrigin
inline constexpr uintptr_t kPortalAnglesOffset = 0x00000298u;  // C_Prop_Portal::m_angRotation
inline constexpr uintptr_t kPortalLinkedOffset = 0x00000E00u;  // C_Prop_Portal::m_hLinkedPortal
inline constexpr uintptr_t kPortalIsPortal2Offset = 0x00000E05u;
inline constexpr uintptr_t kPortalActivatedOffset = 0x00000E06u;
}  // namespace build_19017868_rva

// Source 2013 SP CMoveData binary layout passed to IGameMovement::ProcessMovement(pPlayer, pMove)
struct CMoveData {
    bool m_bFirstRunOfFunctions = true;
    bool m_bGameCodeMovedPlayer = false;
    uint8_t pad0[2] = {};
    uint32_t m_nPlayerHandle = 0;
    int32_t m_nImpulseCommand = 0;
    Vec3 m_vecViewAngles{};
    Vec3 m_vecAbsViewAngles{};
    int32_t m_nButtons = 0;
    int32_t m_nOldButtons = 0;
    float m_flForwardMove = 0.0f;
    float m_flSideMove = 0.0f;
    float m_flUpMove = 0.0f;
    float m_flMaxSpeed = 320.0f;
    float m_flClientMaxSpeed = 320.0f;
    Vec3 m_vecVelocity{};
    Vec3 m_vecAngles{};
    Vec3 m_vecOldAngles{};
    float m_outStepHeight = 0.0f;
    Vec3 m_outWishVel{};
    Vec3 m_outJumpVel{};
    Vec3 m_vecConstraintCenter{};
    float m_flConstraintRadius = 0.0f;
    float m_flConstraintWidth = 0.0f;
    float m_flConstraintSpeedFactor = 0.0f;
    Vec3 m_vecAbsOrigin{};
};

// Source 2013 SP CUserCmd binary layout passed to IClientMode::CreateMove
struct CUserCmd {
    void* vtable = nullptr;
    int32_t command_number = 0;
    int32_t tick_count = 0;
    Vec3 viewangles{};
    float forwardmove = 0.0f;
    float sidemove = 0.0f;
    float upmove = 0.0f;
    int32_t buttons = 0;
    uint8_t impulse = 0;
    uint8_t pad0[3] = {};
    int32_t weaponselect = 0;
    int32_t weaponsubtype = 0;
    int32_t random_seed = 0;
    int16_t mousedx = 0;
    int16_t mousedy = 0;
    bool hasbeenpredicted = false;
    uint8_t pad1[3] = {};
};

// Binary interface matching client.dll's GameMovement001 vtable in Source 2013 SP (x86)
class IClientGameMovement {
public:
    virtual ~IClientGameMovement() = default;
    virtual void ProcessMovement(void* pPlayer, CMoveData* pMove) = 0;
    virtual void StartTrackPredictionErrors(void* pPlayer) = 0;
    virtual void FinishTrackPredictionErrors(void* pPlayer) = 0;
    virtual void DiffPrint(const char* fmt, ...) = 0;
};

// Binary interface matching VClientEntityList003 in client.dll
class IClientEntityList {
public:
    virtual void* GetClientNetworkable(int entnum) = 0;
    virtual void* GetClientNetworkableFromHandle(uint32_t hEnt) = 0;
    virtual void* GetClientUnknownFromHandle(uint32_t hEnt) = 0;
    virtual void* GetClientEntity(int entnum) = 0;
    virtual void* GetClientEntityFromHandle(uint32_t hEnt) = 0;
    virtual int NumberOfEntities(bool bIncludeNonNetworkable) = 0;
    virtual int GetHighestEntityIndex() = 0;
    virtual void SetMaxEntities(int maxents) = 0;
    virtual int GetMaxEntities() = 0;
};

}  // namespace ar::client
