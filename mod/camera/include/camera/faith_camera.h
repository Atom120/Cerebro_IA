// Mirror's Edge style first-person camera (G3). Owner: Gonzalo (mod/camera); written by Sebas.
// Pure C++, no engine: the host feeds FaithState + the tick's events and applies the output to the
// view (roll, pitch offset, eye offset, FOV, yaw delta for the 180 turn).
//
// Deterministic and trivially copyable like FaithState, so the client can predict it if needed.
#pragma once

#include <cstdint>
#include <type_traits>

#include "ar/faith_movement.h"

namespace ar::camera {

struct CameraParams {
    float roll_lambda = 8.0f;         // 1/s: roll follows FaithState::view_roll with exp(-lambda*dt)
    float fov_base = 75.0f;           // degrees (Portal default)
    float fov_sprint_extra = 10.0f;   // degrees added at full sprint
    float fov_lambda = 4.0f;          // 1/s
    float bob_stride = 64.0f;         // su travelled per full step cycle
    float bob_vertical = 1.2f;        // su at full sprint
    float bob_lateral = 0.6f;         // su at full sprint
    float bob_lambda = 10.0f;         // 1/s: how fast the bob fades in/out
    float land_shake_pitch = 4.0f;    // degrees at kHardLandingHeight impact speed
    float land_shake_sec = 0.35f;
    float land_shake_freq = 18.0f;    // Hz
    float turn180_sec = 0.3f;
};

struct CameraState {
    float roll = 0.0f;         // degrees
    float fov_extra = 0.0f;    // degrees
    float bob_phase = 0.0f;    // radians
    float bob_weight = 0.0f;   // 0..1
    float shake_left = 0.0f;   // seconds
    float shake_amp = 0.0f;    // degrees
    float roll_left = 0.0f;    // Skill Roll seconds left
    float turn_done = 0.0f;    // degrees of the current 180 turn already applied
    float turn_left = 0.0f;    // seconds
};
static_assert(std::is_trivially_copyable_v<CameraState>);

struct CameraOutput {
    float roll = 0.0f;          // degrees, add to view roll
    float pitch_offset = 0.0f;  // degrees, add to view pitch (+ looks down): landing shake, Skill Roll
    float yaw_delta = 0.0f;     // degrees to add to the view yaw THIS tick (180 turn spread over turn180_sec)
    Vec3 eye_offset;            // su, view-space-free offset: x/y lateral bob, z vertical bob
    float fov = 75.0f;          // degrees
};

// impact_speed: FaithStepResult::impact_speed (su/s) when kFaithEventLanded is set.
CameraOutput UpdateCamera(CameraState& cam, const FaithState& faith, uint32_t events, float impact_speed, float dt,
                          const CameraParams& params = CameraParams{});

}  // namespace ar::camera
