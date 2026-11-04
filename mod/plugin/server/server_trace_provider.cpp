#define _CRT_SECURE_NO_WARNINGS 1
#include "server_trace_provider.h"
#include <cstring>

namespace ar {

namespace {

class TraceFilterPlayerOnly final : public source::ITraceFilter {
public:
    explicit TraceFilterPlayerOnly(const void* pass_entity) : pass_entity_(pass_entity) {}

    bool ShouldHitEntity(void* pEntity, int /*contentsMask*/) override {
        return pEntity != pass_entity_;
    }

    source::TraceType_t GetTraceType() const override {
        return source::TRACE_EVERYTHING;
    }

private:
    const void* pass_entity_ = nullptr;
};

}  // namespace

ServerTraceProvider::ServerTraceProvider(source::IEngineTrace* engine_trace, void* ignore_player)
    : engine_trace_(engine_trace), ignore_player_(ignore_player) {}

TraceResult ServerTraceProvider::TraceHull(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs) const {
    TraceResult result;
    result.end_pos = end;

    if (!engine_trace_) {
        return result;
    }

    source::Ray_t ray;
    ray.Init(start, end, mins, maxs);

    TraceFilterPlayerOnly filter(ignore_player_);
    source::trace_t tr;
    std::memset(&tr, 0, sizeof(tr));

    engine_trace_->TraceRay(ray, source::kMaskPlayerSolid, &filter, &tr);

    result.fraction = tr.fraction;
    result.end_pos = Vec3(tr.endpos[0], tr.endpos[1], tr.endpos[2]);
    result.normal = Vec3(tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]);
    result.start_solid = tr.startsolid;
    result.all_solid = tr.allsolid;

    if (result.Hit()) {
        if (!tr.m_pEnt) {
            result.kind = TraceHitKind::World;
        } else {
            result.kind = TraceHitKind::StaticProp;
        }
    } else {
        result.kind = TraceHitKind::None;
    }

    return result;
}

TraceResult ServerTraceProvider::TraceRay(Vec3 start, Vec3 end) const {
    TraceResult result;
    result.end_pos = end;

    if (!engine_trace_) {
        return result;
    }

    source::Ray_t ray;
    ray.Init(start, end);

    TraceFilterPlayerOnly filter(ignore_player_);
    source::trace_t tr;
    std::memset(&tr, 0, sizeof(tr));

    engine_trace_->TraceRay(ray, source::kMaskPlayerSolid, &filter, &tr);

    result.fraction = tr.fraction;
    result.end_pos = Vec3(tr.endpos[0], tr.endpos[1], tr.endpos[2]);
    result.normal = Vec3(tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]);
    result.start_solid = tr.startsolid;
    result.all_solid = tr.allsolid;

    if (result.Hit()) {
        if (!tr.m_pEnt) {
            result.kind = TraceHitKind::World;
        } else {
            result.kind = TraceHitKind::StaticProp;
        }
    } else {
        result.kind = TraceHitKind::None;
    }

    return result;
}

}  // namespace ar
