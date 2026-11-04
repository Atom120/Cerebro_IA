// Faith's movement (ar::IFaithMovement).
//   P1: library skeleton. P2: ground (walk, run, sprint with inertia, jump, gravity, landings).
//   P3: parkour state machine (wallrun, wallclimb + turn + jump, ledge hang and climb, vault, slide,
//       crouch, coil, Skill Roll, 180 turn). P4 (portal continuation) plugs into Step later.
//
// Everything that survives a tick lives in FaithState; the library-private part is Internal,
// copied in and out of FaithState::internal so Source prediction can rewind it.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "ar/faith_movement.h"
#include "faith_move/faith_params.h"

namespace ar {
namespace {

using namespace faith;
using aperture_runner::ClampLength;
using aperture_runner::Cross;
using aperture_runner::Dot;
using aperture_runner::Length;
using aperture_runner::Normalized;

enum class Mode : uint8_t { Ground, Air, Slide, WallRun, WallClimb, WallClimbTurn, LedgeHang, LedgeClimb };
enum AirKind : uint8_t { kAirFall, kAirJump, kAirWallRunJump, kAirWallClimbJump };
enum WallSide : uint8_t { kWallNone, kWallLeft, kWallRight };

struct Internal {
    float apex_z;            // highest origin.z since leaving the ground (fall height = apex - landing)
    float coyote_left;       // seconds during which a jump is still allowed after walking off a ledge
    float land_lock_left;    // hard landing: seconds without input
    float land_anim_left;    // soft landing: seconds the LandSoft animation keeps playing
    float mode_time;         // seconds in the current mode
    float roll_left;         // Skill Roll seconds left
    float turn_left;         // ground 180 turn seconds left (animation only)
    float coil_time;         // seconds tucked in the air
    float down_ago;          // seconds since Down was last pressed (Skill Roll window)
    float ledge_cooldown;    // after dropping from a ledge, no regrab for a moment
    float exit_speed;        // vault: horizontal speed to keep once on top
    float reserved;
    Vec3 last_wall;          // last wall run on, so the same wall cannot be re-run before landing
    Vec3 ledge_start;
    Vec3 ledge_target;
    uint8_t mode;
    uint8_t air_kind;
    uint8_t wall_side;
    uint8_t climb_used;      // one wallclimb per airtime
    uint8_t vault;           // LedgeClimb is a vault
    uint8_t pad[3];
};
static_assert(sizeof(Internal) <= sizeof(FaithState::internal), "Internal does not fit in FaithState");
static_assert(std::is_trivially_copyable_v<Internal>);

Internal Load(const FaithState& s) {
    Internal in;
    std::memcpy(&in, s.internal, sizeof(in));
    return in;
}

void Store(FaithState& s, const Internal& in) { std::memcpy(s.internal, &in, sizeof(in)); }

Vec3 Horizontal(Vec3 v) { return {v.x, v.y, 0.0f}; }

Vec3 ClipVelocity(Vec3 v, Vec3 normal) { return v - normal * Dot(v, normal); }

bool Pressed(const FaithInput& in, uint32_t b) { return (in.buttons_pressed & b) != 0; }
bool Held(const FaithInput& in, uint32_t b) { return (in.buttons & b) != 0; }

// Timers in whole ticks: 0.3 s at dt 0.015 ends after exactly 20 ticks despite float rounding.
void Countdown(float& seconds, float dt) { seconds = seconds - dt > 1e-4f ? seconds - dt : 0.0f; }

bool WallKind(TraceHitKind k) { return k == TraceHitKind::World || k == TraceHitKind::StaticProp; }

void SetMode(Internal& in, Mode m) {
    if (in.mode != static_cast<uint8_t>(m)) {
        in.mode = static_cast<uint8_t>(m);
        in.mode_time = 0.0f;
    }
}

class FaithMovement final : public IFaithMovement {
public:
    explicit FaithMovement(const FaithHostConfig& config) : cfg_(config) {}

    void Reset(FaithState& state, Vec3 origin, Vec3 velocity) const override {
        state = FaithState{};
        state.origin = origin;
        state.velocity = velocity;
        state.eye_height = cfg_.view_height;
        state.speed_2d = Length(Horizontal(velocity));
        Internal in{};
        in.apex_z = origin.z;
        in.mode = static_cast<uint8_t>(Mode::Air);
        in.down_ago = 10.0f;
        Store(state, in);
    }

    FaithStepResult Step(const FaithInput& input, FaithState& state, const ITraceProvider& trace) const override {
        Tick t{input, state, trace, Load(state), {}};
        const FaithAnimState previous_anim = state.anim;
        const float dt = input.dt;
        Internal& in = t.in;

        if (input.portal.teleported) {
            // The engine moved the origin; the host refreshes it. Only our vectors need rotating (P4 extends this).
            state.velocity = input.portal.rotation * state.velocity;
            state.wall_normal = input.portal.rotation * state.wall_normal;
        }

        in.mode_time += dt;
        Countdown(in.land_lock_left, dt);
        Countdown(in.land_anim_left, dt);
        Countdown(in.roll_left, dt);
        Countdown(in.turn_left, dt);
        Countdown(in.ledge_cooldown, dt);
        in.down_ago = Pressed(input, kFaithButtonDown) ? 0.0f : std::min(in.down_ago + dt, 10.0f);

        t.forward = YawForward(input.view_angles.y);
        const Vec3 right{t.forward.y, -t.forward.x, 0.0f};
        const Vec3 wish = t.forward * input.forward_move + right * input.side_move;
        t.wish_len = std::min(Length(wish), 1.0f);
        t.wish_dir = Normalized(wish);

        // Still standing? Walking off a ledge starts a fall with coyote time.
        const Mode mode = static_cast<Mode>(in.mode);
        if ((mode == Mode::Ground || mode == Mode::Slide) && !CheckGround(t)) LeaveGround(t);

        switch (static_cast<Mode>(in.mode)) {
            case Mode::Ground:
            case Mode::Slide: StepGround(t); break;
            case Mode::Air: StepAir(t, false); break;
            case Mode::WallRun: StepWallRun(t); break;
            case Mode::WallClimb:
            case Mode::WallClimbTurn: StepWallClimb(t); break;
            case Mode::LedgeHang:
            case Mode::LedgeClimb: StepLedge(t); break;
        }

        const Mode now = static_cast<Mode>(in.mode);
        const bool on_wall = now == Mode::WallRun || now == Mode::WallClimb || now == Mode::WallClimbTurn ||
                             now == Mode::LedgeHang || now == Mode::LedgeClimb;
        if (!on_wall) state.wall_normal = {};
        state.on_ground = now == Mode::Ground || now == Mode::Slide;
        state.speed_2d = Length(Horizontal(state.velocity));
        state.anim = PickAnim(t);
        state.state_time = state.anim == previous_anim ? state.state_time + dt : 0.0f;
        state.eye_height = state.ducked ? cfg_.duck_view_height : cfg_.view_height;
        state.view_roll = now == Mode::WallRun ? (in.wall_side == kWallLeft ? kWallrunRollDeg : -kWallrunRollDeg) : 0.0f;
        Store(state, in);
        return t.result;
    }

private:
    struct Tick {
        const FaithInput& input;
        FaithState& s;
        const ITraceProvider& trace;
        Internal in;
        FaithStepResult result;
        Vec3 forward;
        Vec3 wish_dir;
        float wish_len = 0.0f;
    };

    static Vec3 YawForward(float yaw_deg) {
        const float yaw = yaw_deg * aperture_runner::kDegToRad;
        return {std::cos(yaw), std::sin(yaw), 0.0f};
    }

    Vec3 Mins(const FaithState& s) const { return s.ducked ? cfg_.duck_hull_mins : cfg_.hull_mins; }
    Vec3 Maxs(const FaithState& s) const { return s.ducked ? cfg_.duck_hull_maxs : cfg_.hull_maxs; }
    float DuckDrop() const { return cfg_.hull_maxs.z - cfg_.duck_hull_maxs.z; }

    TraceResult Hull(const Tick& t, Vec3 from, Vec3 to) const {
        return t.trace.TraceHull(from, to, Mins(t.s), Maxs(t.s));
    }

    // ---------------------------------------------------------------- ground

    void StepGround(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        const bool locked = in.land_lock_left > 0.0f;

        if (Pressed(t.input, kFaithButtonUp) && !locked && in.roll_left <= 0.0f) {
            if (TryVault(t)) return StepLedge(t);
            if (TryStartWallClimb(t)) return StepWallClimb(t);
            if (TryStartWallRun(t, true)) return StepWallRun(t);
            if (!s.ducked || CanStand(s, t.trace)) {
                Jump(t);
                return StepAir(t, true);
            }
        }

        if (Pressed(t.input, kFaithButtonTurn180) && static_cast<Mode>(in.mode) == Mode::Ground && !locked) {
            const Vec3 h = Horizontal(s.velocity);
            s.velocity = ClampLength(-h * 0.5f, kRunSpeed);
            in.turn_left = kTurn180Sec;
            t.result.events |= kFaithEventTurn180;
        }

        if (static_cast<Mode>(in.mode) == Mode::Ground && Pressed(t.input, kFaithButtonDown) && !locked &&
            in.roll_left <= 0.0f && s.speed_2d >= kSlideMinSpeed) {
            SetMode(in, Mode::Slide);
            s.ducked = true;
            t.result.events |= kFaithEventSlideStart;
        }

        Vec3 h = Horizontal(s.velocity);
        if (static_cast<Mode>(in.mode) == Mode::Slide) {
            const float speed = std::max(0.0f, Length(h) - kSlideDecel * t.input.dt);
            h = Normalized(h) * speed;
            if (speed < kSlideAbortSpeed || !Held(t.input, kFaithButtonDown)) SetMode(in, Mode::Ground);
        } else if (in.roll_left > 0.0f) {
            // The roll keeps the landing speed.
        } else {
            if (Held(t.input, kFaithButtonDown)) {
                s.ducked = true;
            } else if (s.ducked && CanStand(s, t.trace)) {
                s.ducked = false;
            }
            const bool walk = Held(t.input, kFaithButtonWalk);
            const float base = s.ducked ? kCrouchSpeed : (walk ? kWalkSpeed : kRunSpeed);
            const bool can_sprint = !walk && !s.ducked && t.input.forward_move > 0.5f &&
                                    Dot(t.wish_dir, t.forward) > kSprintMinForwardDot;
            h = locked ? GroundAccelerate(h, {}, 0.0f, base, false, t.input.dt)
                       : GroundAccelerate(h, t.wish_dir, t.wish_len, base, can_sprint, t.input.dt);
        }
        s.velocity = {h.x, h.y, 0.0f};

        const Vec3 start_origin = s.origin;
        const Vec3 start_velocity = s.velocity;
        bool hit_wall = false;
        SlideMove(t, s.origin, s.velocity, t.input.dt, &hit_wall);
        if (hit_wall) TryStepUp(t, start_origin, start_velocity);
        if (!CheckGround(t) && !SnapDown(t, cfg_.step_height + kGroundProbe)) LeaveGround(t);
    }

    // Speed along the wish direction rises quickly up to `base`, then slowly up to kSprintMaxSpeed while
    // sprinting (inertia). Speed sideways to the wish direction is braked, so gentle turns keep the
    // sprint and sharp turns lose it.
    static Vec3 GroundAccelerate(Vec3 h, Vec3 wish_dir, float wish_len, float base, bool can_sprint, float dt) {
        if (wish_len < 1e-3f) {
            const float speed = std::max(0.0f, Length(h) - kGroundDecel * dt);
            return Normalized(h) * speed;
        }
        const float base_cap = base * wish_len;
        const float cap = can_sprint ? kSprintMaxSpeed : base_cap;

        float along = Dot(h, wish_dir);
        Vec3 lateral = h - wish_dir * along;
        lateral = Normalized(lateral) * std::max(0.0f, Length(lateral) - kGroundDecel * dt);

        if (along < base_cap) {
            along = std::min(base_cap, along + kGroundAccel * dt);
        } else if (along < cap) {
            along = std::min(cap, along + kSprintAccel * dt);
        } else if (along > cap) {
            along = std::max(cap, along - kGroundDecel * dt);
        }
        return wish_dir * along + lateral;
    }

    void Jump(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        Vec3 h = Horizontal(s.velocity);
        const float speed = Length(h);
        if (speed > 1.0f) h = h * (std::min(speed + kJumpAddXY, kSprintMaxSpeed + kJumpAddXY) / speed);
        s.velocity = {h.x, h.y, kJumpSpeed};
        s.ducked = false;
        SetMode(in, Mode::Air);
        in.air_kind = kAirJump;
        in.apex_z = s.origin.z;
        in.coyote_left = 0.0f;
        t.result.events |= kFaithEventJumped;
    }

    void LeaveGround(Tick& t) const {
        SetMode(t.in, Mode::Air);
        t.in.apex_z = t.s.origin.z;
        t.in.coyote_left = kCoyoteTimeSec;
        t.in.air_kind = kAirFall;
        t.in.roll_left = 0.0f;
    }

    // ---------------------------------------------------------------- air

    void StepAir(Tick& t, bool just_jumped) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        const float dt = t.input.dt;

        if (!just_jumped) {
            if (in.air_kind == kAirFall) Countdown(in.coyote_left, dt);
            if (Pressed(t.input, kFaithButtonUp) && in.coyote_left > 0.0f && in.air_kind == kAirFall) {
                Jump(t);
                just_jumped = true;
            }
        }

        // Coil: tuck the legs. The head stays put, so the feet (origin) rise by the duck difference.
        if (Pressed(t.input, kFaithButtonDown) && !s.ducked) {
            s.ducked = true;
            s.origin.z += DuckDrop();
            in.coil_time = 0.0f;
        }
        if (s.ducked) {
            in.coil_time += dt;
            if (!Held(t.input, kFaithButtonDown) || in.coil_time > kCoilMaxSec) TryUnduckInAir(t);
        }

        if (!just_jumped) {
            if (TryGrabLedgeInAir(t)) return;
            if (Held(t.input, kFaithButtonUp)) {
                if (TryStartWallClimb(t)) return StepWallClimb(t);
                if (TryStartWallRun(t, false)) return StepWallRun(t);
            }
        }

        Vec3 h = Horizontal(s.velocity);
        if (t.wish_len > 1e-3f) {
            const float limit = std::max(Length(h), kRunSpeed);  // air control only steers
            h = ClampLength(h + t.wish_dir * (kAirAccel * t.wish_len * dt), limit);
        }
        const float vz = just_jumped ? s.velocity.z : s.velocity.z - kGravity * dt;
        s.velocity = {h.x, h.y, vz};
        MoveAirborne(t, vz);
    }

    // Moves while airborne and lands if the move ended on a floor.
    void MoveAirborne(Tick& t, float impact_vz) const {
        SlideMove(t, t.s.origin, t.s.velocity, t.input.dt, nullptr);
        // Apex of the feet as if standing: tucking the legs (coil) raises the origin but is not height gained.
        t.in.apex_z = std::max(t.in.apex_z, t.s.origin.z - (t.s.ducked ? DuckDrop() : 0.0f));
        if (t.s.velocity.z <= 0.0f && CheckGround(t)) Land(t, impact_vz);
    }

    void TryUnduckInAir(Tick& t) const {
        FaithState& s = t.s;
        const Vec3 lowered = s.origin - Vec3{0, 0, DuckDrop()};
        if (Hull(t, s.origin, lowered).Hit()) return;
        if (t.trace.TraceHull(lowered, lowered, cfg_.hull_mins, cfg_.hull_maxs).start_solid) return;
        s.origin = lowered;
        s.ducked = false;
    }

    void Land(Tick& t, float impact_vz) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        const float fall = in.apex_z - s.origin.z;
        t.result.events |= kFaithEventLanded;
        t.result.impact_speed = std::max(0.0f, -impact_vz);
        SetMode(in, Mode::Ground);
        s.on_ground = true;
        if (fall >= kSkillRollHeight && in.down_ago <= kRollWindowSec) {
            t.result.events |= kFaithEventSkillRoll;
            in.roll_left = kSkillRollSec;
            s.ducked = true;
        } else if (fall >= kHardLandingHeight) {
            t.result.events |= kFaithEventHardLanding;
            in.land_lock_left = kHardLandingLockSec;
            in.land_anim_left = 0.0f;
            s.velocity = ClampLength(Horizontal(s.velocity), kHardLandingMaxSpeed);
        } else if (fall >= kMinLandingAnimHeight) {
            in.land_anim_left = kSoftLandingAnimSec;
        }
        in.air_kind = kAirFall;
        in.climb_used = 0;
        in.last_wall = {};
        in.coyote_left = kCoyoteTimeSec;
        in.apex_z = s.origin.z;
    }

    // ---------------------------------------------------------------- walls

    // A wall within `reach` along dir: vertical enough and of a kind Faith can run or climb on.
    bool FindWall(const Tick& t, Vec3 dir, float reach, Vec3* normal) const {
        const TraceResult r = Hull(t, t.s.origin, t.s.origin + dir * reach);
        if (r.start_solid || !r.Hit() || std::fabs(r.normal.z) > kWallMaxNormalZ || !WallKind(r.kind)) return false;
        *normal = r.normal;
        return true;
    }

    // The wall in dir still exists at `height` above the feet.
    bool WallAtHeight(const Tick& t, Vec3 dir, float height, float reach) const {
        const Vec3 from = t.s.origin + Vec3{0, 0, height};
        return t.trace.TraceRay(from, from + dir * (cfg_.hull_maxs.x + reach + 2.0f)).Hit();
    }

    bool TryStartWallRun(Tick& t, bool from_ground) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        if (s.ducked) return false;
        const Vec3 h = Horizontal(s.velocity);
        const float speed = Length(h);
        if (speed < kWallrunMinSpeed || (!from_ground && s.velocity.z < -kWallrunMaxStartFallSpeed)) return false;
        const Vec3 dir = h * (1.0f / speed);
        const Vec3 sides[2] = {{-dir.y, dir.x, 0.0f}, {dir.y, -dir.x, 0.0f}};
        for (int i = 0; i < 2; ++i) {
            Vec3 n;
            if (!FindWall(t, sides[i], kWallReach, &n)) continue;
            if (Dot(dir, -n) > kWallrunMaxIntoDot) continue;
            if (Dot(n, in.last_wall) > 0.9f) continue;
            if (!WallAtHeight(t, sides[i], kWallrunMinWallHeight, kWallReach)) continue;

            SetMode(in, Mode::WallRun);
            in.wall_side = i == 0 ? kWallLeft : kWallRight;
            in.air_kind = kAirFall;
            in.coyote_left = 0.0f;
            in.apex_z = s.origin.z;
            s.wall_normal = n;
            const Vec3 along = Normalized(ClipVelocity(h, n)) * speed;
            const float vz = from_ground ? kWallrunGroundStartZ
                                         : std::clamp(s.velocity.z, kWallrunInitialZ, kWallrunGroundStartZ);
            s.velocity = {along.x, along.y, vz};
            t.result.events |= kFaithEventWallRunStart;
            return true;
        }
        return false;
    }

    void EndWallRun(Tick& t) const {
        t.in.last_wall = t.s.wall_normal;
        t.in.apex_z = t.s.origin.z;
        t.in.air_kind = kAirFall;
        SetMode(t.in, Mode::Air);
        t.result.events |= kFaithEventWallRunEnd;
    }

    void StepWallRun(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        const float dt = t.input.dt;
        const Vec3 n = s.wall_normal;
        const Vec3 h = Horizontal(s.velocity);

        if (Pressed(t.input, kFaithButtonUp) && in.mode_time > 0.0f) {  // wall jump
            EndWallRun(t);
            const Vec3 out = h + n * kWallrunJumpOut;
            s.velocity = {out.x, out.y, kJumpSpeed};
            in.air_kind = kAirWallRunJump;
            t.result.events |= kFaithEventJumped;
            return MoveAirborne(t, s.velocity.z);
        }

        float speed = Length(h);
        speed = t.input.forward_move > 0.5f ? std::min(kSprintMaxSpeed, speed + kWallrunAccel * dt)
                                            : std::max(0.0f, speed - kWallrunDecel * dt);
        const Vec3 along = Normalized(ClipVelocity(h, n));
        if (Pressed(t.input, kFaithButtonDown) || in.mode_time >= kWallrunDurationSec || speed < kWallrunMinSpeed ||
            Length(along) < 0.5f) {
            EndWallRun(t);
            return StepAir(t, false);
        }

        const float vz = s.velocity.z - kGravity * kWallrunGravityScale * dt;
        s.velocity = along * speed - n * kWallStick + Vec3{0, 0, vz};
        SlideMove(t, s.origin, s.velocity, dt, nullptr);
        s.velocity = s.velocity - n * std::min(0.0f, Dot(s.velocity, n));  // drop the stick push

        Vec3 wall;
        if (!FindWall(t, -n, kWallReach, &wall) || Dot(wall, n) < 0.9f) {
            EndWallRun(t);
            return;
        }
        s.wall_normal = wall;
        if (s.velocity.z <= 0.0f && CheckGround(t)) {
            t.result.events |= kFaithEventWallRunEnd;
            Land(t, vz);
        }
    }

    bool TryStartWallClimb(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        if (in.climb_used || s.ducked) return false;
        if (static_cast<Mode>(in.mode) == Mode::Air && (in.air_kind > kAirJump || s.velocity.z < -100.0f)) return false;
        Vec3 n;
        if (!FindWall(t, t.forward, kWallClimbReach, &n)) return false;
        if (Dot(t.forward, -n) < kWallClimbMinFacingDot) return false;
        if (!WallAtHeight(t, t.forward, cfg_.hull_maxs.z + 8.0f, kWallClimbReach)) return false;

        SetMode(in, Mode::WallClimb);
        in.climb_used = 1;
        in.air_kind = kAirFall;
        in.apex_z = s.origin.z;
        s.wall_normal = n;
        s.velocity = {0.0f, 0.0f, kWallClimbSpeed};
        t.result.events |= kFaithEventWallClimbStart;
        return true;
    }

    void StepWallClimb(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        const Vec3 n = s.wall_normal;

        if (static_cast<Mode>(in.mode) == Mode::WallClimbTurn) {
            s.velocity = {};
            if (Pressed(t.input, kFaithButtonUp)) {
                SetMode(in, Mode::Air);
                in.air_kind = kAirWallClimbJump;
                in.apex_z = s.origin.z;
                const Vec3 out = n * kWallClimbJumpOut;
                s.velocity = {out.x, out.y, kJumpSpeed};
                t.result.events |= kFaithEventJumped;
                return MoveAirborne(t, s.velocity.z);
            }
            if (Pressed(t.input, kFaithButtonDown) || in.mode_time > kWallClimbTurnHoldSec) LetGo(t);
            return;
        }

        if (Pressed(t.input, kFaithButtonTurn180)) {
            SetMode(in, Mode::WallClimbTurn);
            s.velocity = {};
            t.result.events |= kFaithEventTurn180;
            return;
        }
        if (Pressed(t.input, kFaithButtonDown)) return LetGo(t);
        if (TryGrabLedge(t, -n)) return;

        const float vz = s.velocity.z - kGravity * kWallClimbGravityScale * t.input.dt;
        if (vz < kWallClimbEndSpeed) {
            s.velocity = {0.0f, 0.0f, vz};
            return LetGo(t);
        }
        s.velocity = -n * kWallStick + Vec3{0, 0, vz};
        SlideMove(t, s.origin, s.velocity, t.input.dt, nullptr);
        s.velocity = s.velocity - n * std::min(0.0f, Dot(s.velocity, n));

        Vec3 wall;
        if (!FindWall(t, -n, kWallReach, &wall) && !TryGrabLedge(t, -n)) LetGo(t);
    }

    void LetGo(Tick& t) const {
        SetMode(t.in, Mode::Air);
        t.in.air_kind = kAirFall;
        t.in.apex_z = t.s.origin.z;
    }

    // ---------------------------------------------------------------- ledges

    // Finds a floor Faith can stand on, over the obstacle in dir, low enough for her hands.
    bool FindLedge(const Tick& t, Vec3 dir, Vec3* target) const {
        const FaithState& s = t.s;
        const float reach = cfg_.hull_maxs.z + kLedgeReach;
        const TraceResult up = t.trace.TraceHull(s.origin, s.origin + Vec3{0, 0, reach}, cfg_.hull_mins, cfg_.hull_maxs);
        if (up.start_solid || up.Hit()) return false;
        const TraceResult over = t.trace.TraceHull(up.end_pos, up.end_pos + dir * kLedgeForward, cfg_.hull_mins,
                                                   cfg_.hull_maxs);
        if (over.Hit()) return false;
        const TraceResult down = t.trace.TraceHull(over.end_pos, over.end_pos - Vec3{0, 0, reach}, cfg_.hull_mins,
                                                   cfg_.hull_maxs);
        if (down.start_solid || !down.Hit() || down.normal.z < kWalkableNormalZ) return false;
        *target = down.end_pos + Vec3{0, 0, kSkin};
        return target->z - s.origin.z > cfg_.step_height;
    }

    // The two-part path of a climb (straight up, then over) is free.
    bool ClimbPathFree(const Tick& t, Vec3 target) const {
        const Vec3 top{t.s.origin.x, t.s.origin.y, target.z};
        return !t.trace.TraceHull(t.s.origin, top, cfg_.hull_mins, cfg_.hull_maxs).Hit() &&
               !t.trace.TraceHull(top, target, cfg_.hull_mins, cfg_.hull_maxs).Hit();
    }

    bool TryGrabLedge(Tick& t, Vec3 dir) const {
        if (t.in.ledge_cooldown > 0.0f || t.s.ducked) return false;
        Vec3 target;
        if (!FindLedge(t, dir, &target) || target.z - t.s.origin.z < kLedgeMinHeight) return false;
        SetMode(t.in, Mode::LedgeHang);
        t.in.ledge_target = target;
        t.in.air_kind = kAirFall;
        t.s.wall_normal = -dir;
        t.s.velocity = {};
        t.result.events |= kFaithEventLedgeGrab;
        return true;
    }

    // In the air Faith grabs a ledge on her own when she moves toward it near the top of a jump.
    bool TryGrabLedgeInAir(Tick& t) const {
        if (t.s.velocity.z > kLedgeGrabMaxRiseSpeed) return false;
        const Vec3 h = Horizontal(t.s.velocity);
        Vec3 dir;
        if (Length(h) > 10.0f) {
            dir = Normalized(h);
        } else if (t.input.forward_move > 0.5f) {
            dir = t.forward;
        } else {
            return false;
        }
        Vec3 n;
        if (!FindWall(t, dir, kWallReach, &n)) return false;
        return TryGrabLedge(t, -n);
    }

    bool TryVault(Tick& t) const {
        FaithState& s = t.s;
        if (s.ducked || s.speed_2d < kVaultMinSpeed) return false;
        const Vec3 dir = Normalized(Horizontal(s.velocity));
        Vec3 n, target;
        if (!FindWall(t, dir, kVaultReach, &n) || !FindLedge(t, -n, &target)) return false;
        if (target.z - s.origin.z > kVaultMaxHeight || !ClimbPathFree(t, target)) return false;
        StartClimb(t, target, true);
        t.s.wall_normal = n;
        return true;
    }

    void StartClimb(Tick& t, Vec3 target, bool vault) const {
        SetMode(t.in, Mode::LedgeClimb);
        t.in.ledge_start = t.s.origin;
        t.in.ledge_target = target;
        t.in.vault = vault ? 1 : 0;
        t.in.exit_speed = vault ? t.s.speed_2d : 0.0f;
        t.s.velocity = {};
    }

    void StepLedge(Tick& t) const {
        FaithState& s = t.s;
        Internal& in = t.in;
        if (static_cast<Mode>(in.mode) == Mode::LedgeHang) {
            s.velocity = {};
            if (Pressed(t.input, kFaithButtonUp) && ClimbPathFree(t, in.ledge_target)) {
                StartClimb(t, in.ledge_target, false);
            } else if (Pressed(t.input, kFaithButtonDown)) {
                LetGo(t);
                in.ledge_cooldown = kLedgeDropCooldownSec;
            }
            return;
        }

        // LedgeClimb: first up, then over. Positions are a pure function of mode_time (prediction-safe).
        const float duration = in.vault ? kVaultSec : kLedgeClimbSec;
        const float p = std::min(1.0f, in.mode_time / duration);
        const Vec3 a = in.ledge_start;
        const Vec3 b = in.ledge_target;
        constexpr float kUpPart = 0.6f;
        if (p < kUpPart) {
            s.origin = {a.x, a.y, a.z + (b.z - a.z) * (p / kUpPart)};
        } else {
            const float q = (p - kUpPart) / (1.0f - kUpPart);
            s.origin = {a.x + (b.x - a.x) * q, a.y + (b.y - a.y) * q, b.z};
        }
        s.velocity = {};
        if (p >= 1.0f) {
            s.origin = b;
            const Vec3 dir = Normalized(Horizontal(b - a));
            s.velocity = dir * in.exit_speed;
            SetMode(in, Mode::Ground);
            in.apex_z = b.z;
            in.vault = 0;
            CheckGround(t);
        }
    }

    // ---------------------------------------------------------------- collision

    // Source-style TryPlayerMove: move, clip the velocity against what was hit, repeat.
    void SlideMove(const Tick& t, Vec3& origin, Vec3& velocity, float dt, bool* hit_wall) const {
        Vec3 planes[5];
        int num_planes = 0;
        const Vec3 primal = velocity;
        Vec3 original = velocity;
        float time_left = dt;
        for (int bump = 0; bump < 4; ++bump) {
            if (Dot(velocity, velocity) < 1e-8f) break;
            const TraceResult r = Hull(t, origin, origin + velocity * time_left);
            if (r.start_solid || r.all_solid) {
                velocity = {};
                return;
            }
            if (r.fraction > 0.0f) {
                origin = r.end_pos;
                original = velocity;
                num_planes = 0;
            }
            if (r.fraction >= 1.0f) break;

            if (r.normal.z < kWalkableNormalZ && hit_wall) *hit_wall = true;
            origin = origin + r.normal * kSkin;
            time_left -= time_left * r.fraction;
            if (num_planes >= 5) {
                velocity = {};
                break;
            }
            planes[num_planes++] = r.normal;

            int i = 0;
            for (; i < num_planes; ++i) {
                const Vec3 v = ClipVelocity(original, planes[i]);
                int j = 0;
                for (; j < num_planes; ++j)
                    if (j != i && Dot(v, planes[j]) < 0.0f) break;
                if (j == num_planes) {
                    velocity = v;
                    break;
                }
            }
            if (i == num_planes) {
                if (num_planes != 2) {
                    velocity = {};
                    break;
                }
                const Vec3 crease = Normalized(Cross(planes[0], planes[1]));
                velocity = crease * Dot(crease, velocity);
            }
            if (Dot(velocity, primal) <= 0.0f) {
                velocity = {};
                break;
            }
        }
    }

    // Blocked by something low: retry the move from step_height up and keep it if it got further.
    void TryStepUp(const Tick& t, Vec3 start_origin, Vec3 start_velocity) const {
        FaithState& s = t.s;
        const TraceResult up = Hull(t, start_origin, start_origin + Vec3{0, 0, cfg_.step_height});
        if (up.start_solid) return;
        Vec3 origin = up.end_pos;
        if (up.Hit()) origin = origin - Vec3{0, 0, kSkin};
        const float raised = origin.z - start_origin.z;
        Vec3 velocity = start_velocity;
        SlideMove(t, origin, velocity, t.input.dt, nullptr);

        const TraceResult down = Hull(t, origin, origin - Vec3{0, 0, raised + kGroundProbe});
        if (down.start_solid || !down.Hit() || down.normal.z < kWalkableNormalZ) return;
        origin = down.end_pos + Vec3{0, 0, kSkin};

        const float direct = Length(Horizontal(s.origin - start_origin));
        const float stepped = Length(Horizontal(origin - start_origin));
        if (stepped > direct + 0.01f) {
            s.origin = origin;
            s.velocity = {velocity.x, velocity.y, 0.0f};
        }
    }

    // Standing means a walkable surface within kGroundProbe below the feet; snaps onto it.
    bool CheckGround(Tick& t) const { return SnapDown(t, kGroundProbe); }

    bool SnapDown(Tick& t, float distance) const {
        FaithState& s = t.s;
        const TraceResult r = Hull(t, s.origin, s.origin - Vec3{0, 0, distance});
        if (r.start_solid || !r.Hit() || r.normal.z < kWalkableNormalZ) return false;
        s.origin = r.end_pos + Vec3{0, 0, kSkin};
        s.velocity.z = 0.0f;
        s.ground_normal = r.normal;
        return true;
    }

    bool CanStand(const FaithState& s, const ITraceProvider& trace) const {
        return !trace.TraceHull(s.origin, s.origin, cfg_.hull_mins, cfg_.hull_maxs).start_solid;
    }

    // ---------------------------------------------------------------- animation

    static FaithAnimState PickAnim(const Tick& t) {
        const FaithState& s = t.s;
        const Internal& in = t.in;
        switch (static_cast<Mode>(in.mode)) {
            case Mode::WallRun: return in.wall_side == kWallLeft ? FaithAnimState::WallRunLeft : FaithAnimState::WallRunRight;
            case Mode::WallClimb: return FaithAnimState::WallClimb;
            case Mode::WallClimbTurn: return FaithAnimState::WallClimbTurn;
            case Mode::LedgeHang: return FaithAnimState::LedgeHang;
            case Mode::LedgeClimb: return in.vault ? FaithAnimState::Vault : FaithAnimState::LedgeClimbUp;
            case Mode::Slide: return FaithAnimState::Slide;
            case Mode::Air:
                if (s.velocity.z > 0.0f && in.air_kind == kAirWallRunJump) return FaithAnimState::WallRunJump;
                if (s.velocity.z > 0.0f && in.air_kind == kAirWallClimbJump) return FaithAnimState::WallClimbJump;
                if (s.ducked) return FaithAnimState::Coil;
                if (s.velocity.z > 0.0f && in.air_kind == kAirJump) return FaithAnimState::Jump;
                if (in.apex_z - s.origin.z > kUncontrolledFallHeight) return FaithAnimState::FallUncontrolled;
                return FaithAnimState::Fall;
            case Mode::Ground: break;
        }
        if (in.land_lock_left > 0.0f) return FaithAnimState::LandHard;
        if (in.roll_left > 0.0f) return FaithAnimState::SkillRoll;
        if (in.turn_left > 0.0f) return FaithAnimState::Turn180;
        if (in.land_anim_left > 0.0f) return FaithAnimState::LandSoft;
        if (s.ducked) return s.speed_2d < 1.0f ? FaithAnimState::Crouch : FaithAnimState::CrouchWalk;
        if (s.speed_2d < 1.0f) return FaithAnimState::Idle;
        if (Held(t.input, kFaithButtonWalk) || s.speed_2d <= kWalkSpeed + 1.0f) return FaithAnimState::Walk;
        if (s.speed_2d <= kRunSpeed + 0.5f) return FaithAnimState::Run;
        return FaithAnimState::Sprint;
    }

    FaithHostConfig cfg_;
};

}  // namespace

std::unique_ptr<IFaithMovement> CreateFaithMovement(const FaithHostConfig& config) {
    return std::make_unique<FaithMovement>(config);
}

}  // namespace ar
