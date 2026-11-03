// Contract: units, engine constants and Mirror's Edge reference values.
// Owner: Sebas. Do not edit in a feature branch; ask for a new contract version instead.
//
// Conventions shared by every module of the mod
//   * Space: Source world space. Right-handed, Z up, X forward, Y left. Units: inches (1 su = 2.54 cm).
//   * Angles: degrees, Source QAngle order (pitch, yaw, roll). +pitch looks DOWN.
//   * Time: seconds. Movement always receives the host's tick dt; never read a wall clock.
//   * Mirror's Edge values (uu, 1 uu ~= 1 cm, left-handed) convert with source = uu / 2.54 and Y mirrored
//     (aperture_runner::Ue3ToSourceVector). The 1.905 factor in LESSONS.md is Source *architecture*
//     scale and is NOT used for movement.
#pragma once

#include <cstdint>

#include "aperture_runner/math.h"

namespace ar {

using Vec3 = aperture_runner::Vec3;
using Mat3 = aperture_runner::Mat3;

inline constexpr float kSuPerUu = aperture_runner::kScaleUe3ToSource;  // 0.393700787
inline constexpr float kUuPerSu = aperture_runner::kScaleSourceToUe3;  // 2.54
inline constexpr float kSuPerMeter = aperture_runner::kSuPerMeter;     // 39.3700787

constexpr float UuToSu(float uu) { return uu * kSuPerUu; }

// Portal 1 (Steam app 400) build the mod targets. Signatures and offsets are only valid for it.
inline constexpr uint32_t kPortalBuildId = 19017868;

// Source player defaults (HL2 g_DefaultViewVectors). Portal is expected to use the same values;
// Z2 confirms them in build 19017868. The host passes the real values in FaithHostConfig, so
// faith_move must not hard-code these.
namespace source_defaults {
inline constexpr Vec3 kHullMins{-16.0f, -16.0f, 0.0f};
inline constexpr Vec3 kHullMaxs{16.0f, 16.0f, 72.0f};
inline constexpr Vec3 kDuckHullMins{-16.0f, -16.0f, 0.0f};
inline constexpr Vec3 kDuckHullMaxs{16.0f, 16.0f, 36.0f};
inline constexpr float kViewHeight = 64.0f;
inline constexpr float kDuckViewHeight = 28.0f;
inline constexpr float kStepHeight = 18.0f;          // sv_stepsize
inline constexpr float kTickInterval = 0.015f;       // 66.67 Hz
}  // namespace source_defaults

// Faith's class defaults read from Mirror's Edge TdGame.u by tnrjns/faith-runner (MIT), in uu.
// Starting points for tuning, not final values. "measured" = derived from gameplay, not a default.
namespace me_reference {
inline constexpr float kCollisionRadiusUu = 30.0f;
inline constexpr float kCollisionHalfHeightUu = 90.0f;
inline constexpr float kBaseEyeHeightUu = 76.0f;
inline constexpr float kMaxWallStepHeightUu = 35.0f;
inline constexpr float kSprintStartSpeedUu = 400.0f;     // SpeedMaxBaseVelocity: sprint begins above this
inline constexpr float kSprintMaxSpeedUu = 720.0f;       // GroundSpeed
inline constexpr float kAccelRateUu = 6144.0f;
inline constexpr float kJumpZUu = 630.0f;                // BaseJumpZ
inline constexpr float kJumpAddXYUu = 100.0f;
inline constexpr float kFaithGravityUu = 1641.0f;        // measured; world DefaultGravityZ is -800
inline constexpr float kWallrunMinSpeedUu = 200.0f;
inline constexpr float kWallrunInitialZUu = 170.0f;
inline constexpr float kWallrunHorizAccelUu = 820.0f;
inline constexpr float kWallrunHorizDecelUu = 500.0f;
inline constexpr float kWallrunMinWallHeightUu = 192.0f;
inline constexpr float kWallrunDurationSec = 1.2f;       // derived from the values above
inline constexpr float kHardLandingHeightUu = 530.0f;
inline constexpr float kSkillRollLandingHeightUu = 200.0f;
inline constexpr float kFallingUncontrolledHeightUu = 1000.0f;
inline constexpr float kSlideAbortSpeedUu = 250.0f;
inline constexpr float kSpringboardUu = 950.0f;
}  // namespace me_reference

}  // namespace ar
