#pragma once

// Mod settings remembered between sessions: addons/aperture_runner.cfg next to the DLL.
//   ar_enable=1     Faith movement on at start
//   ar_camera=1     Faith camera (180 turn, wallrun roll, FOV, head bob) on the view
// Written by the console commands ar_enable / ar_camera; read when the plugin loads.

#include <string>

namespace ar {

struct ModSettings {
    bool enabled = false;
    bool camera = true;
};

std::string ModSettingsPath();  // full path of addons/aperture_runner.cfg ("" if unknown)
ModSettings LoadModSettings();  // defaults if the file is missing or unreadable
bool SaveModSettings(const ModSettings& settings);

}  // namespace ar
