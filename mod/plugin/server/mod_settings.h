#pragma once

// Mod settings remembered between sessions: addons/aperture_runner.cfg next to the DLL.
//   ar_enable=1     Faith movement on at start
//   ar_camera=1     Faith camera (180 turn, wallrun roll, FOV, head bob) on the view
//   ar_personaje=1  1 = Faith (her arms and legs), 0 = Chell (Portal's own look); parkour works with both
// Written by the console commands ar_enable / ar_camera / ar_personaje; read when the plugin loads.

#include <string>

namespace ar {

struct ModSettings {
    bool enabled = false;
    bool camera = true;
    bool faith_look = true;
};

std::string ModSettingsPath();  // full path of addons/aperture_runner.cfg ("" if unknown)
ModSettings LoadModSettings();  // defaults if the file is missing or unreadable
bool SaveModSettings(const ModSettings& settings);

}  // namespace ar
