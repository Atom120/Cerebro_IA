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

// Crouch, slide, coil and Skill Roll (P3).
inline constexpr float kCrouchSpeed = 63.0f;                    // placeholder
inline constexpr float kSlideMinSpeed = kRunSpeed - 1.0f;       // slide only from a run or sprint
inline constexpr float kSlideDecel = 120.0f;                    // placeholder
inline constexpr float kSlideAbortSpeed = UuToSu(me_reference::kSlideAbortSpeedUu);  // 98.4
inline constexpr float kCoilMaxSec = 0.8f;                      // placeholder: coil un-tucks after this
inline constexpr float kSkillRollHeight = UuToSu(me_reference::kSkillRollLandingHeightUu);  // 78.7
inline constexpr float kRollWindowSec = 0.35f;  // placeholder: Down pressed this long before landing rolls
inline constexpr float kSkillRollSec = 0.6f;    // placeholder
inline constexpr float kTurn180Sec = 0.3f;      // placeholder

// Walls (P3).
inline constexpr float kWallReach = 16.0f;                  // how far beside/ahead a wall still counts
inline constexpr float kWallStick = 5.0f;                   // su/s pushed into the wall to keep contact
inline constexpr float kWallMaxNormalZ = 0.3f;              // steeper than this is not a wall
inline constexpr float kWallrunMinSpeed = UuToSu(me_reference::kWallrunMinSpeedUu);        // 78.7
inline constexpr float kWallrunInitialZ = UuToSu(me_reference::kWallrunInitialZUu);        // 66.9
inline constexpr float kWallrunGroundStartZ = 150.0f;       // placeholder: wallrun started from the floor
inline constexpr float kWallrunAccel = UuToSu(me_reference::kWallrunHorizAccelUu);         // 322.8, forward held
inline constexpr float kWallrunDecel = UuToSu(me_reference::kWallrunHorizDecelUu);         // 196.9, released
inline constexpr float kWallrunGravityScale = 0.28f;        // placeholder
inline constexpr float kWallrunDurationSec = me_reference::kWallrunDurationSec;            // 1.2
inline constexpr float kWallrunMinWallHeight = UuToSu(me_reference::kWallrunMinWallHeightUu);  // 75.6
inline constexpr float kWallrunMaxIntoDot = 0.8f;           // running more into the wall than this climbs instead
inline constexpr float kWallrunMaxStartFallSpeed = 150.0f;  // placeholder
inline constexpr float kWallrunRollDeg = 15.0f;             // placeholder: camera leans away from the wall
inline constexpr float kWallrunJumpOut = 150.0f;            // placeholder: push away from the wall
inline constexpr float kWallClimbReach = 20.0f;
inline constexpr float kWallClimbMinFacingDot = 0.8f;       // ~37 degrees from head-on
inline constexpr float kWallClimbSpeed = 260.0f;            // placeholder
inline constexpr float kWallClimbGravityScale = 0.6f;       // placeholder: ~87 su of climb
inline constexpr float kWallClimbEndSpeed = -60.0f;         // falling faster than this lets go
inline constexpr float kWallClimbTurnHoldSec = 1.0f;        // placeholder
inline constexpr float kWallClimbJumpOut = 200.0f;          // placeholder

// Ledges and vault (P3).
inline constexpr float kLedgeReach = 24.0f;       // hands reach this far above the head
inline constexpr float kLedgeForward = 36.0f;     // probe this far over the ledge
inline constexpr float kLedgeMinHeight = 40.0f;   // lower ledges are vaulted or stepped
inline constexpr float kLedgeGrabMaxRiseSpeed = 150.0f;
inline constexpr float kLedgeClimbSec = 0.6f;     // placeholder
inline constexpr float kLedgeDropCooldownSec = 0.4f;
inline constexpr float kVaultMaxHeight = 48.0f;   // placeholder
inline constexpr float kVaultReach = 24.0f;
inline constexpr float kVaultMinSpeed = 50.0f;
inline constexpr float kVaultSec = 0.35f;         // placeholder

// Collision.
inline constexpr float kWalkableNormalZ = 0.7f;  // Source: surfaces flatter than ~45 degrees are floor
inline constexpr float kGroundProbe = 2.0f;      // how far below the feet counts as standing
inline constexpr float kSkin = 0.03125f;         // gap kept from surfaces after a hit (Source DIST_EPSILON-ish)

}  // namespace ar::faith
