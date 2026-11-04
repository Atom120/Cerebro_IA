#include "camera/faith_camera.h"

#include <algorithm>
#include <cmath>

namespace ar::camera {
namespace {

constexpr float kTwoPi = 6.28318530718f;

// Exponential smoothing that is exact for any dt: after time t the gap shrinks by exp(-lambda*t).
float Approach(float value, float target, float lambda, float dt) {
    return target + (value - target) * std::exp(-lambda * dt);
}

float RunSpeed() { return UuToSu(me_reference::kSprintStartSpeedUu); }
float SprintSpeed() { return UuToSu(me_reference::kSprintMaxSpeedUu); }

bool Running(const FaithState& f) {
    return f.on_ground && (f.anim == FaithAnimState::Walk || f.anim == FaithAnimState::Run ||
                           f.anim == FaithAnimState::Sprint || f.anim == FaithAnimState::CrouchWalk);
}

}  // namespace

CameraOutput UpdateCamera(CameraState& cam, const FaithState& faith, uint32_t events, float impact_speed, float dt,
                          const CameraParams& p) {
    CameraOutput out;

    // Roll toward the wallrun lean the movement asks for.
    cam.roll = Approach(cam.roll, faith.view_roll, p.roll_lambda, dt);
    out.roll = cam.roll;

    // FOV opens up between the sprint start and full sprint.
    const float sprint = std::clamp((faith.speed_2d - RunSpeed()) / (SprintSpeed() - RunSpeed()), 0.0f, 1.0f);
    cam.fov_extra = Approach(cam.fov_extra, faith.on_ground || faith.anim == FaithAnimState::WallRunLeft ||
                                                    faith.anim == FaithAnimState::WallRunRight
                                                ? sprint * p.fov_sprint_extra
                                                : cam.fov_extra,
                             p.fov_lambda, dt);
    out.fov = p.fov_base + cam.fov_extra;

    // Head bob: phase advances with distance, weight with speed; fades out in the air or standing.
    const float bob_target = Running(faith) ? std::clamp(faith.speed_2d / SprintSpeed(), 0.0f, 1.0f) : 0.0f;
    cam.bob_weight = Approach(cam.bob_weight, bob_target, p.bob_lambda, dt);
    cam.bob_phase = std::fmod(cam.bob_phase + faith.speed_2d * dt / p.bob_stride * kTwoPi, kTwoPi);
    out.eye_offset = {std::sin(cam.bob_phase) * p.bob_lateral * cam.bob_weight, 0.0f,
                      std::sin(2.0f * cam.bob_phase) * p.bob_vertical * cam.bob_weight};

    // Landing shake: amplitude from impact speed (a hard landing from HardLandingHeight is full strength).
    if (events & kFaithEventLanded) {
        const float hard_speed = std::sqrt(2.0f * UuToSu(me_reference::kFaithGravityUu) *
                                           UuToSu(me_reference::kHardLandingHeightUu));
        float amp = p.land_shake_pitch * std::clamp(impact_speed / hard_speed, 0.0f, 1.5f);
        if (events & kFaithEventHardLanding) amp *= 1.5f;
        if (amp > cam.shake_amp * (cam.shake_left / p.land_shake_sec)) {
            cam.shake_amp = amp;
            cam.shake_left = p.land_shake_sec;
        }
    }
    if (cam.shake_left > 0.0f) {
        const float t = p.land_shake_sec - cam.shake_left;
        const float fade = cam.shake_left / p.land_shake_sec;
        out.pitch_offset += cam.shake_amp * fade * std::sin(kTwoPi * p.land_shake_freq * t);
        cam.shake_left = std::max(0.0f, cam.shake_left - dt);
    }

    // Skill Roll: the view does one forward somersault (pitch 0 -> 360) over the roll.
    const float roll_sec = 0.6f;  // faith::kSkillRollSec; the camera keeps its own copy (no faith_move dependency)
    if (events & kFaithEventSkillRoll) cam.roll_left = roll_sec;
    if (cam.roll_left > 0.0f) {
        cam.roll_left = std::max(0.0f, cam.roll_left - dt);
        const float u = 1.0f - cam.roll_left / roll_sec;
        out.pitch_offset += 360.0f * u * u * (3.0f - 2.0f * u);  // smoothstep
        if (cam.roll_left == 0.0f) out.pitch_offset = 0.0f;
    }

    // 180 turn: spread the yaw change over turn180_sec, smoothstep, and hand out the delta per tick.
    if (events & kFaithEventTurn180) {
        cam.turn_left = p.turn180_sec;
        cam.turn_done = 0.0f;
    }
    if (cam.turn_left > 0.0f) {
        cam.turn_left = cam.turn_left - dt > 1e-4f ? cam.turn_left - dt : 0.0f;
        const float u = 1.0f - cam.turn_left / p.turn180_sec;
        const float target = 180.0f * u * u * (3.0f - 2.0f * u);
        out.yaw_delta = target - cam.turn_done;
        cam.turn_done = target;
    }
    return out;
}

}  // namespace ar::camera
