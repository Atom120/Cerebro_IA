#include "aperture_runner/bridge.h"

#include <cstdio>
#include <cstring>

namespace aperture_runner {
namespace {

bool IsWallrunning(TdMovementState s) {
    return s == TdMovementState::WallRunningLeft || s == TdMovementState::WallRunningRight;
}

void FillEvent(ApertureEventPacket& e, ApertureEventType type, Vec3 pos, float speed_mps,
               const char* details, uint64_t tick) {
    std::memset(&e, 0, sizeof(e));
    e.tick_counter = tick;
    e.event_type = static_cast<uint16_t>(type);
    e.speed_at_event = speed_mps;
    e.event_position = pos.ToFloat3();
    std::snprintf(e.details, sizeof(e.details), "%s", details ? details : "");
}

}  // namespace

void HostBridge::PushEvent(HostTickResult& r, ApertureEventType type, Vec3 pos_su, float speed_mps,
                           const char* details, uint64_t tick) const {
    if (r.event_count >= kMaxEventsPerTick) return;
    FillEvent(r.events[r.event_count++], type, pos_su, speed_mps, details, tick);
}

HostTickResult HostBridge::Tick(const FaithKinematicState& faith, const PortalPairState& portals,
                                uint64_t tick) {
    HostTickResult r;
    const WorldCalibration calib = WorldCalibration::FromData(portals.calibration);
    const Vec3 cur = Ue3ToSourcePoint(Vec3(faith.position), calib);
    const Vec3 vel = Ue3ToSourceVector(Vec3(faith.velocity));
    r.source_position = cur;
    r.source_velocity = vel;

    // While a teleport is in flight, the guest still reports its pre-teleport pose; running
    // crossing detection on it would re-trigger the same portal.
    if (pending_) {
        const bool acked = faith.last_applied_teleport_seq == issued_seq_;
        const bool timed_out = tick - pending_since_tick_ > tuning_.teleport_ack_timeout_ticks;
        if (!acked && !timed_out) {
            r.waiting_for_ack = true;
            return r;
        }
        pending_ = false;
        has_prev_ = false;
    }

    if (!has_prev_) {
        prev_src_pos_ = cur;
        has_prev_ = true;
        return r;
    }

    const bool pair_ready = portals.blue.active && portals.orange.active &&
                            portals.blue.linked && portals.orange.linked;
    if (pair_ready) {
        const PortalDescriptor* sides[2] = {&portals.blue, &portals.orange};
        for (int i = 0; i < 2; ++i) {
            const Mat4 m_in = Mat4::FromMatrix4(sides[i]->world_matrix);
            const PortalCrossing crossing = DetectPortalCrossing(prev_src_pos_, cur, m_in);
            if (!crossing.crossed) continue;

            const Mat4 m_out = Mat4::FromMatrix4(sides[1 - i]->world_matrix);
            const PortalTransform pt = ComputePortalTransform(m_in, m_out);

            const Vec3 new_pos = TransformPoint(pt.transform, cur);
            const float fling_cap_su = tuning_.portal_fling_max_speed_mps * kSuPerMeter;
            const Vec3 new_vel = ClampLength(pt.rotation * vel, fling_cap_su);

            const Quat cam_src = MirrorYQuat(Quat(faith.camera_rotation));
            const Quat new_cam_src = Normalized(Mat3ToQuat(pt.rotation) * cam_src);

            TeleportCommand& cmd = r.teleport;
            std::memset(&cmd, 0, sizeof(cmd));
            issued_seq_ = issued_seq_ + 1 == 0 ? 1 : issued_seq_ + 1;
            cmd.sequence_id = issued_seq_;
            cmd.entered_portal = static_cast<uint8_t>(i);
            cmd.new_position = SourceToUe3Point(new_pos, calib).ToFloat3();
            cmd.new_velocity = SourceToUe3Vector(new_vel).ToFloat3();
            cmd.new_camera_rotation = MirrorYQuat(new_cam_src).ToFloat4();

            const auto state = static_cast<TdMovementState>(faith.movement_state);
            const float exit_speed_mps = Length(new_vel) / kSuPerMeter;
            if (IsWallrunning(state)) {
                Vec3 expected;
                if (ExpectedExitWallNormal(pt, Vec3(faith.wall_normal),
                                           tuning_.wallrun_portal_tolerance_deg, &expected)) {
                    cmd.preserve_wallrun = 1;
                    cmd.expected_wall_normal = expected.ToFloat3();
                    PushEvent(r, ApertureEventType::WallrunThroughPortal, new_pos, exit_speed_mps,
                              i == 0 ? "blue->orange" : "orange->blue", tick);
                }
            }
            PushEvent(r, ApertureEventType::PortalTeleport, new_pos, exit_speed_mps,
                      i == 0 ? "blue->orange" : "orange->blue", tick);
            if (exit_speed_mps >= tuning_.fling_event_min_speed_mps) {
                char details[kEventDetailsLen];
                std::snprintf(details, sizeof(details), "exit %.1f m/s", exit_speed_mps);
                PushEvent(r, ApertureEventType::PortalFlingExecuted, new_pos, exit_speed_mps,
                          details, tick);
            }

            r.teleport_issued = true;
            r.source_position = new_pos;
            r.source_velocity = new_vel;
            pending_ = true;
            pending_since_tick_ = tick;
            has_prev_ = false;
            return r;
        }
    }

    prev_src_pos_ = cur;
    return r;
}

GuestTeleportResult GuestBridge::ApplyTeleport(const TeleportCommand& cmd,
                                               TdMovementState current_state,
                                               IGuestPawnAdapter& pawn) {
    GuestTeleportResult result;
    const Vec3 pos(cmd.new_position);
    const Vec3 vel(cmd.new_velocity);
    const Quat cam = Normalized(Quat(cmd.new_camera_rotation));
    pawn.TeleportPawn(pos, vel, cam);

    if (cmd.preserve_wallrun && IsWallrunning(current_state)) {
        const Vec3 expected = Normalized(Vec3(cmd.expected_wall_normal));
        Vec3 found;
        // The wall is behind its normal: trace from Faith into the wall.
        if (pawn.TraceWall(pos, -expected, tuning_.wallrun_reacquire_distance_uu, &found) &&
            WallMatchesExpected(found, expected, tuning_.wallrun_portal_tolerance_deg)) {
            // A portal transform is a proper rotation, so the wall stays on the same side.
            pawn.ForceWallrun(Normalized(found), current_state == TdMovementState::WallRunningRight);
            result.wallrun_continued = true;
        } else {
            pawn.CancelWallrun();
        }
    } else if (IsWallrunning(current_state)) {
        pawn.CancelWallrun();
    }

    result.acked_sequence = cmd.sequence_id;
    return result;
}

Quat GuestBridge::TickAirborneRoll(Quat camera, float dt) const {
    return DecayCameraRoll(camera, dt, tuning_.roll_decay_lambda);
}

LandingResult GuestBridge::OnLanded(double time_sec, Vec3 position_uu, Vec3 velocity_uu,
                                    Quat camera, IGuestPawnAdapter& pawn,
                                    ApertureEventPacket* out_event, bool* has_event,
                                    uint64_t tick) {
    const Vec3 look = Rotate(camera, {1, 0, 0});
    const LandingResult r = skill_roll_.OnLanded(time_sec, velocity_uu, look, tuning_, kUuPerMeter);
    *has_event = false;
    char details[kEventDetailsLen];
    switch (r.outcome) {
        case LandingOutcome::Soft:
            break;
        case LandingOutcome::SkillRoll:
            pawn.SetVelocity(r.velocity);
            std::snprintf(details, sizeof(details), "impact %.1f m/s", r.impact_speed_mps);
            FillEvent(*out_event, ApertureEventType::SkillRollSucceeded, position_uu,
                      Length(r.velocity) / kUuPerMeter, details, tick);
            *has_event = true;
            break;
        case LandingOutcome::HardLanding:
            pawn.SetVelocity(r.velocity);
            pawn.BeginHardLanding(r.stagger_sec, r.impact_speed_mps);
            std::snprintf(details, sizeof(details), "impact %.1f m/s, no roll", r.impact_speed_mps);
            FillEvent(*out_event, ApertureEventType::SkillRollFailed, position_uu,
                      r.impact_speed_mps, details, tick);
            *has_event = true;
            break;
    }
    return r;
}

}  // namespace aperture_runner
