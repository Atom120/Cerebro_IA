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
#include "camera/faith_camera.h"
#include "source_interfaces.h"

namespace ar {

class FaithDriver {
public:
    typedef void(__thiscall* ProcessMovementFn)(void* thisptr, void* player, void* move);

    static FaithDriver& Instance();

    void SetEnabled(bool enabled);
    bool Enabled() const { return enabled_; }
    void SetDebug(int level) { debug_ = level; }  // 0 off, 1 twice a second + events, 2 every tick

    // Finds the prop_portal entities of the map (ServerActivate) / forgets them (LevelShutdown).
    void ScanPortals(void* edict_list, int edict_count);
    void OnLevelShutdown();

    void Process(void* game_movement, void* player, source::CMoveData* mv, ProcessMovementFn original,
                 const ITraceProvider& trace);

    // QA (B3): for `ticks` ticks use this input instead of the player's, so automated tests do not
    // depend on the Portal window having keyboard focus. buttons: FaithButton bits, pressed on press_tick and held after it.
    void SetQaInput(float forward, float side, uint32_t buttons, int ticks, int press_tick = 0);

    // Map audit (QA): logs vault/ledge spots in a grid at floor height z (ar_find_ledges).
    void FindLedges(const ITraceProvider& trace, float x0, float y0, float x1, float y1, float z, float step) const;

    void PrintStatus() const;
    size_t PortalCount() const { return portals_.size(); }
    void SetCamera(bool enabled) { camera_enabled_ = enabled; }
    bool CameraEnabled() const { return camera_enabled_; }

private:
    void ApplyCamera(const FaithStepResult& r);
    void ApplyPlayerView(const camera::CameraOutput& co);
    void RestorePlayerView();

    bool NearPortal(Vec3 feet) const;
    bool NearestPortalBasis(Vec3 p, Mat3* basis) const;
    void Sync(const source::CMoveData* mv, PortalTransportHint* hint);

    std::unique_ptr<IFaithMovement> move_;
    FaithState state_{};
    camera::CameraState camera_{};
    bool camera_enabled_ = true;
    bool engine_side_effects_ = true;  // run the original movement too (duck hull, ground, footsteps)
    float applied_pitch_ = 0.0f;  // landing-shake pitch currently added to the view
    float view_yaw_ = 0.0f;       // last view yaw (for the sideways head bob)
    int written_fov_ = 0;         // m_iFOV the mod last wrote (0 = none)
    int base_fov_ = 0;            // the player's own FOV when the mod took it over
    float written_bob_[3] = {};   // head-bob offset the mod last added to m_vecViewOffset
    Vec3 engine_origin_;  // last origin the engine passed in (for ar_status)
    void* last_player_ = nullptr;  // CBasePlayer* of the last ProcessMovement
    std::vector<void*> portals_;  // IServerUnknown* of each prop_portal
    bool enabled_ = false;
    int debug_ = 0;
    bool initialized_ = false;
    bool last_tick_portal_ = false;   // the original movement ran last tick (portal or stuck)
    bool last_tick_stuck_ = false;
    bool portal_teleported_ = false;  // the engine teleported the player during those ticks
    float portal_entry_speed_ = 0.0f; // Faith's speed when the portal ticks began
    Vec3 portal_in_origin_;           // where the player was just before the teleport
    Vec3 last_end_origin_;            // player origin when the previous tick ended
    uint64_t portal_crossings_ = 0;
    double time_ = 0.0;
    uint64_t faith_ticks_ = 0;
    uint64_t portal_ticks_ = 0;
    uint32_t last_events_ = 0;
    float qa_forward_ = 0.0f;
    float qa_side_ = 0.0f;
    uint32_t qa_buttons_ = 0;
    int qa_ticks_left_ = 0;
    int qa_tick_ = 0;        // ticks of the current QA input already used
    int qa_press_tick_ = 0;  // tick on which the QA buttons are pressed
};

}  // namespace ar
