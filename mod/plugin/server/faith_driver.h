#pragma once

// Z5: drives the Portal player with faith_move when ar_enable is on. Called from the
// ProcessMovement hook (Z3) with the engine's CMoveData; traces go through ServerTraceProvider (Z4).
//
// Each tick the FaithState is synced from the engine's origin/velocity (so teleports, elevators and
// portal crossings done by the engine are picked up), faith_move steps it, and the result is written
// back into CMoveData. Near a prop_portal the original Portal movement runs instead, because plain
// engine traces do not see through portals (CPortalGameMovement uses portal-aware traces).

#include <cstdint>
#include <memory>
#include <vector>

#include "ar/faith_movement.h"
#include "source_interfaces.h"

namespace ar {

class FaithDriver {
public:
    typedef void(__thiscall* ProcessMovementFn)(void* thisptr, void* player, void* move);

    static FaithDriver& Instance();

    void SetEnabled(bool enabled);
    bool Enabled() const { return enabled_; }
    void SetDebug(bool debug) { debug_ = debug; }

    // Finds the prop_portal entities of the map (ServerActivate) / forgets them (LevelShutdown).
    void ScanPortals(void* edict_list, int edict_count);
    void OnLevelShutdown();

    void Process(void* game_movement, void* player, source::CMoveData* mv, ProcessMovementFn original,
                 const ITraceProvider& trace);

    void PrintStatus() const;
    size_t PortalCount() const { return portals_.size(); }

private:
    bool NearPortal(Vec3 feet) const;
    void Sync(const source::CMoveData* mv);

    std::unique_ptr<IFaithMovement> move_;
    FaithState state_{};
    Vec3 engine_origin_;  // last origin the engine passed in (for ar_status)
    std::vector<void*> portals_;  // IServerUnknown* of each prop_portal
    bool enabled_ = false;
    bool debug_ = false;
    bool initialized_ = false;
    bool last_tick_portal_ = false;
    double time_ = 0.0;
    uint64_t faith_ticks_ = 0;
    uint64_t portal_ticks_ = 0;
    uint32_t last_events_ = 0;
};

}  // namespace ar
