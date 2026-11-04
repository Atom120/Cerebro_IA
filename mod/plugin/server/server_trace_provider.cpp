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

bool StartsWith(const char* s, const char* prefix) { return std::strncmp(s, prefix, std::strlen(prefix)) == 0; }

#ifdef _WIN32
// Reads entity->GetNetworkable()->GetClassName() through the SDK 2013 vtables. Guarded: a bad
// pointer returns nullptr instead of taking the game down.
const char* ReadClassName(void* entity) {
    typedef void*(__thiscall * GetNetworkableFn)(void*);
    typedef const char*(__thiscall * GetClassNameFn)(void*);
    __try {
        void** vt = *reinterpret_cast<void***>(entity);
        void* networkable = reinterpret_cast<GetNetworkableFn>(vt[source::kServerUnknownGetNetworkableSlot])(entity);
        if (!networkable) return nullptr;
        void** nvt = *reinterpret_cast<void***>(networkable);
        return reinterpret_cast<GetClassNameFn>(nvt[source::kServerNetworkableGetClassNameSlot])(networkable);
    } __except (1) {
        return nullptr;
    }
}
#else
const char* ReadClassName(void*) { return nullptr; }
#endif

}  // namespace

TraceHitKind ClassifyHitEntity(void* entity) {
    if (!entity) return TraceHitKind::World;
    const char* name = ReadClassName(entity);
    if (!name) return TraceHitKind::Other;
    // Static props are part of the world entity in Source, so they arrive here as "worldspawn".
    if (std::strcmp(name, "worldspawn") == 0) return TraceHitKind::World;
    if (std::strcmp(name, "player") == 0) return TraceHitKind::Player;
    if (StartsWith(name, "prop_physics") || StartsWith(name, "func_physbox") || StartsWith(name, "prop_ragdoll") ||
        StartsWith(name, "physics_prop") || StartsWith(name, "npc_")) {
        return TraceHitKind::Physics;  // the cube (prop_physics), turrets...: solid, never wallrunnable
    }
    return TraceHitKind::StaticProp;  // func_door, func_brush, prop_dynamic, elevators...
}

ServerTraceProvider::ServerTraceProvider(source::IEngineTrace* engine_trace, void* ignore_player)
    : engine_trace_(engine_trace), ignore_player_(ignore_player) {}

TraceResult ServerTraceProvider::TraceHull(Vec3 start, Vec3 end, Vec3 mins, Vec3 maxs) const {
    source::Ray_t ray;
    ray.Init(start, end, mins, maxs);
    return Run(ray, end);
}

TraceResult ServerTraceProvider::TraceRay(Vec3 start, Vec3 end) const {
    source::Ray_t ray;
    ray.Init(start, end);
    return Run(ray, end);
}

TraceResult ServerTraceProvider::Run(const source::Ray_t& ray, Vec3 end) const {
    TraceResult result;
    result.end_pos = end;
    if (!engine_trace_) return result;

    TraceFilterPlayerOnly filter(ignore_player_);
    source::trace_t tr;
    std::memset(&tr, 0, sizeof(tr));
    engine_trace_->TraceRay(ray, source::kMaskPlayerSolid, &filter, &tr);

    result.fraction = tr.fraction;
    result.end_pos = Vec3(tr.endpos[0], tr.endpos[1], tr.endpos[2]);
    result.normal = Vec3(tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]);
    result.start_solid = tr.startsolid;
    result.all_solid = tr.allsolid;
    result.kind = result.Hit() ? ClassifyHitEntity(tr.m_pEnt) : TraceHitKind::None;
    return result;
}

}  // namespace ar
