// First-person Faith arms and body viewmodel/player model selector and sequence mapper (Task G4).
// Owner: Gonzalo Robles (mod/plugin/client/render/).
#pragma once

#include <cstdint>
#include <string>

#include "ar/faith_movement.h"
#include "ar/units.h"

namespace ar::client::render {

// Paths defined in docs/contratos/asset_manifest.json (v1)
inline constexpr const char* kFaithArmsModelPath = "models/aperture_runner/faith_arms.mdl";
inline constexpr const char* kFaithBodyModelPath = "models/aperture_runner/faith_body.mdl";
inline constexpr const char* kCameraAttachmentName = "camera";

// Provisional Portal 1 models used when Antonio's extracted models are not yet present on the PC
inline constexpr const char* kProvisionalArmsModelPath = "models/weapons/v_portalgun.mdl";
inline constexpr const char* kProvisionalBodyModelPath = "models/player/chell.mdl";

using ConsoleWarningFn = void (*)(const char* message);

struct RenderModelSelection {
    const char* arms_model_path = kProvisionalArmsModelPath;
    const char* body_model_path = kProvisionalBodyModelPath;
    bool using_provisional_models = true;
};

struct FaithRenderFrame {
    const char* sequence_name = "idle";
    FaithAnimState anim_state = FaithAnimState::Idle;
    float playback_rate = 1.0f;
    float cycle_time_sec = 0.0f;
    bool draw_arms_1p = true;
    bool draw_body_1p = true;
    Vec3 body_origin;
    float body_yaw_deg = 0.0f;
};

class FaithFirstPersonRenderer {
public:
    FaithFirstPersonRenderer() = default;

    // Resolves whether the locally compiled Faith models exist under
    // <portal_root>/portal/custom/aperture_runner/. If missing, selects the provisional
    // Portal models and emits a console warning once.
    RenderModelSelection ResolveModels(const std::string& portal_root_dir,
                                       ConsoleWarningFn warning_sink = nullptr);

    const RenderModelSelection& ActiveModels() const { return selection_; }
    bool WarnedMissingModels() const { return warned_missing_models_; }

    // Computes the animation sequence name, playback speed and body transform for the current tick.
    FaithRenderFrame BuildFrame(const FaithState& state, float view_yaw_deg) const;

private:
    RenderModelSelection selection_{};
    bool warned_missing_models_ = false;
};

}  // namespace ar::client::render
