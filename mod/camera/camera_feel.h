// Pure C++20 Mirror's Edge camera feel module (Task G3).
// Owner: Gonzalo Robles (mod/camera/).
// Input: ar::FaithState (and optional ar::FaithStepResult) per tick.
// Output: angle, position and FOV offsets. No engine dependencies.
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>

#include "ar/faith_movement.h"
#include "ar/units.h"

namespace ar::camera {

struct CameraConfig {
    // Roll smoothing and decay: roll(t + dt) = roll(t) * exp(-lambda * dt)
    float roll_decay_lambda = 8.5f;         // 1/s decay rate back to 0 deg
    float roll_approach_lambda = 10.0f;     // 1/s approach rate toward wallrun target roll
    float wallrun_roll_deg = 15.0f;         // fallback wallrun roll lean when FaithState::view_roll == 0
    float max_pitch_for_roll_deg = 85.0f;   // suppress roll when looking straight up/down (|pitch| >= 85 deg)

    // View bobbing while running/sprinting on ground
    float bob_min_speed_su = 40.0f;         // su/s threshold to start bobbing
    float bob_base_freq_hz = 1.65f;         // full left-right stride cycle frequency at base speed
    float bob_max_freq_hz = 2.65f;          // stride cycle frequency at full sprint
    float bob_vertical_amp_su = 1.25f;      // su vertical oscillation amplitude at max sprint
    float bob_lateral_amp_su = 0.85f;       // su lateral oscillation amplitude at max sprint
    float bob_pitch_amp_deg = 0.65f;        // deg pitch micro-bob at max sprint
    float bob_roll_amp_deg = 0.55f;         // deg roll micro-bob at max sprint
    float bob_smooth_lambda = 10.0f;        // 1/s bob envelope smoothing rate

    // Dynamic FOV boost with horizontal speed
    float fov_start_speed_su = UuToSu(me_reference::kSprintStartSpeedUu);  // ~157.48 su/s (400 uu/s)
    float fov_max_speed_su = UuToSu(me_reference::kSprintMaxSpeedUu);      // ~283.46 su/s (720 uu/s)
    float fov_max_sprint_offset_deg = 8.0f;   // deg added at full sprint speed
    float fov_max_fling_offset_deg = 12.0f;   // deg cap during high-speed portal flings
    float fov_smooth_lambda = 6.0f;           // 1/s exponential smoothing for FOV transitions

    // Landing shake (soft and hard landings)
    float landing_ref_speed_su = UuToSu(me_reference::kJumpZUu);  // ~248.03 su/s normal jump landing
    float landing_dip_su = 3.5f;              // base vertical compression dip (su) at reference landing speed
    float landing_max_dip_su = 10.0f;         // maximum vertical dip (su) on hard landings
    float landing_pitch_kick_deg = 4.5f;      // downward pitch kick (deg) at reference landing speed
    float landing_max_pitch_kick_deg = 12.0f; // maximum downward pitch kick (deg) on hard landings
    float landing_shake_freq_hz = 5.5f;       // damped oscillation frequency (Hz)
    float landing_soft_decay_lambda = 8.0f;   // 1/s decay rate for soft landings
    float landing_hard_decay_lambda = 4.0f;   // 1/s slower decay rate for hard landings

    // Skill Roll somersault camera motion
    float skill_roll_duration_sec = 0.65f;    // duration of the 360-deg forward pitch roll (s)
    float skill_roll_dip_su = 18.0f;          // downward Z dip at mid-roll (su)
    float skill_roll_side_tilt_deg = 6.0f;    // slight roll tilt during the somersault (deg)
};

// Trivially copyable camera state so client prediction can snapshot and rewind it alongside FaithState.
struct CameraState {
    float current_roll_deg = 0.0f;
    float bob_phase_rad = 0.0f;
    float bob_weight = 0.0f;
    float current_fov_offset_deg = 0.0f;

    // Damped landing shake state
    float shake_elapsed_sec = 0.0f;
    float shake_dip_amp_su = 0.0f;
    float shake_pitch_amp_deg = 0.0f;
    float shake_decay_lambda = 8.0f;
    bool shake_active = false;

    // Skill Roll 360 somersault state
    float skill_roll_elapsed_sec = 0.0f;
    bool skill_roll_active = false;

    FaithAnimState prev_anim = FaithAnimState::Idle;
    uint8_t reserved[2] = {};
};

static_assert(std::is_trivially_copyable_v<CameraState>,
              "CameraState must be trivially copyable for client prediction rewind/replay");

struct CameraOutput {
    Vec3 angle_offset;         // degrees (pitch, yaw, roll) to add to player view angles
    Vec3 position_offset;      // su (forward, left, up) offset in player view/world Z space
    float fov_offset_deg = 0.0f;  // degrees to add to base FOV
};

// Returns true when the player is looking nearly straight up or down (|pitch| >= threshold_deg),
// where camera roll becomes ill-defined (gimbal lock) and must not be applied.
constexpr bool IsLookingStraightUpOrDown(float pitch_deg, float threshold_deg = 85.0f) {
    const float abs_pitch = pitch_deg < 0.0f ? -pitch_deg : pitch_deg;
    return abs_pitch >= threshold_deg;
}

// Pure exponential roll decay required by G3:
//   roll(t + dt) = roll(t) * exp(-lambda * dt)
// Returns 0 if the player looks straight up or down.
inline float DecayRoll(float current_roll_deg, float dt, float lambda = 8.5f,
                       float view_pitch_deg = 0.0f, float max_pitch_deg = 85.0f) {
    if (IsLookingStraightUpOrDown(view_pitch_deg, max_pitch_deg)) {
        return 0.0f;
    }
    if (dt <= 0.0f || lambda <= 0.0f) {
        return current_roll_deg;
    }
    return current_roll_deg * std::exp(-lambda * dt);
}

class CameraController {
public:
    explicit constexpr CameraController(const CameraConfig& config = {}) : config_(config) {}

    constexpr const CameraConfig& Config() const { return config_; }
    constexpr void SetConfig(const CameraConfig& config) { config_ = config; }

    void Reset(CameraState& cam_state) const;

    // Advances camera state by dt seconds given the latest FaithState, optional FaithStepResult
    // events, and the player's current view pitch (degrees, +pitch looks down).
    CameraOutput Update(const FaithState& faith_state, float dt, CameraState& cam_state,
                        const FaithStepResult& step_result = {},
                        float view_pitch_deg = 0.0f) const;

private:
    CameraConfig config_{};
};

}  // namespace ar::camera
