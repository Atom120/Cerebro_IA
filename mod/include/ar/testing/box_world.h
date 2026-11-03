// Test helper: an ITraceProvider made of axis-aligned boxes, so faith_move, the camera and the
// plugin can be tested without a running game. Header-only, deterministic.
// Owner: Sebas.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "ar/trace_provider.h"

namespace ar::testing {

struct Box {
    Vec3 mins;
    Vec3 maxs;
    TraceHitKind kind = TraceHitKind::World;
};

class BoxWorld final : public ITraceProvider {
public:
    BoxWorld& Add(Vec3 mins, Vec3 maxs, TraceHitKind kind = TraceHitKind::World) {
        boxes_.push_back({mins, maxs, kind});
        return *this;
    }

    // Floor whose top is at z = top_z, spanning +-half_extent in X and Y.
    BoxWorld& AddFloor(float top_z = 0.0f, float half_extent = 4096.0f) {
        return Add({-half_extent, -half_extent, top_z - 16.0f}, {half_extent, half_extent, top_z});
    }

    TraceResult TraceHull(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs) const override {
        TraceResult best;
        best.end_pos = end;
        const Vec3 delta = end - start;
        for (const Box& b : boxes_) {
            // Minkowski sum: sweeping the hull against b equals sweeping a point against b grown by the hull.
            const Vec3 lo = b.mins - maxs;
            const Vec3 hi = b.maxs - mins;
            Sweep(start, delta, lo, hi, b.kind, best);
        }
        best.end_pos = start + delta * best.fraction;
        return best;
    }

    TraceResult TraceRay(Vec3 start, Vec3 end) const override {
        return TraceHull(start, end, {0, 0, 0}, {0, 0, 0});
    }

private:
    static void Sweep(Vec3 start, Vec3 delta, Vec3 lo, Vec3 hi, TraceHitKind kind, TraceResult& best) {
        const float s[3] = {start.x, start.y, start.z};
        const float d[3] = {delta.x, delta.y, delta.z};
        const float l[3] = {lo.x, lo.y, lo.z};
        const float h[3] = {hi.x, hi.y, hi.z};

        bool inside = true;
        for (int i = 0; i < 3; ++i)
            if (s[i] <= l[i] || s[i] >= h[i]) inside = false;
        if (inside) {
            best.start_solid = true;
            const Vec3 e = start + delta;
            best.all_solid = e.x > lo.x && e.x < hi.x && e.y > lo.y && e.y < hi.y && e.z > lo.z && e.z < hi.z;
            best.fraction = 0.0f;
            best.kind = kind;
            return;
        }

        // t_enter starts below 0 so a shape resting exactly on a face and moving into it hits at 0.
        float t_enter = -1.0f, t_exit = 1.0f;
        int axis = -1;
        float sign = 0.0f;
        for (int i = 0; i < 3; ++i) {
            if (std::fabs(d[i]) < 1e-9f) {
                if (s[i] <= l[i] || s[i] >= h[i]) return;  // parallel and outside the slab
                continue;
            }
            float t0 = (l[i] - s[i]) / d[i];
            float t1 = (h[i] - s[i]) / d[i];
            float face = -1.0f;  // entering through the low face: normal points to -axis
            if (t0 > t1) {
                std::swap(t0, t1);
                face = 1.0f;
            }
            if (t0 > t_enter) {
                t_enter = t0;
                axis = i;
                sign = face;
            }
            t_exit = std::min(t_exit, t1);
            if (t_enter > t_exit) return;
        }
        if (axis < 0 || t_exit <= 0.0f) return;
        t_enter = std::max(t_enter, 0.0f);
        if (t_enter >= best.fraction) return;
        best.fraction = t_enter;
        best.kind = kind;
        best.normal = {axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f, axis == 2 ? sign : 0.0f};
    }

    std::vector<Box> boxes_;
};

}  // namespace ar::testing
