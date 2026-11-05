// Implementation of first-person Faith arms/body model selector and sequence mapper (Task G4).
// Owner: Gonzalo Robles (mod/plugin/client/render/).
#define _CRT_SECURE_NO_WARNINGS 1
#include "faith_render.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace ar::client::render {

namespace {

bool FileExistsOnDisk(const std::string& full_path) {
    if (full_path.empty()) {
        return false;
    }
    std::ifstream f(full_path, std::ios::binary);
    return f.good();
}

void DefaultSourceConsoleWarning(const char* message) {
    if (message == nullptr) {
        return;
    }
#ifdef _WIN32
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    if (tier0 != nullptr) {
        using MsgFn = void(__cdecl*)(const char* fmt, ...);
        auto fn = reinterpret_cast<MsgFn>(GetProcAddress(tier0, "Msg"));
        if (fn != nullptr) {
            fn("%s", message);
            return;
        }
    }
#endif
}

float ComputePlaybackRate(const FaithState& state) {
    const float sprint_start = UuToSu(me_reference::kSprintStartSpeedUu);
    const float sprint_max = UuToSu(me_reference::kSprintMaxSpeedUu);
    switch (state.anim) {
        case FaithAnimState::Walk:
        case FaithAnimState::CrouchWalk:
            return std::clamp(state.speed_2d / 100.0f, 0.6f, 1.3f);
        case FaithAnimState::Run:
            return std::clamp(state.speed_2d / std::max(sprint_start, 1.0f), 0.75f, 1.35f);
        case FaithAnimState::Sprint: {
            const float ratio = state.speed_2d / std::max(sprint_max, 1.0f);
            return std::clamp(ratio, 0.85f, 1.45f);
        }
        default:
            return 1.0f;
    }
}

}  // namespace

RenderModelSelection FaithFirstPersonRenderer::ResolveModels(
    const std::string& portal_root_dir, ConsoleWarningFn warning_sink) {
    const std::string custom_base =
        portal_root_dir.empty()
            ? std::string()
            : (portal_root_dir + "/portal/custom/aperture_runner/");

    const std::string arms_disk = custom_base + kFaithArmsModelPath;
    const std::string body_disk = custom_base + kFaithBodyModelPath;

    const bool has_arms = !custom_base.empty() && FileExistsOnDisk(arms_disk);
    const bool has_body = !custom_base.empty() && FileExistsOnDisk(body_disk);

    if (has_arms && has_body) {
        selection_.arms_model_path = kFaithArmsModelPath;
        selection_.body_model_path = kFaithBodyModelPath;
        selection_.using_provisional_models = false;
        return selection_;
    }

    selection_.arms_model_path = kProvisionalArmsModelPath;
    selection_.body_model_path = kProvisionalBodyModelPath;
    selection_.using_provisional_models = true;

    if (!warned_missing_models_) {
        warned_missing_models_ = true;
        const char* msg =
            "[ApertureRunner] Aviso: no se encontraron models/aperture_runner/faith_arms.mdl "
            "ni faith_body.mdl en portal/custom/aperture_runner/; usando modelos provisionales "
            "de Portal (G4).\n";
        if (warning_sink != nullptr) {
            warning_sink(msg);
        } else {
            DefaultSourceConsoleWarning(msg);
        }
    }

    return selection_;
}

FaithRenderFrame FaithFirstPersonRenderer::BuildFrame(const FaithState& state,
                                                      float view_yaw_deg) const {
    FaithRenderFrame frame{};
    const char* seq = SequenceName(state.anim);
    frame.sequence_name = (seq != nullptr) ? seq : "idle";
    frame.anim_state = (seq != nullptr) ? state.anim : FaithAnimState::Idle;
    frame.playback_rate = ComputePlaybackRate(state);
    frame.cycle_time_sec = state.state_time;
    frame.draw_arms_1p = true;
    frame.draw_body_1p = true;
    frame.body_origin = state.origin;
    frame.body_yaw_deg = view_yaw_deg;
    return frame;
}

}  // namespace ar::client::render
