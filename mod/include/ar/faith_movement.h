// Contract: the Faith movement library (mod/faith_move, Peña) and its hosts (server hook, Zarek;
// client prediction, Gonzalo). Camera (Gonzalo) and render (Gonzalo/Antonio) read FaithState.
// Owner: Sebas. Any change bumps kFaithMovementContractVersion.
//
// Model
//   The library is stateless. All state that must survive between ticks lives in FaithState, a
//   trivially copyable struct the host stores per player. Source prediction rewinds and replays
//   ticks on the client, so the host must be able to copy, restore and re-run a FaithState and get
//   bit-identical results: no globals, no wall clock, no randomness inside Step.
//
//   Each tick the host fills FaithInput, calls Step and then writes FaithState::origin, velocity and
//   ducked back into the player. Portal teleports are still done by the engine (prop_portal touch);
//   the host reports them through FaithInput::portal so the library can rotate its own vectors
//   (wall normal, wallrun direction) and keep a wallrun going through a portal.
#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>

#include "ar/trace_provider.h"
#include "ar/units.h"

namespace ar {

inline constexpr uint32_t kFaithMovementContractVersion = 1;

// Semantic buttons. The host maps engine input to these (proposal, confirmed in G1):
//   Up <- IN_JUMP, Down <- IN_DUCK, Turn180 <- IN_RELOAD, Walk <- IN_WALK. IN_USE stays with the engine.
enum FaithButton : uint32_t {
    kFaithButtonUp = 1u << 0,       // jump, wallrun, wallclimb, climb up a ledge
    kFaithButtonDown = 1u << 1,     // crouch, slide, coil in the air, Skill Roll on landing
    kFaithButtonTurn180 = 1u << 2,  // quick turn (and turn-off-wall during a wallclimb)
    kFaithButtonWalk = 1u << 3,     // walk slowly, never sprint
};

// Animation-facing state. One value per tick. Its names are also the $sequence names in the QC
// (SequenceName below) and the keys of docs/contratos/asset_manifest.json.
enum class FaithAnimState : uint8_t {
    Idle = 0,
    Walk,
    Run,
    Sprint,
    Crouch,
    CrouchWalk,
    Jump,
    Fall,
    FallUncontrolled,
    Coil,
    LandSoft,
    LandHard,
    SkillRoll,
    Slide,
    WallRunLeft,
    WallRunRight,
    WallRunJump,
    WallClimb,
    WallClimbTurn,    // 180 turn at the top of a wallclimb
    WallClimbJump,    // jump off after WallClimbTurn
    LedgeHang,
    LedgeClimbUp,
    Vault,
    Turn180,
    Count
};

constexpr const char* SequenceName(FaithAnimState s) {
    switch (s) {
        case FaithAnimState::Idle: return "idle";
        case FaithAnimState::Walk: return "walk";
        case FaithAnimState::Run: return "run";
        case FaithAnimState::Sprint: return "sprint";
        case FaithAnimState::Crouch: return "crouch";
        case FaithAnimState::CrouchWalk: return "crouch_walk";
        case FaithAnimState::Jump: return "jump";
        case FaithAnimState::Fall: return "fall";
        case FaithAnimState::FallUncontrolled: return "fall_uncontrolled";
        case FaithAnimState::Coil: return "coil";
        case FaithAnimState::LandSoft: return "land_soft";
        case FaithAnimState::LandHard: return "land_hard";
        case FaithAnimState::SkillRoll: return "skill_roll";
        case FaithAnimState::Slide: return "slide";
        case FaithAnimState::WallRunLeft: return "wallrun_left";
        case FaithAnimState::WallRunRight: return "wallrun_right";
        case FaithAnimState::WallRunJump: return "wallrun_jump";
        case FaithAnimState::WallClimb: return "wallclimb";
        case FaithAnimState::WallClimbTurn: return "wallclimb_turn";
        case FaithAnimState::WallClimbJump: return "wallclimb_jump";
        case FaithAnimState::LedgeHang: return "ledge_hang";
        case FaithAnimState::LedgeClimbUp: return "ledge_climb_up";
        case FaithAnimState::Vault: return "vault";
        case FaithAnimState::Turn180: return "turn_180";
        case FaithAnimState::Count: break;
    }
    return nullptr;
}

// One-tick events for sound, camera shake and HUD. Bitmask in FaithStepResult::events.
enum FaithEvent : uint32_t {
    kFaithEventJumped = 1u << 0,
    kFaithEventLanded = 1u << 1,         // any landing; see impact_speed
    kFaithEventHardLanding = 1u << 2,
    kFaithEventSkillRoll = 1u << 3,
    kFaithEventWallRunStart = 1u << 4,
    kFaithEventWallRunEnd = 1u << 5,
    kFaithEventWallClimbStart = 1u << 6,
    kFaithEventLedgeGrab = 1u << 7,
    kFaithEventSlideStart = 1u << 8,
    kFaithEventPortalContinued = 1u << 9,  // a wallrun/slide survived a portal teleport
};

// Fixed per session; the host reads the real values from the engine.
struct FaithHostConfig {
    Vec3 hull_mins = source_defaults::kHullMins;
    Vec3 hull_maxs = source_defaults::kHullMaxs;
    Vec3 duck_hull_mins = source_defaults::kDuckHullMins;
    Vec3 duck_hull_maxs = source_defaults::kDuckHullMaxs;
    float view_height = source_defaults::kViewHeight;
    float duck_view_height = source_defaults::kDuckViewHeight;
    float step_height = source_defaults::kStepHeight;
};

// Engine teleport through a portal that happened since the previous Step.
struct PortalTransportHint {
    bool teleported = false;
    Mat3 rotation;  // Omega in->out (aperture_runner::PortalTransform::rotation), Source space
};

struct FaithInput {
    float dt = source_defaults::kTickInterval;  // seconds, host frametime for this tick
    double time = 0.0;                          // host curtime, seconds (monotonic per session)
    float forward_move = 0.0f;                  // -1..1, +1 = forward
    float side_move = 0.0f;                     // -1..1, +1 = right
    Vec3 view_angles;                           // degrees (pitch, yaw, roll), +pitch looks down
    uint32_t buttons = 0;                       // FaithButton bits held this tick
    uint32_t buttons_pressed = 0;               // FaithButton bits that went down this tick
    PortalTransportHint portal;
};

// Everything that must survive between ticks. Trivially copyable, fixed size, no pointers.
struct FaithState {
    Vec3 origin;      // feet position (Source player origin)
    Vec3 velocity;    // su/s
    bool on_ground = false;
    bool ducked = false;  // host must switch to the duck hull and set FL_DUCKING
    FaithAnimState anim = FaithAnimState::Idle;
    uint8_t reserved0 = 0;
    Vec3 ground_normal{0.0f, 0.0f, 1.0f};
    Vec3 wall_normal;          // current wallrun/wallclimb/ledge wall, zero if none
    float state_time = 0.0f;   // seconds since `anim` last changed
    float speed_2d = 0.0f;     // |velocity.xy|, for camera and animation playback rate
    float view_roll = 0.0f;    // degrees the camera should roll (wallrun lean); camera module smooths it
    float eye_height = source_defaults::kViewHeight;  // current eye height above origin
    // Library-private data that must also be predicted (timers, wallrun direction, coyote time...).
    // faith_move owns its layout; nobody else may read or write it.
    uint8_t internal[128] = {};
};

static_assert(std::is_trivially_copyable_v<FaithState>, "FaithState must be copyable for prediction");
static_assert(sizeof(FaithState) == 196, "FaithState layout changed: bump kFaithMovementContractVersion");

struct FaithStepResult {
    uint32_t events = 0;        // FaithEvent bits
    float impact_speed = 0.0f;  // su/s downward at the landing, when kFaithEventLanded is set
};

class IFaithMovement {
public:
    virtual ~IFaithMovement() = default;

    // Places Faith at origin with velocity, clearing every timer (map start, respawn, ar_enable 1).
    virtual void Reset(FaithState& state, Vec3 origin, Vec3 velocity) const = 0;

    // Advances one tick. Must be deterministic for (input, state, world).
    virtual FaithStepResult Step(const FaithInput& input, FaithState& state,
                                 const ITraceProvider& trace) const = 0;
};

// Defined in mod/faith_move (P1). Hosts create one instance per session.
std::unique_ptr<IFaithMovement> CreateFaithMovement(const FaithHostConfig& config);

}  // namespace ar
