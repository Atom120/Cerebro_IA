// Contract: how faith_move asks the engine about geometry.
// Owner: Sebas. Implemented by Zarek (server, Z4), Gonzalo (client prediction, G2) and by
// ar/testing/box_world.h for tests. All positions in Source world space (see units.h).
#pragma once

#include <cstdint>

#include "ar/units.h"

namespace ar {

inline constexpr uint32_t kTraceContractVersion = 1;

// What the trace hit. Wallrun, climb and ledge grab are only allowed on World / StaticProp.
enum class TraceHitKind : uint8_t {
    None = 0,     // nothing hit
    World,        // BSP brushes
    StaticProp,   // prop_static, func_brush, doors and other non-physics solids
    Physics,      // prop_physics (the cube): solid, but never wallrunnable
    Player,
    Other,
};

struct TraceResult {
    float fraction = 1.0f;  // 0..1 along start -> end where the swept shape stopped
    Vec3 end_pos;           // start + (end - start) * fraction
    Vec3 normal;            // surface normal at the hit, unit length; zero if nothing hit
    bool start_solid = false;  // the shape started inside a solid
    bool all_solid = false;    // the whole sweep is inside a solid
    TraceHitKind kind = TraceHitKind::None;

    bool Hit() const { return fraction < 1.0f || start_solid; }
};

// Every call is synchronous, has no side effects and must give the same answer for the same
// world state, because the client re-runs movement during prediction.
class ITraceProvider {
public:
    virtual ~ITraceProvider() = default;

    // Sweeps an axis-aligned box (mins/maxs relative to the origin) from start to end against
    // everything that blocks the player (Source MASK_PLAYERSOLID, ignoring the player itself).
    virtual TraceResult TraceHull(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs) const = 0;

    // Same as TraceHull with a zero-size box.
    virtual TraceResult TraceRay(Vec3 start, Vec3 end) const = 0;
};

}  // namespace ar
