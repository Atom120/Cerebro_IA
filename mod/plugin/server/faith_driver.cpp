#define _CRT_SECURE_NO_WARNINGS 1
#include "faith_driver.h"
#include "faith_viewmodel.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "client/client_view.h"
#include "server_plugin.h"

namespace ar {

namespace {

constexpr float kPortalHalfWidth = aperture_runner::kPortalHalfWidthSu;    // 32
constexpr float kPortalHalfHeight = aperture_runner::kPortalHalfHeightSu;  // 54
// How close to a portal the original Portal movement takes over (su, from the player's center): only
// where the hull (16 su half-width) can overlap the portal hole, so normal walking nearby stays Faith's.
constexpr float kPortalFrontReach = 40.0f;
constexpr float kPortalBackReach = 40.0f;
constexpr float kPortalSideMargin = 8.0f;
constexpr float kPortalVerticalMargin = 16.0f;
// A jump bigger than this between ticks means the engine teleported the player.
constexpr float kTeleportDistance = 32.0f;

typedef void*(__thiscall* PtrFn)(void*);

#ifdef _WIN32
const char* EdictClassName(void* unknown) {
    __try {
        void** vt = *reinterpret_cast<void***>(unknown);
        void* net = reinterpret_cast<PtrFn>(vt[source::kServerUnknownGetNetworkableSlot])(unknown);
        if (!net) return nullptr;
        void** nvt = *reinterpret_cast<void***>(net);
        return reinterpret_cast<const char*(__thiscall*)(void*)>(nvt[source::kServerNetworkableGetClassNameSlot])(net);
    } __except (1) {
        return nullptr;
    }
}

bool PortalTransform(void* unknown, float origin[3], float angles[3]) {
    __try {
        void** vt = *reinterpret_cast<void***>(unknown);
        void* col = reinterpret_cast<PtrFn>(vt[source::kServerUnknownGetCollideableSlot])(unknown);
        if (!col) return false;
        void** cvt = *reinterpret_cast<void***>(col);
        const float* o = static_cast<const float*>(reinterpret_cast<PtrFn>(cvt[source::kCollideableGetOriginSlot])(col));
        const float* a = static_cast<const float*>(reinterpret_cast<PtrFn>(cvt[source::kCollideableGetAnglesSlot])(col));
        std::memcpy(origin, o, 3 * sizeof(float));
        std::memcpy(angles, a, 3 * sizeof(float));
        return true;
    } __except (1) {
        return false;
    }
}

// Open portals only: a closed prop_portal is just a wall, which Faith handles herself. If the flag
// cannot be read the portal counts as open (the safe side: Portal's movement takes over).
bool PortalActive(void* unknown) {
    __try {
        void** vt = *reinterpret_cast<void***>(unknown);
        const unsigned char* entity = static_cast<const unsigned char*>(
            reinterpret_cast<PtrFn>(vt[source::kServerUnknownGetBaseEntitySlot])(unknown));
        return !entity || entity[source::kPortalActivatedOffset] != 0;
    } __except (1) {
        return true;
    }
}
#else
const char* EdictClassName(void*) { return nullptr; }
bool PortalTransform(void*, float*, float*) { return false; }
bool PortalActive(void*) { return true; }
#endif

uint32_t MapButtons(int in) {
    uint32_t b = 0;
    if (in & source::kInJump) b |= kFaithButtonUp;
    if (in & source::kInDuck) b |= kFaithButtonDown;
    if (in & source::kInReload) b |= kFaithButtonTurn180;
    if (in & source::kInWalk) b |= kFaithButtonWalk;
    return b;
}

float Axis(float move) { return std::clamp(move / source::kForwardSpeed, -1.0f, 1.0f); }

}  // namespace

FaithDriver& FaithDriver::Instance() {
    static FaithDriver driver;
    return driver;
}

void FaithDriver::SetEnabled(bool enabled) {
    if (enabled && !enabled_) initialized_ = false;  // re-sync from the engine on the next tick
    enabled_ = enabled;
}

void FaithDriver::ScanPortals(void* edict_list, int edict_count) {
    portals_.clear();
    if (!edict_list) return;
    for (int i = 0; i < edict_count; ++i) {
        char* edict = static_cast<char*>(edict_list) + i * source::kEdictSize;
        void* unknown = *reinterpret_cast<void**>(edict + source::kEdictUnknownOffset);
        if (!unknown) continue;
        const char* name = EdictClassName(unknown);
        if (name && std::strcmp(name, "prop_portal") == 0) portals_.push_back(unknown);
    }
}

void FaithDriver::OnLevelShutdown() {
    portals_.clear();
    initialized_ = false;
}

bool FaithDriver::NearPortal(Vec3 feet) const {
    const Vec3 center = feet + Vec3{0, 0, 36.0f};
    for (void* portal : portals_) {
        float o[3], a[3];
        if (!PortalActive(portal) || !PortalTransform(portal, o, a)) continue;
        const float p = a[0] * aperture_runner::kDegToRad, y = a[1] * aperture_runner::kDegToRad;
        const Vec3 fwd{std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p)};
        const Vec3 right{std::sin(y), -std::cos(y), 0.0f};
        const Vec3 up{std::sin(p) * std::cos(y), std::sin(p) * std::sin(y), std::cos(p)};
        const Vec3 d = center - Vec3{o[0], o[1], o[2]};
        const float along = aperture_runner::Dot(d, fwd);
        if (along < -kPortalBackReach || along > kPortalFrontReach) continue;
        if (std::fabs(aperture_runner::Dot(d, right)) > kPortalHalfWidth + kPortalSideMargin) continue;
        if (std::fabs(aperture_runner::Dot(d, up)) > kPortalHalfHeight + kPortalVerticalMargin) continue;
        return true;
    }
    return false;
}

// Basis of the portal closest to p: columns forward, left, up (Source AngleMatrix).
bool FaithDriver::NearestPortalBasis(Vec3 p, Mat3* basis) const {
    float best = 1e30f;
    bool found = false;
    for (void* portal : portals_) {
        float o[3], a[3];
        if (!PortalTransform(portal, o, a)) continue;
        const float d = aperture_runner::Length(p - Vec3{o[0], o[1], o[2]});
        if (d >= best) continue;
        best = d;
        found = true;
        const float pi = a[0] * aperture_runner::kDegToRad, ya = a[1] * aperture_runner::kDegToRad;
        const float sp = std::sin(pi), cp = std::cos(pi), sy = std::sin(ya), cy = std::cos(ya);
        *basis = Mat3::FromColumns({cp * cy, cp * sy, -sp}, {-sy, cy, 0.0f}, {sp * cy, sp * sy, cp});
    }
    return found;
}

// P4: picks the player back up after ticks the original Portal movement ran.
//  * Through a portal: Faith's state (wallrun, slide...) is kept, its vectors are rotated by the
//    portal pair (FaithInput::portal), and the speed she had going in is restored: Portal's ground
//    friction would otherwise eat up to ~40% of a sprint in the few ticks near the portal.
//  * Out of a solid (stuck): the state is reset from the engine.
void FaithDriver::Sync(const source::CMoveData* mv, PortalTransportHint* hint) {
    const Vec3 origin{mv->m_vecAbsOrigin[0], mv->m_vecAbsOrigin[1], mv->m_vecAbsOrigin[2]};
    Vec3 velocity{mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]};
    const bool resume = last_tick_portal_ && !last_tick_stuck_;
    if (!initialized_ || last_tick_stuck_ ||
        (!resume && aperture_runner::Length(origin - state_.origin) > kTeleportDistance)) {
        move_->Reset(state_, origin, velocity);
        initialized_ = true;
        return;
    }
    if (resume) {
        const float speed = aperture_runner::Length(velocity);
        if (speed > 1.0f && speed < portal_entry_speed_) velocity = velocity * (portal_entry_speed_ / speed);
        if (portal_teleported_) {
            Mat3 in_basis, out_basis;
            if (NearestPortalBasis(portal_in_origin_, &in_basis) && NearestPortalBasis(origin, &out_basis)) {
                hint->teleported = true;
                hint->rotation = out_basis * Mat3::Diagonal(-1.0f, -1.0f, 1.0f) * in_basis.Transposed();
                ++portal_crossings_;
            }
        }
    }
    state_.origin = origin;
    state_.velocity = velocity;
}

void FaithDriver::Process(void* game_movement, void* player, source::CMoveData* mv, ProcessMovementFn original,
                          const ITraceProvider& trace) {
    last_player_ = player;
    if (!enabled_ || !mv) {
        RestorePlayerView();
        FaithViewModel::Instance().Update(player, FaithAnimState::Idle, false);
        if (original) original(game_movement, player, mv);
        return;
    }
    if (!move_) move_ = CreateFaithMovement(FaithHostConfig{});

    // QA input override: written into CMoveData so the original movement (portal ticks) sees it too.
    // The buttons are held from qa_press_tick_ on and count as pressed on that tick only.
    const bool qa_active = qa_ticks_left_ > 0;
    const uint32_t qa_held = qa_active && qa_tick_ >= qa_press_tick_ ? qa_buttons_ : 0u;
    const uint32_t qa_pressed = qa_active && qa_tick_ == qa_press_tick_ ? qa_buttons_ : 0u;
    if (qa_active) {
        --qa_ticks_left_;
        ++qa_tick_;
        mv->m_flForwardMove = qa_forward_ * source::kForwardSpeed;
        mv->m_flSideMove = qa_side_ * source::kForwardSpeed;
        if (qa_held & kFaithButtonUp) mv->m_nButtons |= source::kInJump;
        if (qa_held & kFaithButtonDown) mv->m_nButtons |= source::kInDuck;
    }

    const Vec3 feet{mv->m_vecAbsOrigin[0], mv->m_vecAbsOrigin[1], mv->m_vecAbsOrigin[2]};
    engine_origin_ = feet;
    // The engine teleports through a prop_portal when the player touches it, between ticks (after
    // ProcessMovement), so a portal crossing shows up as a jump from where the last tick ended.
    if (last_tick_portal_ && !portal_teleported_ &&
        aperture_runner::Length(feet - last_end_origin_) > kTeleportDistance) {
        portal_teleported_ = true;
        portal_in_origin_ = last_end_origin_;
    }
    const FaithHostConfig cfg;
    const bool stuck = trace.TraceHull(feet, feet, cfg.hull_mins, cfg.hull_maxs).start_solid;
    if (stuck || NearPortal(feet)) {
        // Let Portal carry the player through the portal (or out of a solid, CheckStuck); pick the
        // result up when the player is clear (Sync).
        if (!last_tick_portal_) {
            portal_entry_speed_ = initialized_ ? aperture_runner::Length(state_.velocity) : 0.0f;
            portal_teleported_ = false;
        }
        StripHeadBob(player);  // Portal's own movement would "restore" a bobbed view height (and log it)
        if (original) original(game_movement, player, mv);
        last_end_origin_ = {mv->m_vecAbsOrigin[0], mv->m_vecAbsOrigin[1], mv->m_vecAbsOrigin[2]};
        last_tick_portal_ = true;
        last_tick_stuck_ = stuck;
        ++portal_ticks_;
        return;
    }
    PortalTransportHint hint;
    Sync(mv, &hint);
    last_tick_portal_ = false;
    last_tick_stuck_ = false;

    FaithInput in;
    in.dt = source_defaults::kTickInterval;
    in.time = time_;
    in.forward_move = Axis(mv->m_flForwardMove);
    in.side_move = Axis(mv->m_flSideMove);
    in.view_angles = {mv->m_vecViewAngles[0], mv->m_vecViewAngles[1], mv->m_vecViewAngles[2]};
    in.portal = hint;
    in.buttons = MapButtons(mv->m_nButtons);
    in.buttons_pressed = MapButtons(mv->m_nButtons & ~mv->m_nOldButtons);
    if (qa_active) {
        in.buttons = qa_held;
        in.buttons_pressed = qa_pressed;
    }

    const FaithStepResult r = move_->Step(in, state_, trace);
    time_ += in.dt;
    ++faith_ticks_;
    last_events_ |= r.events;

    // The original movement still runs for the engine's side effects (duck hull + FL_DUCKING + eye
    // height, ground entity, footsteps), with IN_DUCK matching Faith; then Faith's result replaces
    // the position and velocity it computed.
    if (original && engine_side_effects_) {
        mv->m_nButtons = (mv->m_nButtons & ~source::kInDuck) | (state_.ducked ? source::kInDuck : 0);
        StripHeadBob(player);
        original(game_movement, player, mv);
    }
    for (int i = 0; i < 3; ++i) {
        mv->m_vecVelocity[i] = (&state_.velocity.x)[i];
        mv->m_vecAbsOrigin[i] = (&state_.origin.x)[i];
    }
    last_end_origin_ = state_.origin;
    ApplyCamera(r);
    FaithViewModel::Instance().Update(player, state_.anim, camera_enabled_);

    if (debug_ >= 2 || (debug_ == 1 && (faith_ticks_ % 33 == 0 || r.events))) {
        ApertureRunnerServerPlugin::LogMsg("[AR] t=%.2f anim=%s vel2d=%.1f vz=%.1f suelo=%d pos=(%.1f %.1f %.1f) eventos=0x%x yaw=%.1f roll=%.1f\n",
                                           time_, SequenceName(state_.anim), state_.speed_2d, state_.velocity.z,
                                           state_.on_ground ? 1 : 0, state_.origin.x, state_.origin.y,
                                           state_.origin.z, r.events, in.view_angles.y, in.view_angles.z);
    }
}

// G4 (camera part): mod/camera drives the local player's view through the client engine. Yaw gets
// the 180 turn as per-tick deltas, roll is set absolutely (wallrun lean), and pitch gets the landing
// shake as a delta that is taken back next tick. The Skill Roll somersault is left out: the engine
// clamps view pitch to +-89 degrees.
void FaithDriver::ApplyCamera(const FaithStepResult& r) {
    const camera::CameraOutput co =
        camera::UpdateCamera(camera_, state_, r.events, r.impact_speed, source_defaults::kTickInterval);
    if (!camera_enabled_) {
        RestorePlayerView();
        return;
    }
    if (ClientView::Ready()) {
        float view[3];
        if (ClientView::GetViewAngles(view)) {
            const float shake = camera_.roll_left > 0.0f ? 0.0f : std::clamp(co.pitch_offset, -12.0f, 12.0f);
            view[0] += shake - applied_pitch_;
            view[1] += co.yaw_delta;
            view[2] = co.roll;
            applied_pitch_ = shake;
            ClientView::SetViewAngles(view);
            view_yaw_ = view[1];
        }
    }
    ApplyPlayerView(co);
}

// FOV and head bob go through the player's networked fields (m_iFOV, m_vecViewOffset). The mod only
// writes a field while it still holds the value the mod wrote (or the default), so Portal's own
// zoom or a duck transition always win.
void FaithDriver::ApplyPlayerView(const camera::CameraOutput& co) {
    if (!last_player_) return;
    char* p = static_cast<char*>(last_player_);
    // The base FOV is the player's own m_iFOV when the mod first takes it over (their fov setting).
    // The mod only writes while m_iFOV still holds its last value or that base (Portal's zoom wins),
    // and never leaves [base, base + 15] or [60, 110].
    int& fov = *reinterpret_cast<int*>(p + source::kPlayerFovOffset);
    if (written_fov_ == 0) base_fov_ = (fov >= 60 && fov <= 110) ? fov : 75;
    if (fov == written_fov_ || fov == base_fov_) {
        const float extra = std::clamp(co.fov - camera::CameraParams{}.fov_base, 0.0f, 15.0f);
        const int want = std::clamp(static_cast<int>(std::lround(static_cast<float>(base_fov_) + extra)), 60, 110);
        fov = want;
        written_fov_ = want;
    }

    float* offset = reinterpret_cast<float*>(p + source::kPlayerViewOffsetOffset);
    const FaithHostConfig cfg;
    const float base = cfg.view_height;
    // The engine's own movement (still run for its side effects) puts a standing player back at
    // exactly view_height every tick; that value, or the one the mod wrote, is ours to replace.
    const bool owned = std::fabs(offset[2] - base) < 0.01f || std::fabs(offset[2] - (base + written_bob_[2])) < 0.01f;
    if (!state_.ducked && owned) {
        const float yaw = view_yaw_ * aperture_runner::kDegToRad;
        const float side = co.eye_offset.x;  // along the view's right axis
        written_bob_[0] = std::sin(yaw) * side;
        written_bob_[1] = -std::cos(yaw) * side;
        written_bob_[2] = co.eye_offset.z;
        offset[0] = written_bob_[0];
        offset[1] = written_bob_[1];
        offset[2] = base + written_bob_[2];
    } else if (state_.ducked && written_bob_[2] != 0.0f) {
        offset[0] = 0.0f;
        offset[1] = 0.0f;
        written_bob_[0] = written_bob_[1] = written_bob_[2] = 0.0f;
    }
}

// Takes the head bob out of m_vecViewOffset before the engine's own movement runs: the engine
// "restores" any standing view height that is not exactly view_height (and logs it every tick).
// ApplyPlayerView puts it back afterwards.
void FaithDriver::StripHeadBob(void* player) {
    if (!player || (written_bob_[0] == 0.0f && written_bob_[1] == 0.0f && written_bob_[2] == 0.0f)) return;
    float* offset = reinterpret_cast<float*>(static_cast<char*>(player) + source::kPlayerViewOffsetOffset);
    if (std::fabs(offset[2] - (FaithHostConfig{}.view_height + written_bob_[2])) < 0.01f) {
        offset[0] = 0.0f;
        offset[1] = 0.0f;
        offset[2] = FaithHostConfig{}.view_height;
    }
}

// Puts FOV and eye offset back when Faith or her camera is turned off.
void FaithDriver::RestorePlayerView() {
    if (!last_player_ || (written_fov_ == 0 && written_bob_[2] == 0.0f && written_bob_[0] == 0.0f)) return;
    char* p = static_cast<char*>(last_player_);
    int& fov = *reinterpret_cast<int*>(p + source::kPlayerFovOffset);
    if (written_fov_ != 0 && fov == written_fov_ && base_fov_ != 0) fov = base_fov_;
    float* offset = reinterpret_cast<float*>(p + source::kPlayerViewOffsetOffset);
    const float stand = FaithHostConfig{}.view_height;
    if (std::fabs(offset[2] - stand) < 0.01f || std::fabs(offset[2] - (stand + written_bob_[2])) < 0.01f) {
        offset[0] = 0.0f;
        offset[1] = 0.0f;
        offset[2] = FaithHostConfig{}.view_height;
    }
    written_fov_ = 0;
    written_bob_[0] = written_bob_[1] = written_bob_[2] = 0.0f;
}

void FaithDriver::FindLedges(const ITraceProvider& trace, float x0, float y0, float x1, float y1, float z,
                             float step) const {
    const FaithHostConfig cfg;
    const Vec3 mins = cfg.hull_mins, maxs = cfg.hull_maxs;
    const float reach_up = maxs.z + 24.0f;
    int found = 0;
    for (float x = x0; x <= x1 && found < 40; x += step) {
        for (float y = y0; y <= y1 && found < 40; y += step) {
            const Vec3 p{x, y, z + 1.0f};
            if (trace.TraceHull(p, p, mins, maxs).start_solid) continue;
            const TraceResult floor = trace.TraceHull(p, p - Vec3{0, 0, 8}, mins, maxs);
            if (!floor.Hit() || floor.normal.z < 0.7f) continue;
            const Vec3 feet = floor.end_pos + Vec3{0, 0, 0.1f};
            for (int d = 0; d < 4; ++d) {
                const float yaw = 90.0f * static_cast<float>(d);
                const Vec3 dir{std::cos(yaw * aperture_runner::kDegToRad), std::sin(yaw * aperture_runner::kDegToRad), 0};
                const TraceResult wall = trace.TraceHull(feet, feet + dir * 24.0f, mins, maxs);
                if (!wall.Hit() || wall.start_solid || std::fabs(wall.normal.z) > 0.3f) continue;
                const TraceResult up = trace.TraceHull(feet, feet + Vec3{0, 0, reach_up}, mins, maxs);
                if (up.Hit()) continue;
                const Vec3 into = Vec3{0, 0, 0} - wall.normal;  // like faith_move: over the edge along the wall normal
                // Same order as faith_move: first walkable top over the edge, then the climb path.
                float h = -1.0f;
                Vec3 target;
                const float gap = wall.fraction * 24.0f;
                for (const float forward : {gap + 2.0f * maxs.x + 1.0f, 36.0f, 24.0f, 48.0f}) {  // as faith_move
                    const TraceResult over = trace.TraceHull(up.end_pos, up.end_pos + into * forward, mins, maxs);
                    if (over.Hit()) continue;
                    const TraceResult down = trace.TraceHull(over.end_pos, over.end_pos - Vec3{0, 0, reach_up}, mins, maxs);
                    if (down.start_solid || !down.Hit() || down.normal.z < 0.7f) continue;
                    target = down.end_pos;
                    if (target.z - feet.z > 4.0f) {
                        h = target.z - feet.z;
                        break;
                    }
                }
                const Vec3 top{feet.x, feet.y, target.z + 2.0f}, across{target.x, target.y, target.z + 2.0f};
                if (h < 0.0f || trace.TraceHull(feet, top, mins, maxs).Hit() || trace.TraceHull(top, across, mins, maxs).Hit()) continue;
                const char* kind = h > 18.0f && h <= 48.0f ? "VAULT" : (h >= 40.0f && h <= 96.0f ? "BORDE" : nullptr);
                if (!kind) continue;
                ApertureRunnerServerPlugin::LogMsg("[AR-LEDGE] %s pos=(%.0f %.0f %.1f) yaw=%.0f altura=%.1f muro_a=%.1f kind=%d\n", kind,
                                                   feet.x, feet.y, feet.z, yaw, h, wall.fraction * 24.0f, static_cast<int>(wall.kind));
                ++found;
            }
        }
    }
    ApertureRunnerServerPlugin::LogMsg("[AR-LEDGE] fin (%d sitios)\n", found);
}

void FaithDriver::SetQaInput(float forward, float side, uint32_t buttons, int ticks, int press_tick) {
    qa_forward_ = std::clamp(forward, -1.0f, 1.0f);
    qa_side_ = std::clamp(side, -1.0f, 1.0f);
    qa_buttons_ = buttons;
    qa_ticks_left_ = std::max(0, ticks);
    qa_tick_ = 0;
    qa_press_tick_ = std::max(0, press_tick);
}

void FaithDriver::PrintStatus() const {
    FaithViewModel::Instance().PrintStatus();
    if (last_player_) {
        const char* p = static_cast<const char*>(last_player_);
        const float* off = reinterpret_cast<const float*>(p + source::kPlayerViewOffsetOffset);
        ApertureRunnerServerPlugin::LogMsg("[AR] jugador: m_iFOV=%d fov_base=%d fov_escrito=%d vista_offset=(%.2f %.2f %.2f)\n",
                                           *reinterpret_cast<const int*>(p + source::kPlayerFovOffset), base_fov_, written_fov_,
                                           off[0], off[1], off[2]);
    }
    ApertureRunnerServerPlugin::LogMsg(
        "[AR] ar_enable=%d anim=%s vel2d=%.1f vz=%.1f suelo=%d agachada=%d pos=(%.1f %.1f %.1f) motor=(%.1f %.1f %.1f) "
        "ticks_faith=%llu ticks_portal=%llu portales=%u cruces=%llu eventos_acumulados=0x%x\n",
        enabled_ ? 1 : 0, SequenceName(state_.anim), state_.speed_2d, state_.velocity.z, state_.on_ground ? 1 : 0,
        state_.ducked ? 1 : 0, state_.origin.x, state_.origin.y, state_.origin.z, engine_origin_.x, engine_origin_.y,
        engine_origin_.z, faith_ticks_, portal_ticks_,
        static_cast<unsigned>(portals_.size()), portal_crossings_, last_events_);
}

}  // namespace ar
