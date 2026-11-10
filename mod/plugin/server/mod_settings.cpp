#define _CRT_SECURE_NO_WARNINGS 1
#include "mod_settings.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

std::string ModSettingsPath() {
#ifdef _WIN32
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&ModSettingsPath), &self)) {
        return "";
    }
    char path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(self, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return "";
    std::string dir(path, n);
    const size_t slash = dir.find_last_of("\\/");
    dir = slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
    return dir + "aperture_runner.cfg";
#else
    return "";
#endif
}

ModSettings LoadModSettings() {
    ModSettings s;
    const std::string path = ModSettingsPath();
    if (path.empty()) return s;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return s;
    char line[128];
    while (std::fgets(line, sizeof(line), f)) {
        int value = 0;
        if (std::sscanf(line, "ar_enable=%d", &value) == 1) s.enabled = value != 0;
        if (std::sscanf(line, "ar_camera=%d", &value) == 1) s.camera = value != 0;
        if (std::sscanf(line, "ar_personaje=%d", &value) == 1) s.faith_look = value != 0;
    }
    std::fclose(f);
    return s;
}

bool SaveModSettings(const ModSettings& s) {
    const std::string path = ModSettingsPath();
    if (path.empty()) return false;
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return false;
    std::fprintf(f, "// ApertureRunner: se guarda solo con los comandos ar_enable / ar_camera / ar_personaje.\n");
    std::fprintf(f, "// ar_personaje: 1 = Faith, 0 = Chell (el parkour funciona con las dos).\n");
    std::fprintf(f, "ar_enable=%d\nar_camera=%d\nar_personaje=%d\n", s.enabled ? 1 : 0, s.camera ? 1 : 0,
                 s.faith_look ? 1 : 0);
    std::fclose(f);
    return true;
}

}  // namespace ar
