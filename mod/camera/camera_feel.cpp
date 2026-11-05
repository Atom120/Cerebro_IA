// Implementation of the pure C++20 Mirror's Edge camera feel module (Task G3).
// Owner: Gonzalo Robles (mod/camera/).
#include "camera_feel.h"

#include <algorithm>
#include <cmath>

namespace ar::camera {

namespace {

constexpr float kTwoPi = 2.0f * aperture_runner::kPi;

constexpr bool IsRunningGroundAnim(FaithAnimState anim) {
    return anim == FaithAnimState::Walk || anim == FaithAnimState::Run ||
           anim == FaithAnimState::Sprint || anim == FaithAnimState::CrouchWalk;
}

float ComputeHorizontalSpeed(const FaithState& s) {
    if (s.speed_2d > 0.0f) {
        return s.speed_2d;
    }
    return std::sqrt(s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y);
}

float ComputeTargetRollDeg(const FaithState& s, const CameraConfig& cfg) {
    if (std::fabs(s.view_roll) > 1e-4f) {
        return s.view_roll;
    }
    if (s.anim == FaithAnimState::WallRunLeft) {
        return cfg.wallrun_roll_deg;
    }
    if (s.anim == FaithAnimState::WallRunRight) {
        return -cfg.wallrun_roll_deg;
    }
    return 0.0f;
}

float ComputeTargetFovOffsetDeg(float speed_2d, const CameraConfig& cfg) {
    if (speed_2d <= cfg.fov_start_speed_su) {
        return 0.0f;
    }
    const float span = std::max(cfg.fov_max_speed_su - cfg.fov_start_speed_su, 1.0f);
    const float normalized = (speed_2d - cfg.fov_start_speed_su) / span;
    const float raw_offset = normalized * cfg.fov_max_sprint_offset_deg;
    return std::clamp(raw_offset, 0.0f, cfg.fov_max_fling_offset_deg);
}

}  // namespace

void CameraController::Reset(CameraState& cam_state) const {
    cam_state = CameraState{};
}

CameraOutput CameraController::Update(const FaithState& faith_state, float dt,
                                      CameraState& cam_state,
                                      const FaithStepResult& step_result,
                                      float view_pitch_deg) const {
    CameraOutput out{};
    if (dt < 0.0f) {
        dt = 0.0f;
    }

    const float speed_2d = ComputeHorizontalSpeed(faith_state);

    // -------------------------------------------------------------------------
    // 1. Camera Roll: wallrun lean and exponential decay roll(t+dt) = roll(t)*exp(-lambda*dt)
    //    Suppressed completely when looking straight up or down (|pitch| >= max_pitch_for_roll_deg).
    // -------------------------------------------------------------------------
    if (IsLookingStraightUpOrDown(view_pitch_deg, config_.max_pitch_for_roll_deg)) {
        cam_state.current_roll_deg = 0.0f;
    } else {
        const float target_roll = ComputeTargetRollDeg(faith_state, config_);
        if (std::fabs(target_roll) < 1e-4f) {
            cam_state.current_roll_deg =
                DecayRoll(cam_state.current_roll_deg, dt, config_.roll_decay_lambda,
                          view_pitch_deg, config_.max_pitch_for_roll_deg);
        } else {
            const float decay = std::exp(-config_.roll_approach_lambda * dt);
            cam_state.current_roll_deg =
                target_roll + (cam_state.current_roll_deg - target_roll) * decay;
        }
    }
    out.angle_offset.z += cam_state.current_roll_deg;

    // -------------------------------------------------------------------------
    // 2. View Bobbing: harmonic stride bobbing while running/sprinting on ground
    // -------------------------------------------------------------------------
    const bool bob_eligible =
        faith_state.on_ground && IsRunningGroundAnim(faith_state.anim) &&
        speed_2d >= config_.bob_min_speed_su;

    if (bob_eligible && dt > 0.0f) {
        const float speed_ratio = std::clamp(
            (speed_2d - config_.bob_min_speed_su) /
                std::max(config_.fov_max_speed_su - config_.bob_min_speed_su, 1.0f),
            0.0f, 1.25f);
        const float target_weight = std::clamp(0.25f + 0.75f * speed_ratio, 0.0f, 1.25f);
        const float env_decay = std::exp(-config_.bob_smooth_lambda * dt);
        cam_state.bob_weight = target_weight + (cam_state.bob_weight - target_weight) * env_decay;

        const float freq_hz =
            config_.bob_base_freq_hz +
            (config_.bob_max_freq_hz - config_.bob_base_freq_hz) * std::min(speed_ratio, 1.0f);
        cam_state.bob_phase_rad += kTwoPi * freq_hz * dt;
        if (cam_state.bob_phase_rad >= kTwoPi) {
            cam_state.bob_phase_rad = std::fmod(cam_state.bob_phase_rad, kTwoPi);
        }
    } else {
        cam_state.bob_weight *= std::exp(-config_.bob_smooth_lambda * dt);
        if (cam_state.bob_weight < 1e-4f) {
            cam_state.bob_weight = 0.0f;
        }
    }

    if (cam_state.bob_weight > 0.0f) {
        const float phase = cam_state.bob_phase_rad;
        const float w = cam_state.bob_weight;
        // Vertical bob oscillates at 2x stride frequency (one dip per left/right footstep).
        out.position_offset.z += config_.bob_vertical_amp_su * w * std::sin(2.0f * phase);
        // Lateral bob oscillates at 1x stride frequency (sway between left and right foot).
        out.position_offset.y += config_.bob_lateral_amp_su * w * std::cos(phase);
        // Subtle angular head bobbing
        out.angle_offset.x += config_.bob_pitch_amp_deg * w * std::sin(2.0f * phase);
        if (!IsLookingStraightUpOrDown(view_pitch_deg, config_.max_pitch_for_roll_deg)) {
            out.angle_offset.z += config_.bob_roll_amp_deg * w * std::sin(phase);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Dynamic FOV: rises with horizontal speed above sprint threshold
    // -------------------------------------------------------------------------
    const float target_fov = ComputeTargetFovOffsetDeg(speed_2d, config_);
    const float fov_decay = std::exp(-config_.fov_smooth_lambda * dt);
    cam_state.current_fov_offset_deg =
        target_fov + (cam_state.current_fov_offset_deg - target_fov) * fov_decay;
    if (std::fabs(cam_state.current_fov_offset_deg) < 1e-4f && target_fov == 0.0f) {
        cam_state.current_fov_offset_deg = 0.0f;
    }
    out.fov_offset_deg = cam_state.current_fov_offset_deg;

    // -------------------------------------------------------------------------
    // 4. Camera Shake: Landing impulse (soft / hard) and Skill Roll somersault
    // -------------------------------------------------------------------------
    const bool triggered_skill_roll =
        (step_result.events & kFaithEventSkillRoll) != 0u ||
        (faith_state.anim == FaithAnimState::SkillRoll &&
         cam_state.prev_anim != FaithAnimState::SkillRoll);

    if (triggered_skill_roll) {
        cam_state.skill_roll_active = true;
        cam_state.skill_roll_elapsed_sec = 0.0f;
        cam_state.shake_active = false;
    }

    const bool triggered_hard_land =
        (step_result.events & kFaithEventHardLanding) != 0u ||
        (faith_state.anim == FaithAnimState::LandHard &&
         cam_state.prev_anim != FaithAnimState::LandHard);

    const bool triggered_soft_land =
        !triggered_skill_roll && !triggered_hard_land &&
        ((step_result.events & kFaithEventLanded) != 0u ||
         (faith_state.anim == FaithAnimState::LandSoft &&
          cam_state.prev_anim != FaithAnimState::LandSoft));

    if (triggered_hard_land || triggered_soft_land) {
        const float raw_impact =
            step_result.impact_speed > 0.0f
                ? step_result.impact_speed
                : std::max(-faith_state.velocity.z, config_.landing_ref_speed_su);
        const float impact_scale = std::clamp(
            raw_impact / std::max(config_.landing_ref_speed_su, 1.0f), 0.35f, 3.0f);
        const float hard_mult = triggered_hard_land ? 1.85f : 1.0f;

        cam_state.shake_active = true;
        cam_state.shake_elapsed_sec = 0.0f;
        cam_state.shake_dip_amp_su = std::min(
            config_.landing_dip_su * impact_scale * hard_mult, config_.landing_max_dip_su);
        cam_state.shake_pitch_amp_deg = std::min(
            config_.landing_pitch_kick_deg * impact_scale * hard_mult,
            config_.landing_max_pitch_kick_deg);
        cam_state.shake_decay_lambda = triggered_hard_land
                                           ? config_.landing_hard_decay_lambda
                                           : config_.landing_soft_decay_lambda;
    }

    if (cam_state.shake_active) {
        cam_state.shake_elapsed_sec += dt;
        const float t = cam_state.shake_elapsed_sec;
        const float envelope = std::exp(-cam_state.shake_decay_lambda * t);
        if (envelope < 1e-3f) {
            cam_state.shake_active = false;
            cam_state.shake_elapsed_sec = 0.0f;
        } else {
            const float wave = std::sin(kTwoPi * config_.landing_shake_freq_hz * t);
            // Compression dip is downward (-Z) and pitch kick looks downward (+pitch in Source).
            out.position_offset.z -= cam_state.shake_dip_amp_su * envelope * std::fabs(wave);
            out.angle_offset.x += cam_state.shake_pitch_amp_deg * envelope * wave;
        }
    }

    if (cam_state.skill_roll_active) {
        cam_state.skill_roll_elapsed_sec += dt;
        const float dur = std::max(config_.skill_roll_duration_sec, 0.1f);
        const float u = cam_state.skill_roll_elapsed_sec / dur;
        if (u >= 1.0f) {
            cam_state.skill_roll_active = false;
            cam_state.skill_roll_elapsed_sec = 0.0f;
        } else {
            // Smooth cosine-eased 0 -> 360 deg forward somersault (+pitch is looking down/forward).
            const float progress = 0.5f * (1.0f - std::cos(aperture_runner::kPi * u));
            const float somersault_pitch = 360.0f * progress;
            const float arch = std::sin(aperture_runner::kPi * u);
            out.angle_offset.x += somersault_pitch;
            out.position_offset.z -= config_.skill_roll_dip_su * arch;
            if (!IsLookingStraightUpOrDown(view_pitch_deg, config_.max_pitch_for_roll_deg)) {
                out.angle_offset.z += config_.skill_roll_side_tilt_deg * arch;
            }
        }
    }

    cam_state.prev_anim = faith_state.anim;
    return out;
}

}  // namespace ar::camera
