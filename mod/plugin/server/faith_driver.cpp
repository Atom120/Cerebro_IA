#define _CRT_SECURE_NO_WARNINGS 1
#include "faith_driver.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
#else
const char* EdictClassName(void*) { return nullptr; }
bool PortalTransform(void*, float*, float*) { return false; }
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
        if (!PortalTransform(portal, o, a)) continue;
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

void FaithDriver::Sync(const source::CMoveData* mv) {
    const Vec3 origin{mv->m_vecAbsOrigin[0], mv->m_vecAbsOrigin[1], mv->m_vecAbsOrigin[2]};
    const Vec3 velocity{mv->m_vecVelocity[0], mv->m_vecVelocity[1], mv->m_vecVelocity[2]};
    if (!initialized_ || last_tick_portal_ || aperture_runner::Length(origin - state_.origin) > kTeleportDistance) {
        move_->Reset(state_, origin, velocity);
        initialized_ = true;
    } else {
        state_.origin = origin;
        state_.velocity = velocity;
    }
}

void FaithDriver::Process(void* game_movement, void* player, source::CMoveData* mv, ProcessMovementFn original,
                          const ITraceProvider& trace) {
    if (!enabled_ || !mv) {
        if (original) original(game_movement, player, mv);
        return;
    }
    if (!move_) move_ = CreateFaithMovement(FaithHostConfig{});

    // QA input override: written into CMoveData so the original movement (portal ticks) sees it too.
    const bool qa_active = qa_ticks_left_ > 0;
    const uint32_t qa_pressed = qa_first_tick_ ? qa_buttons_ : 0u;
    if (qa_active) {
        --qa_ticks_left_;
        qa_first_tick_ = false;
        mv->m_flForwardMove = qa_forward_ * source::kForwardSpeed;
        mv->m_flSideMove = qa_side_ * source::kForwardSpeed;
        if (qa_buttons_ & kFaithButtonUp) mv->m_nButtons |= source::kInJump;
        if (qa_buttons_ & kFaithButtonDown) mv->m_nButtons |= source::kInDuck;
    }

    const Vec3 feet{mv->m_vecAbsOrigin[0], mv->m_vecAbsOrigin[1], mv->m_vecAbsOrigin[2]};
    engine_origin_ = feet;
    const FaithHostConfig cfg;
    const bool stuck = trace.TraceHull(feet, feet, cfg.hull_mins, cfg.hull_maxs).start_solid;
    if (stuck || NearPortal(feet)) {
        // Let Portal carry the player through the portal (or out of a solid, CheckStuck); pick the
        // result up next tick.
        if (original) original(game_movement, player, mv);
        last_tick_portal_ = true;
        ++portal_ticks_;
        return;
    }
    Sync(mv);
    last_tick_portal_ = false;

    FaithInput in;
    in.dt = source_defaults::kTickInterval;
    in.time = time_;
    in.forward_move = Axis(mv->m_flForwardMove);
    in.side_move = Axis(mv->m_flSideMove);
    in.view_angles = {mv->m_vecViewAngles[0], mv->m_vecViewAngles[1], mv->m_vecViewAngles[2]};
    in.buttons = MapButtons(mv->m_nButtons);
    in.buttons_pressed = MapButtons(mv->m_nButtons & ~mv->m_nOldButtons);
    if (qa_active) {
        in.buttons = qa_buttons_;
        in.buttons_pressed = qa_pressed;
    }

    const FaithStepResult r = move_->Step(in, state_, trace);
    time_ += in.dt;
    ++faith_ticks_;
    last_events_ |= r.events;

    for (int i = 0; i < 3; ++i) {
        mv->m_vecVelocity[i] = (&state_.velocity.x)[i];
        mv->m_vecAbsOrigin[i] = (&state_.origin.x)[i];
    }

    if (debug_ >= 2 || (debug_ == 1 && (faith_ticks_ % 33 == 0 || r.events))) {
        ApertureRunnerServerPlugin::LogMsg("[AR] t=%.2f anim=%s vel2d=%.1f vz=%.1f suelo=%d pos=(%.1f %.1f %.1f) eventos=0x%x\n",
                                           time_, SequenceName(state_.anim), state_.speed_2d, state_.velocity.z,
                                           state_.on_ground ? 1 : 0, state_.origin.x, state_.origin.y,
                                           state_.origin.z, r.events);
    }
}

void FaithDriver::SetQaInput(float forward, float side, uint32_t buttons, int ticks) {
    qa_forward_ = std::clamp(forward, -1.0f, 1.0f);
    qa_side_ = std::clamp(side, -1.0f, 1.0f);
    qa_buttons_ = buttons;
    qa_ticks_left_ = std::max(0, ticks);
    qa_first_tick_ = true;
}

void FaithDriver::PrintStatus() const {
    ApertureRunnerServerPlugin::LogMsg(
        "[AR] ar_enable=%d anim=%s vel2d=%.1f vz=%.1f suelo=%d agachada=%d pos=(%.1f %.1f %.1f) motor=(%.1f %.1f %.1f) "
        "ticks_faith=%llu ticks_portal=%llu portales=%u eventos_acumulados=0x%x\n",
        enabled_ ? 1 : 0, SequenceName(state_.anim), state_.speed_2d, state_.velocity.z, state_.on_ground ? 1 : 0,
        state_.ducked ? 1 : 0, state_.origin.x, state_.origin.y, state_.origin.z, engine_origin_.x, engine_origin_.y,
        engine_origin_.z, faith_ticks_, portal_ticks_,
        static_cast<unsigned>(portals_.size()), last_events_);
}

}  // namespace ar
