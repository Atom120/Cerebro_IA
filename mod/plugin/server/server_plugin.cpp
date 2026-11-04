#define _CRT_SECURE_NO_WARNINGS 1
#include "server_plugin.h"
#include "movement_hook.h"
#include "faith_driver.h"
#include "client/client_movement_hook.h"
#include "client/client_view.h"
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ar {

typedef void (*MsgFn)(const char* fmt, ...);
static MsgFn s_pfnMsg = nullptr;

ApertureRunnerServerPlugin& ApertureRunnerServerPlugin::Instance() {
    static ApertureRunnerServerPlugin instance;
    return instance;
}

const char* ApertureRunnerServerPlugin::GetPluginDescription() {
    return "ApertureRunner: Mirror's Edge movement in Portal 1 (testchmb_a_00)";
}

void ApertureRunnerServerPlugin::LogMsg(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

#ifdef _WIN32
    if (s_pfnMsg) {
        s_pfnMsg("%s", buffer);
    }
    OutputDebugStringA(buffer);
#endif
    std::printf("%s", buffer);
}

bool ApertureRunnerServerPlugin::Load(source::CreateInterfaceFn interfaceFactory, source::CreateInterfaceFn gameServerFactory) {
    if (is_loaded_) return true;

#ifdef _WIN32
    if (!s_pfnMsg) {
        HMODULE hTier0 = GetModuleHandleA("tier0.dll");
        if (hTier0) {
            s_pfnMsg = reinterpret_cast<MsgFn>(GetProcAddress(hTier0, "Msg"));
        }
    }
#endif

    LogMsg(
        "\n======================================================\n"
        " [ApertureRunner] Plugin VSP cargado con exito.\n"
        " Target Engine : Portal 1 (Build %u, Win32/x86)\n"
        " Estado Inicial: ar_enable = 0 (passthrough)\n"
        "======================================================\n\n",
        kPortalBuildId);

    // Z4: Obtener EngineTraceServer003 para el trace provider del servidor
    if (interfaceFactory) {
        int returnCode = 0;
        void* iface = interfaceFactory(source::kEngineTraceServerVersion, &returnCode);
        if (iface && returnCode == 0) {
            engine_trace_ = static_cast<source::IEngineTrace*>(iface);
            trace_provider_.SetEngineTrace(engine_trace_);
            LogMsg("[ApertureRunner] EngineTraceServer003 adquirido exitosamente (Z4).\n");
        } else {
            LogMsg("[ApertureRunner] AVISO: EngineTraceServer003 no disponible en interfaceFactory (rc=%d).\n", returnCode);
        }
    }

    // Z2/Z3: Obtener GameMovement001 y enganchar ProcessMovement en passthrough
    if (gameServerFactory) {
        int returnCode = 0;
        void* iface = gameServerFactory(source::kGameMovementVersion, &returnCode);
        if (iface && returnCode == 0) {
            auto* pGM = static_cast<source::IGameMovement*>(iface);
            if (MovementHook::Install(pGM)) {
                LogMsg("[ApertureRunner] IGameMovement::ProcessMovement hookeado exitosamente en passthrough (Hito H1).\n");
            } else {
                LogMsg("[ApertureRunner] AVISO: No se pudo instalar el hook en GameMovement001.\n");
            }
        } else {
            LogMsg("[ApertureRunner] AVISO: GameMovement001 no disponible en gameServerFactory.\n");
        }
    }

    // G1: copia de prediccion del cliente (client.dll), en el mismo proceso.
    LogMsg(ClientMovementHook::Install() ? "[ApertureRunner] Prediccion del cliente (client.dll GameMovement001) enganchada (G1).\n"
                                         : "[ApertureRunner] client.dll no disponible: sin hook de prediccion.\n");
    // G4: angulos de la vista del jugador local (camara de Faith).
    if (ClientView::Init()) {
        LogMsg("[ApertureRunner] Vista del cliente disponible (%s) (G4).\n", ClientView::Version());
    }

    is_loaded_ = true;
    return true;
}

void ApertureRunnerServerPlugin::Unload() {
    if (!is_loaded_) return;

    if (MovementHook::IsHooked()) {
        MovementHook::Uninstall();
        LogMsg("[ApertureRunner] VTable de IGameMovement restaurada al original.\n");
    }
    ClientMovementHook::Uninstall();

    engine_trace_ = nullptr;
    trace_provider_.SetEngineTrace(nullptr);
    trace_provider_.SetIgnorePlayer(nullptr);

    LogMsg("[ApertureRunner] Plugin descargado limpiamente.\n");
    is_loaded_ = false;
}

void ApertureRunnerServerPlugin::LevelInit(const char* pMapName) {
    if (pMapName) {
        std::strncpy(current_map_, pMapName, sizeof(current_map_) - 1);
        current_map_[sizeof(current_map_) - 1] = '\0';
        LogMsg("[ApertureRunner] LevelInit: %s\n", current_map_);
    }
}

// Z4 self-test in testchmb_a_00: a ray straight down onto the floor at info_player_start must hit
// the world at z = 160 with an up normal. Catches IEngineTrace/Ray_t ABI mismatches in the real game.
static TraceResult SelfTestTrace(const ServerTraceProvider& provider, bool* crashed) {
    TraceResult r;
#ifdef _WIN32
    __try {
        r = provider.TraceRay({-607.8f, -346.7f, 200.0f}, {-607.8f, -346.7f, 0.0f});
    } __except (1) {
        *crashed = true;
    }
#else
    r = provider.TraceRay({-607.8f, -346.7f, 200.0f}, {-607.8f, -346.7f, 0.0f});
#endif
    return r;
}

void ApertureRunnerServerPlugin::RunTraceSelfTest() {
    if (!engine_trace_ || std::strcmp(current_map_, "testchmb_a_00") != 0) return;
    bool crashed = false;
    const TraceResult r = SelfTestTrace(trace_provider_, &crashed);
    const bool ok = !crashed && r.Hit() && !r.start_solid && r.end_pos.z > 155.0f && r.end_pos.z < 165.0f &&
                    r.normal.z > 0.99f && r.kind == TraceHitKind::World;
    LogMsg("[ApertureRunner] Autotest Z4 (traza al suelo en info_player_start): %s  frac=%.4f z=%.1f nz=%.2f kind=%d%s\n",
           ok ? "OK" : "FALLO", r.fraction, r.end_pos.z, r.normal.z, static_cast<int>(r.kind),
           crashed ? " (excepcion en TraceRay)" : "");
}

void ApertureRunnerServerPlugin::ServerActivate(void* pEdictList, int edictCount, int /*clientMax*/) {
    FaithDriver::Instance().ScanPortals(pEdictList, edictCount);
    LogMsg("[ApertureRunner] ServerActivate listo en mapa %s (%u prop_portal)\n", current_map_,
           static_cast<unsigned>(FaithDriver::Instance().PortalCount()));
    RunTraceSelfTest();
}

void ApertureRunnerServerPlugin::GameFrame(bool /*simulating*/) {
}

void ApertureRunnerServerPlugin::LevelShutdown() {
    LogMsg("[ApertureRunner] LevelShutdown en mapa %s\n", current_map_);
    FaithDriver::Instance().OnLevelShutdown();
    current_map_[0] = '\0';
}

void ApertureRunnerServerPlugin::ClientActive(void* /*pEntity*/) {
}

void ApertureRunnerServerPlugin::ClientDisconnect(void* /*pEntity*/) {
}

void ApertureRunnerServerPlugin::ClientPutInServer(void* /*pEntity*/, const char* /*playername*/) {
}

void ApertureRunnerServerPlugin::SetCommandClient(int /*index*/) {
}

void ApertureRunnerServerPlugin::ClientSettingsChanged(void* /*pEdict*/) {
}

source::PluginResult ApertureRunnerServerPlugin::ClientConnect(bool* /*bAllowConnect*/, void* /*pEntity*/, const char* /*pszName*/, const char* /*pszAddress*/, char* /*reject*/, int /*maxrejectlen*/) {
    return source::kPluginContinue;
}

// Console commands (typed in Portal's console; the engine forwards unknown commands to the server):
//   ar_enable [0|1]  Faith movement off/on (no argument toggles)
//   ar_status        prints Faith's state
//   ar_debug [0|1]   logs Faith's state twice a second and on every event
static int CommandArgc(const void* args) { return *static_cast<const int*>(args); }
static const char* CommandArg(const void* args, int i) {
    return reinterpret_cast<const char* const*>(static_cast<const char*>(args) + source::kCommandArgvOffset)[i];
}

source::PluginResult ApertureRunnerServerPlugin::ClientCommand(void* /*pEntity*/, const void* args) {
    if (!args || CommandArgc(args) < 1) return source::kPluginContinue;
    const char* cmd = CommandArg(args, 0);
    const char* arg = CommandArgc(args) > 1 ? CommandArg(args, 1) : nullptr;
    FaithDriver& faith = FaithDriver::Instance();
    if (std::strcmp(cmd, "ar_enable") == 0) {
        faith.SetEnabled(arg ? std::strcmp(arg, "0") != 0 : !faith.Enabled());
        LogMsg("[ApertureRunner] ar_enable = %d (%s)\n", faith.Enabled() ? 1 : 0,
               faith.Enabled() ? "movimiento de Faith" : "movimiento original de Portal");
        return source::kPluginStop;
    }
    if (std::strcmp(cmd, "ar_status") == 0) {
        faith.PrintStatus();
        float view[3] = {};
        if (ClientView::GetViewAngles(view)) {
            LogMsg("[AR] vista=(%.1f %.1f %.1f) via %s\n", view[0], view[1], view[2], ClientView::Version());
        }
        return source::kPluginStop;
    }
    if (std::strcmp(cmd, "ar_camera") == 0) {  // ar_camera [0|1]: camara de Faith en la vista (G4)
        faith.SetCamera(!arg || std::strcmp(arg, "0") != 0);
        return source::kPluginStop;
    }
    if (std::strcmp(cmd, "ar_debug") == 0) {
        faith.SetDebug(arg ? std::atoi(arg) : 1);
        return source::kPluginStop;
    }
    // ar_qa_input <forward -1..1> <side -1..1> <buttons: j=Up d=Down t=Turn180 w=Walk, - = none> <ticks> [press_tick]
    // The buttons are pressed on press_tick (default 0) and held until the end: one command can run
    // and then jump/slide at an exact tick, independent of console latency.
    if (std::strcmp(cmd, "ar_qa_input") == 0 && CommandArgc(args) >= 5) {
        uint32_t buttons = 0;
        for (const char* c = CommandArg(args, 3); *c; ++c) {
            if (*c == 'j') buttons |= kFaithButtonUp;
            if (*c == 'd') buttons |= kFaithButtonDown;
            if (*c == 't') buttons |= kFaithButtonTurn180;
            if (*c == 'w') buttons |= kFaithButtonWalk;
        }
        faith.SetQaInput(static_cast<float>(std::atof(CommandArg(args, 1))), static_cast<float>(std::atof(CommandArg(args, 2))),
                         buttons, std::atoi(CommandArg(args, 4)),
                         CommandArgc(args) >= 6 ? std::atoi(CommandArg(args, 5)) : 0);
        return source::kPluginStop;
    }
    return source::kPluginContinue;
}

source::PluginResult ApertureRunnerServerPlugin::NetworkIDValidated(const char* /*pszUserName*/, const char* /*pszNetworkID*/) {
    return source::kPluginContinue;
}

void ApertureRunnerServerPlugin::OnQueryCvarValueFinished(int /*iCookie*/, void* /*pPlayerEntity*/, source::QueryCvarValueStatus /*eStatus*/, const char* /*pCvarName*/, const char* /*pCvarValue*/) {
}

void ApertureRunnerServerPlugin::OnEdictAllocated(void* /*edict*/) {
}

void ApertureRunnerServerPlugin::OnEdictFreed(const void* /*edict*/) {
}

}  // namespace ar
