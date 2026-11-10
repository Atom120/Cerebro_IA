// Tuning values for Faith's movement, in Source units (su, su/s, su/s^2) and seconds.
// Owner: Peña (mod/faith_move). Values marked "placeholder" have no Mirror's Edge source yet;
// P5 replaces them with values fitted to real ME recordings (qa/telemetria/*_live.csv).
#pragma once

#include "ar/units.h"

namespace ar::faith {

// P5: values measured in Mirror's Edge (GOG v1.01) from qa/telemetria/*_live.csv, in uu and seconds.
// Fitted by mod/faith_move/tuning/fit_live.py; see docs/tuning/P5_ajuste.md for the table and charts.
namespace me_live {
inline constexpr float kGravityUu = 1597.0f;            // jump.csv + skill_roll.csv, linear fit of vz per jump
inline constexpr float kJumpZUu = 604.0f;               // jump.csv, vz on the first air tick (3 jumps)
inline constexpr float kSprintTimeConstSec = 1.86f;     // sprint_start.csv: 720 - v ~ exp(-t / 1.86)
inline constexpr float kStopTimeConstSec = 0.12f;       // sprint_start.csv: 715 -> 0 when the stick is released
inline constexpr float kStopSnapUu = 12.0f;             // below this the speed drops to 0 in one tick
inline constexpr float kWallrunGroundStartZUu = 515.0f; // wallrun_left.csv: wallrun started from the floor
inline constexpr float kWallrunGravityUu = 661.0f;      // wallrun_right.csv: vz slope over the one whole wallrun
inline constexpr float kWallrunJumpZUu = 518.0f;        // wallrun_*.csv: vz when jumping off the wall
inline constexpr float kWallClimbZUu = 645.0f;          // ledge_grab.csv: vz at the start of MOVE_WallClimbing
inline constexpr float kWallClimbGravityUu = 604.0f;    // ledge_grab.csv: vz slope while climbing
inline constexpr float kSlideDecelUu = 313.0f;          // slide.csv: 599 -> 264 uu/s in 1.03 s (2 slides)
inline constexpr float kHardLandingLockSec = 1.97f;     // hard_landing.csv: MOVE_Landing, speed 0 afterwards
inline constexpr float kSkillRollSec = 1.23f;           // skill_roll.csv: MOVE_SkillRoll
inline constexpr float kSkillRollExitUu = 240.0f;       // skill_roll.csv: speed when the roll ends
inline constexpr float kLedgeClimbSec = 1.52f;          // ledge_grab.csv: MOVE_GrabPullUp (2 climbs)
inline constexpr float kVaultSec = 0.55f;               // wallclimb.csv: MOVE_VaultOver (0.51 and 0.63 s)
}  // namespace me_live

// Ground (P2). Section 6 of PLAN_EQUIPO.md.
inline constexpr float kWalkSpeed = UuToSu(150.0f);                         // placeholder (59 su/s)
inline constexpr float kRunSpeed = UuToSu(me_reference::kSprintStartSpeedUu);  // 157.5: sprint starts above
inline constexpr float kSprintMaxSpeed = UuToSu(me_reference::kSprintMaxSpeedUu);  // 283.5
inline constexpr float kGroundAccel = UuToSu(me_reference::kAccelRateUu);   // 2419 su/s^2, up to kRunSpeed
inline constexpr float kGroundDecel = kGroundAccel;                         // placeholder: turn braking
inline constexpr float kStopTimeConstSec = me_live::kStopTimeConstSec;     // P5: no input, speed decays
inline constexpr float kStopSnapSpeed = UuToSu(me_live::kStopSnapUu);      // 4.7 su/s
inline constexpr float kSprintTimeConstSec = me_live::kSprintTimeConstSec;   // P5: run -> sprint is exponential
inline constexpr float kSprintSnap = 2.0f;  // su/s (5 uu/s): closer than this to the cap is full sprint (~7.7 s)
inline constexpr float kSprintMinForwardDot = 0.7f;  // wish direction must be this close to the view

// Air (P2).
inline constexpr float kJumpSpeed = UuToSu(me_live::kJumpZUu);             // P5: 237.8 su/s (reference 248)
inline constexpr float kJumpAddXY = UuToSu(me_reference::kJumpAddXYUu);     // 39.4 su/s along the run
inline constexpr float kGravity = UuToSu(me_live::kGravityUu);           // P5: 628.7 su/s^2 (reference 646)
inline constexpr float kAirAccel = 360.0f;                                  // placeholder: steering only
inline constexpr float kCoyoteTimeSec = 0.1f;                               // placeholder

// Landing (P2).
inline constexpr float kHardLandingHeight = UuToSu(me_reference::kHardLandingHeightUu);  // 208.7 su
inline constexpr float kUncontrolledFallHeight = UuToSu(me_reference::kFallingUncontrolledHeightUu);  // 393.7
inline constexpr float kHardLandingLockSec = me_live::kHardLandingLockSec;  // P5: no input after a hard landing
inline constexpr float kHardLandingMaxSpeed = 0.0f;         // P5: Faith stops dead
inline constexpr float kSoftLandingAnimSec = 0.25f;         // placeholder
inline constexpr float kMinLandingAnimHeight = 8.0f;        // smaller drops (steps, spawn) play no anim

// Crouch, slide, coil and Skill Roll (P3).
inline constexpr float kCrouchSpeed = 63.0f;                    // placeholder
inline constexpr float kSlideMinSpeed = kRunSpeed - 1.0f;       // slide only from a run or sprint
inline constexpr float kSlideDecel = UuToSu(me_live::kSlideDecelUu);  // P5: 123.2 su/s^2
inline constexpr float kSlideAbortSpeed = UuToSu(me_reference::kSlideAbortSpeedUu);  // 98.4
inline constexpr float kCoilMaxSec = 0.8f;                      // placeholder: coil un-tucks after this
inline constexpr float kSkillRollHeight = UuToSu(me_reference::kSkillRollLandingHeightUu);  // 78.7
inline constexpr float kRollWindowSec = 0.35f;  // placeholder: Down pressed this long before landing rolls
inline constexpr float kSkillRollSec = me_live::kSkillRollSec;  // P5
inline constexpr float kSkillRollExitSpeed = UuToSu(me_live::kSkillRollExitUu);  // P5: 94.5 su/s
inline constexpr float kTurn180Sec = 0.3f;      // placeholder

// Walls (P3).
inline constexpr float kWallReach = 16.0f;                  // how far beside/ahead a wall still counts
inline constexpr float kWallStick = 5.0f;                   // su/s pushed into the wall to keep contact
inline constexpr float kWallMaxNormalZ = 0.3f;              // steeper than this is not a wall
inline constexpr float kWallrunMinSpeed = UuToSu(me_reference::kWallrunMinSpeedUu);        // 78.7
inline constexpr float kWallrunInitialZ = UuToSu(me_reference::kWallrunInitialZUu);        // 66.9
inline constexpr float kWallrunGroundStartZ = UuToSu(me_live::kWallrunGroundStartZUu);  // P5: 202.8
inline constexpr float kWallrunAccel = UuToSu(me_reference::kWallrunHorizAccelUu);         // 322.8, forward held
inline constexpr float kWallrunDecel = UuToSu(me_reference::kWallrunHorizDecelUu);         // 196.9, released
inline constexpr float kWallrunGravityScale = me_live::kWallrunGravityUu / me_live::kGravityUu;  // P5: 0.414
inline constexpr float kWallrunJumpZ = UuToSu(me_live::kWallrunJumpZUu);  // P5: 203.9 su/s
inline constexpr float kWallrunDurationSec = me_reference::kWallrunDurationSec;            // 1.2
inline constexpr float kWallrunMinWallHeight = UuToSu(me_reference::kWallrunMinWallHeightUu);  // 75.6
inline constexpr float kWallrunMaxIntoDot = 0.8f;           // running more into the wall than this climbs instead
inline constexpr float kWallrunMaxStartFallSpeed = 150.0f;  // placeholder
inline constexpr float kWallrunRollDeg = 15.0f;             // placeholder: camera leans away from the wall
inline constexpr float kWallrunJumpOut = 150.0f;            // placeholder: push away from the wall
inline constexpr float kWallClimbReach = 20.0f;
inline constexpr float kWallClimbMinFacingDot = 0.8f;       // ~37 degrees from head-on
inline constexpr float kWallClimbSpeed = UuToSu(me_live::kWallClimbZUu);  // P5: 253.9
inline constexpr float kWallClimbGravityScale = me_live::kWallClimbGravityUu / me_live::kGravityUu;  // P5: 0.378, ~135 su
inline constexpr float kWallClimbEndSpeed = -60.0f;         // falling faster than this lets go
inline constexpr float kWallClimbTurnHoldSec = 1.0f;        // placeholder
inline constexpr float kWallClimbJumpOut = 200.0f;          // placeholder

// Ledges and vault (P3).
inline constexpr float kLedgeReach = 24.0f;       // hands reach this far above the head
inline constexpr float kLedgeForward = 36.0f;     // probe this far over the ledge (first try)
inline constexpr float kLedgeForwardProbes[] = {kLedgeForward, 24.0f, 48.0f};  // bevelled/rounded tops
inline constexpr float kLedgeMinHeight = 40.0f;   // lower ledges are vaulted, stepped or (in the air) mantled
inline constexpr float kMantleMinHeight = 4.0f;   // in the air, edges this far above the feet are climbed
inline constexpr float kLedgeGrabMaxRiseSpeed = 150.0f;
inline constexpr float kLedgeClimbSec = me_live::kLedgeClimbSec;  // P5
inline constexpr float kLedgeDropCooldownSec = 0.4f;
inline constexpr float kVaultMaxHeight = 48.0f;   // placeholder
inline constexpr float kVaultReach = 24.0f;
inline constexpr float kVaultMinSpeed = 50.0f;
inline constexpr float kVaultSec = me_live::kVaultSec;  // P5
inline constexpr float kClimbClearance = 2.0f;    // the climb/vault path is checked this far above the top

// Collision.
inline constexpr float kWalkableNormalZ = 0.7f;  // Source: surfaces flatter than ~45 degrees are floor
inline constexpr float kGroundProbe = 2.0f;      // how far below the feet counts as standing
inline constexpr float kSkin = 0.03125f;         // gap kept from surfaces after a hit (Source DIST_EPSILON-ish)

}  // namespace ar::faith
