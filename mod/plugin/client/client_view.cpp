#define _CRT_SECURE_NO_WARNINGS 1
#include "client_view.h"

#include "source_interfaces.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

namespace {

// cdll_int.h IVEngineClient (Source 2013): 19 GetViewAngles(QAngle&), 20 SetViewAngles(QAngle&).
constexpr int kGetViewAnglesSlot = 19;
constexpr int kSetViewAnglesSlot = 20;

void* s_engine_client = nullptr;
const char* s_version = "";

typedef void(__thiscall* AnglesFn)(void* thisptr, float* angles);

bool CallAngles(int slot, float* angles) {
#ifdef _WIN32
    if (!s_engine_client) return false;
    __try {
        reinterpret_cast<AnglesFn>((*reinterpret_cast<void***>(s_engine_client))[slot])(s_engine_client, angles);
        return true;
    } __except (1) {
        return false;
    }
#else
    (void)slot;
    (void)angles;
    return false;
#endif
}

}  // namespace

bool ClientView::Init() {
#ifdef _WIN32
    if (s_engine_client) return true;
    HMODULE engine = GetModuleHandleA("engine.dll");
    if (!engine) return false;
    auto factory = reinterpret_cast<source::CreateInterfaceFn>(GetProcAddress(engine, "CreateInterface"));
    if (!factory) return false;
    static const char* const kVersions[] = {"VEngineClient015", "VEngineClient014", "VEngineClient013"};
    for (const char* version : kVersions) {
        int rc = 0;
        void* iface = factory(version, &rc);
        if (iface && rc == 0) {
            s_engine_client = iface;
            s_version = version;
            return true;
        }
    }
#endif
    return false;
}

bool ClientView::Ready() { return s_engine_client != nullptr; }
const char* ClientView::Version() { return s_version; }
bool ClientView::GetViewAngles(float angles[3]) { return CallAngles(kGetViewAnglesSlot, angles); }
bool ClientView::SetViewAngles(const float angles[3]) {
    float copy[3] = {angles[0], angles[1], angles[2]};
    return CallAngles(kSetViewAnglesSlot, copy);
}

}  // namespace ar
