// Tuning values for Faith's movement, in Source units (su, su/s, su/s^2) and seconds.
// Owner: Peña (mod/faith_move). Values marked "placeholder" have no Mirror's Edge source yet;
// P5 replaces them with values fitted to real ME recordings (qa/telemetria/*_live.csv).
#pragma once

#include "ar/units.h"

namespace ar::faith {

// Ground (P2). Section 6 of PLAN_EQUIPO.md.
inline constexpr float kWalkSpeed = UuToSu(150.0f);                         // placeholder (59 su/s)
inline constexpr float kRunSpeed = UuToSu(me_reference::kSprintStartSpeedUu);  // 157.5: sprint starts above
inline constexpr float kSprintMaxSpeed = UuToSu(me_reference::kSprintMaxSpeedUu);  // 283.5
inline constexpr float kGroundAccel = UuToSu(me_reference::kAccelRateUu);   // 2419 su/s^2, up to kRunSpeed
inline constexpr float kGroundDecel = kGroundAccel;                         // placeholder: stop/turn braking
inline constexpr float kSprintRampSec = 1.5f;                               // placeholder: 157.5 -> 283.5
inline constexpr float kSprintAccel = (kSprintMaxSpeed - kRunSpeed) / kSprintRampSec;
inline constexpr float kSprintMinForwardDot = 0.7f;  // wish direction must be this close to the view

// Air (P2).
inline constexpr float kJumpSpeed = UuToSu(me_reference::kJumpZUu);         // 248 su/s
inline constexpr float kJumpAddXY = UuToSu(me_reference::kJumpAddXYUu);     // 39.4 su/s along the run
inline constexpr float kGravity = UuToSu(me_reference::kFaithGravityUu);    // 646 su/s^2
inline constexpr float kAirAccel = 360.0f;                                  // placeholder: steering only
inline constexpr float kCoyoteTimeSec = 0.1f;                               // placeholder

// Landing (P2).
inline constexpr float kHardLandingHeight = UuToSu(me_reference::kHardLandingHeightUu);  // 208.7 su
inline constexpr float kUncontrolledFallHeight = UuToSu(me_reference::kFallingUncontrolledHeightUu);  // 393.7
inline constexpr float kHardLandingLockSec = 0.5f;          // placeholder: no input after a hard landing
inline constexpr float kHardLandingMaxSpeed = kWalkSpeed;   // placeholder: speed kept after it
inline constexpr float kSoftLandingAnimSec = 0.25f;         // placeholder
inline constexpr float kMinLandingAnimHeight = 8.0f;        // smaller drops (steps, spawn) play no anim

// Collision.
inline constexpr float kWalkableNormalZ = 0.7f;  // Source: surfaces flatter than ~45 degrees are floor
inline constexpr float kGroundProbe = 2.0f;      // how far below the feet counts as standing
inline constexpr float kSkin = 0.03125f;         // gap kept from surfaces after a hit (Source DIST_EPSILON-ish)

}  // namespace ar::faith
