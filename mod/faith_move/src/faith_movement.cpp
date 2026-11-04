// Faith's movement (ar::IFaithMovement). P1: library skeleton. P2: ground movement (walk, run,
// sprint with inertia, jump, gravity, fall, soft and hard landing). Parkour (P3) and portal
// continuation (P4) plug into Step later.
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

struct Internal {
    float apex_z;          // highest origin.z since leaving the ground (fall height = apex - landing)
    float coyote_left;     // seconds during which a jump is still allowed after walking off a ledge
    float land_lock_left;  // hard landing: seconds without input
    float land_anim_left;  // soft landing: seconds the LandSoft animation keeps playing
    uint8_t jumped;        // airborne because of a jump (not by walking off)
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

Vec3 ClipVelocity(Vec3 v, Vec3 normal) { return v - normal * aperture_runner::Dot(v, normal); }

class FaithMovement final : public IFaithMovement {
public:
    explicit FaithMovement(const FaithHostConfig& config) : cfg_(config) {}

    void Reset(FaithState& state, Vec3 origin, Vec3 velocity) const override {
        state = FaithState{};
        state.origin = origin;
        state.velocity = velocity;
        state.eye_height = cfg_.view_height;
        state.speed_2d = aperture_runner::Length(Horizontal(velocity));
        Internal in{};
        in.apex_z = origin.z;
        Store(state, in);
    }

    FaithStepResult Step(const FaithInput& input, FaithState& state, const ITraceProvider& trace) const override {
        FaithStepResult result;
        Internal in = Load(state);
        const float dt = input.dt;
        const FaithAnimState previous_anim = state.anim;

        if (input.portal.teleported) {
            // The engine moved the origin; the host refreshes it. Only our vectors need rotating (P4 extends this).
            state.velocity = input.portal.rotation * state.velocity;
            state.wall_normal = input.portal.rotation * state.wall_normal;
        }

        in.land_lock_left = std::max(0.0f, in.land_lock_left - dt);
        in.land_anim_left = std::max(0.0f, in.land_anim_left - dt);

        // Still standing? Walking off a ledge starts a fall with coyote time.
        if (state.on_ground && !CheckGround(trace, state)) {
            state.on_ground = false;
            in.apex_z = state.origin.z;
            in.coyote_left = kCoyoteTimeSec;
            in.jumped = 0;
        }

        const Vec3 forward = YawForward(input.view_angles.y);
        const Vec3 right{forward.y, -forward.x, 0.0f};
        Vec3 wish = forward * input.forward_move + right * input.side_move;
        const float wish_len = std::min(aperture_runner::Length(wish), 1.0f);
        const Vec3 wish_dir = aperture_runner::Normalized(wish);

        bool jumped_now = false;
        const bool can_jump = in.land_lock_left <= 0.0f &&
                              (state.on_ground || (in.coyote_left > 0.0f && !in.jumped));
        if ((input.buttons_pressed & kFaithButtonUp) && can_jump) {
            Vec3 h = Horizontal(state.velocity);
            const float speed = aperture_runner::Length(h);
            if (speed > 1.0f) h = h * (std::min(speed + kJumpAddXY, kSprintMaxSpeed + kJumpAddXY) / speed);
            state.velocity = {h.x, h.y, kJumpSpeed};
            state.on_ground = false;
            in.apex_z = state.origin.z;
            in.coyote_left = 0.0f;
            in.jumped = 1;
            jumped_now = true;
            result.events |= kFaithEventJumped;
        }

        const bool was_on_ground = state.on_ground;
        if (state.on_ground) {
            const bool locked = in.land_lock_left > 0.0f;
            const Vec3 h = GroundAccelerate(Horizontal(state.velocity), locked ? Vec3{} : wish_dir,
                                            locked ? 0.0f : wish_len, input, forward, dt);
            state.velocity = {h.x, h.y, 0.0f};
        } else if (!jumped_now) {
            in.coyote_left = std::max(0.0f, in.coyote_left - dt);
            const Vec3 h = AirAccelerate(Horizontal(state.velocity), wish_dir, wish_len, dt);
            state.velocity = {h.x, h.y, state.velocity.z - kGravity * dt};
        }

        const float impact_vz = state.velocity.z;
        const Vec3 start_origin = state.origin;
        const Vec3 start_velocity = state.velocity;
        bool hit_wall = false;
        SlideMove(trace, state.origin, state.velocity, dt, &hit_wall);
        if (was_on_ground && hit_wall) TryStepUp(trace, start_origin, start_velocity, dt, state);

        if (was_on_ground) {
            if (!CheckGround(trace, state) && !StepDown(trace, state)) {
                state.on_ground = false;
                in.apex_z = state.origin.z;
                in.coyote_left = kCoyoteTimeSec;
                in.jumped = 0;
            }
        } else {
            in.apex_z = std::max(in.apex_z, state.origin.z);
            if (state.velocity.z <= 0.0f && CheckGround(trace, state)) {
                Land(in, state, impact_vz, result);
            }
        }

        state.speed_2d = aperture_runner::Length(Horizontal(state.velocity));
        state.anim = PickAnim(state, in, input);
        state.state_time = state.anim == previous_anim ? state.state_time + dt : 0.0f;
        state.eye_height = cfg_.view_height;
        state.ducked = false;
        state.view_roll = 0.0f;
        Store(state, in);
        return result;
    }

private:
    static Vec3 YawForward(float yaw_deg) {
        const float yaw = yaw_deg * aperture_runner::kDegToRad;
        return {std::cos(yaw), std::sin(yaw), 0.0f};
    }

    // Accelerate toward the wish direction. Speed along it rises quickly up to kRunSpeed, then
    // slowly up to kSprintMaxSpeed while running forward (sprint inertia). Speed sideways to the
    // wish direction is braked, so gentle turns keep the sprint and sharp turns lose it.
    static Vec3 GroundAccelerate(Vec3 h, Vec3 wish_dir, float wish_len, const FaithInput& input, Vec3 forward,
                                 float dt) {
        if (wish_len < 1e-3f) {
            const float speed = std::max(0.0f, aperture_runner::Length(h) - kGroundDecel * dt);
            return aperture_runner::Normalized(h) * speed;
        }
        const bool walk = (input.buttons & kFaithButtonWalk) != 0;
        const bool can_sprint = !walk && input.forward_move > 0.5f &&
                                aperture_runner::Dot(wish_dir, forward) > kSprintMinForwardDot;
        const float base_cap = (walk ? kWalkSpeed : kRunSpeed) * wish_len;
        const float cap = can_sprint ? kSprintMaxSpeed : base_cap;

        float along = aperture_runner::Dot(h, wish_dir);
        Vec3 lateral = h - wish_dir * along;
        const float lateral_speed = std::max(0.0f, aperture_runner::Length(lateral) - kGroundDecel * dt);
        lateral = aperture_runner::Normalized(lateral) * lateral_speed;

        if (along < base_cap) {
            along = std::min(base_cap, along + kGroundAccel * dt);
        } else if (along < cap) {
            along = std::min(cap, along + kSprintAccel * dt);
        } else if (along > cap) {
            along = std::max(cap, along - kGroundDecel * dt);
        }
        return wish_dir * along + lateral;
    }

    // Air control only steers: it never takes the speed above max(current, kRunSpeed).
    static Vec3 AirAccelerate(Vec3 h, Vec3 wish_dir, float wish_len, float dt) {
        if (wish_len < 1e-3f) return h;
        const float limit = std::max(aperture_runner::Length(h), kRunSpeed);
        return aperture_runner::ClampLength(h + wish_dir * (kAirAccel * wish_len * dt), limit);
    }

    // Source-style TryPlayerMove: move, clip the velocity against what was hit, repeat.
    void SlideMove(const ITraceProvider& trace, Vec3& origin, Vec3& velocity, float dt, bool* hit_wall) const {
        Vec3 planes[5];
        int num_planes = 0;
        const Vec3 primal = velocity;
        Vec3 original = velocity;
        float time_left = dt;
        for (int bump = 0; bump < 4; ++bump) {
            if (aperture_runner::Dot(velocity, velocity) < 1e-8f) break;
            const TraceResult t = trace.TraceHull(origin, origin + velocity * time_left, cfg_.hull_mins, cfg_.hull_maxs);
            if (t.start_solid || t.all_solid) {
                velocity = {};
                return;
            }
            if (t.fraction > 0.0f) {
                origin = t.end_pos;
                original = velocity;
                num_planes = 0;
            }
            if (t.fraction >= 1.0f) break;

            if (t.normal.z < kWalkableNormalZ && hit_wall) *hit_wall = true;
            origin = origin + t.normal * kSkin;
            time_left -= time_left * t.fraction;
            if (num_planes >= 5) {
                velocity = {};
                break;
            }
            planes[num_planes++] = t.normal;

            int i = 0;
            for (; i < num_planes; ++i) {
                const Vec3 v = ClipVelocity(original, planes[i]);
                int j = 0;
                for (; j < num_planes; ++j)
                    if (j != i && aperture_runner::Dot(v, planes[j]) < 0.0f) break;
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
                const Vec3 crease = aperture_runner::Normalized(aperture_runner::Cross(planes[0], planes[1]));
                velocity = crease * aperture_runner::Dot(crease, velocity);
            }
            if (aperture_runner::Dot(velocity, primal) <= 0.0f) {
                velocity = {};
                break;
            }
        }
    }

    // Blocked by something low: retry the move from step_height up and keep it if it got further.
    void TryStepUp(const ITraceProvider& trace, Vec3 start_origin, Vec3 start_velocity, float dt,
                   FaithState& state) const {
        const TraceResult up = trace.TraceHull(start_origin, start_origin + Vec3{0, 0, cfg_.step_height},
                                               cfg_.hull_mins, cfg_.hull_maxs);
        if (up.start_solid) return;
        Vec3 origin = up.end_pos;
        if (up.Hit()) origin = origin - Vec3{0, 0, kSkin};
        const float raised = origin.z - start_origin.z;
        Vec3 velocity = start_velocity;
        SlideMove(trace, origin, velocity, dt, nullptr);

        const TraceResult down = trace.TraceHull(origin, origin - Vec3{0, 0, raised + kGroundProbe},
                                                 cfg_.hull_mins, cfg_.hull_maxs);
        if (down.start_solid || !down.Hit() || down.normal.z < kWalkableNormalZ) return;
        origin = down.end_pos + Vec3{0, 0, kSkin};

        const float direct = aperture_runner::Length(Horizontal(state.origin - start_origin));
        const float stepped = aperture_runner::Length(Horizontal(origin - start_origin));
        if (stepped > direct + 0.01f) {
            state.origin = origin;
            state.velocity = {velocity.x, velocity.y, 0.0f};
        }
    }

    // Standing means a walkable surface within kGroundProbe below the feet; snaps onto it.
    bool CheckGround(const ITraceProvider& trace, FaithState& state) const {
        return SnapDown(trace, state, kGroundProbe);
    }

    // Keeps Faith on the floor when walking down a step instead of launching her off it.
    bool StepDown(const ITraceProvider& trace, FaithState& state) const {
        return SnapDown(trace, state, cfg_.step_height + kGroundProbe);
    }

    bool SnapDown(const ITraceProvider& trace, FaithState& state, float distance) const {
        const TraceResult t = trace.TraceHull(state.origin, state.origin - Vec3{0, 0, distance},
                                              cfg_.hull_mins, cfg_.hull_maxs);
        if (t.start_solid || !t.Hit() || t.normal.z < kWalkableNormalZ) return false;
        state.origin = t.end_pos + Vec3{0, 0, kSkin};
        state.velocity.z = 0.0f;
        state.on_ground = true;
        state.ground_normal = t.normal;
        return true;
    }

    static void Land(Internal& in, FaithState& state, float impact_vz, FaithStepResult& result) {
        const float fall = in.apex_z - state.origin.z;
        result.events |= kFaithEventLanded;
        result.impact_speed = std::max(0.0f, -impact_vz);
        if (fall >= kHardLandingHeight) {
            result.events |= kFaithEventHardLanding;
            in.land_lock_left = kHardLandingLockSec;
            in.land_anim_left = 0.0f;
            state.velocity = aperture_runner::ClampLength(Horizontal(state.velocity), kHardLandingMaxSpeed);
        } else if (fall >= kMinLandingAnimHeight) {
            in.land_anim_left = kSoftLandingAnimSec;
        }
        in.jumped = 0;
        in.coyote_left = kCoyoteTimeSec;
        in.apex_z = state.origin.z;
    }

    static FaithAnimState PickAnim(const FaithState& state, const Internal& in, const FaithInput& input) {
        if (!state.on_ground) {
            if (in.jumped && state.velocity.z > 0.0f) return FaithAnimState::Jump;
            if (in.apex_z - state.origin.z > kUncontrolledFallHeight) return FaithAnimState::FallUncontrolled;
            return FaithAnimState::Fall;
        }
        if (in.land_lock_left > 0.0f) return FaithAnimState::LandHard;
        if (in.land_anim_left > 0.0f) return FaithAnimState::LandSoft;
        if (state.speed_2d < 1.0f) return FaithAnimState::Idle;
        if ((input.buttons & kFaithButtonWalk) || state.speed_2d <= kWalkSpeed + 1.0f) return FaithAnimState::Walk;
        if (state.speed_2d <= kRunSpeed + 0.5f) return FaithAnimState::Run;
        return FaithAnimState::Sprint;
    }

    FaithHostConfig cfg_;
};

}  // namespace

std::unique_ptr<IFaithMovement> CreateFaithMovement(const FaithHostConfig& config) {
    return std::make_unique<FaithMovement>(config);
}

}  // namespace ar
