// Per-tick bridge logic for both sides, free of engine types.
//
// HostBridge runs inside the Source plugin: it watches Faith's position (from the guest),
// detects portal-plane crossings and issues TeleportCommands with transported pose/velocity.
// GuestBridge runs inside the Mirror's Edge hook: it applies those commands to the pawn,
// decides whether a wallrun survives the portal, corrects roll and resolves Skill Rolls.
//
// The engine-specific parts are behind IGuestPawnAdapter; implementing it needs the SDK dump
// (class layouts, function addresses) of the user's own MirrorsEdge.exe.
#pragma once

#include <cstdint>

#include "aperture_runner/math.h"
#include "aperture_runner/protocol.h"

namespace aperture_runner {

inline constexpr uint32_t kMaxEventsPerTick = 4;

struct HostTickResult {
    bool teleport_issued = false;
    TeleportCommand teleport{};
    uint32_t event_count = 0;
    ApertureEventPacket events[kMaxEventsPerTick]{};
    Vec3 source_position;  // su, after any teleport
    Vec3 source_velocity;  // su/s, after any teleport
    bool waiting_for_ack = false;
};

class HostBridge {
public:
    explicit HostBridge(const ApertureTuning& tuning = {}) : tuning_(tuning.Sanitized()) {}

    HostTickResult Tick(const FaithKinematicState& faith, const PortalPairState& portals,
                        uint64_t tick);

    uint32_t last_issued_sequence() const { return issued_seq_; }

private:
    void PushEvent(HostTickResult& r, ApertureEventType type, Vec3 pos_su, float speed_mps,
                   const char* details, uint64_t tick) const;

    ApertureTuning tuning_;
    bool has_prev_ = false;
    Vec3 prev_src_pos_;
    uint32_t issued_seq_ = 0;
    uint64_t pending_since_tick_ = 0;
    bool pending_ = false;
};

// Engine hooks the guest must provide. All values in UE3 world space (uu, uu/s).
class IGuestPawnAdapter {
public:
    virtual ~IGuestPawnAdapter() = default;
    virtual void TeleportPawn(Vec3 position_uu, Vec3 velocity_uu, Quat camera) = 0;
    // Trace from origin along dir up to max_dist; on hit writes the surface normal.
    virtual bool TraceWall(Vec3 origin_uu, Vec3 dir, float max_dist_uu, Vec3* out_normal) = 0;
    virtual void ForceWallrun(Vec3 wall_normal_uu, bool wall_on_right) = 0;
    virtual void CancelWallrun() = 0;
    virtual void SetCameraRotation(Quat camera) = 0;
    virtual void SetVelocity(Vec3 velocity_uu) = 0;
    virtual void BeginHardLanding(float stagger_sec, float impact_speed_mps) = 0;
};

struct GuestTeleportResult {
    uint32_t acked_sequence = 0;
    bool wallrun_continued = false;
};

class GuestBridge {
public:
    explicit GuestBridge(const ApertureTuning& tuning = {}) : tuning_(tuning.Sanitized()) {}

    // Applies one teleport. current_state is the state that was active when the command
    // arrived (used to keep the wallrun side). Returns the sequence to write into
    // FaithKinematicState::last_applied_teleport_seq.
    GuestTeleportResult ApplyTeleport(const TeleportCommand& cmd, TdMovementState current_state,
                                      IGuestPawnAdapter& pawn);

    // Call every tick while airborne (not grounded, not wallrunning/climbing).
    Quat TickAirborneRoll(Quat camera, float dt) const;

    void OnCrouchPressed(double time_sec) { skill_roll_.OnCrouchPressed(time_sec); }

    // Call on the tick the pawn touches ground. Applies the outcome through the adapter and
    // fills out_event when the landing is worth reporting to the director.
    LandingResult OnLanded(double time_sec, Vec3 position_uu, Vec3 velocity_uu, Quat camera,
                           IGuestPawnAdapter& pawn, ApertureEventPacket* out_event,
                           bool* has_event, uint64_t tick);

    const ApertureTuning& tuning() const { return tuning_; }

private:
    ApertureTuning tuning_;
    SkillRollResolver skill_roll_;
};

}  // namespace aperture_runner
