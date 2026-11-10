#define _CRT_SECURE_NO_WARNINGS 1
#include "server_plugin.h"
#include "movement_hook.h"
#include "faith_driver.h"
#include "faith_viewmodel.h"
#include "mod_settings.h"
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

    // Ajustes recordados entre partidas (addons/aperture_runner.cfg).
    const ModSettings settings = LoadModSettings();
    FaithDriver::Instance().SetEnabled(settings.enabled);
    FaithDriver::Instance().SetCamera(settings.camera);
    FaithDriver::Instance().SetFaithLook(settings.faith_look);

    LogMsg(
        "\n======================================================\n"
        " [ApertureRunner] Plugin VSP cargado con exito.\n"
        " Target Engine : Portal 1 (Build %u, Win32/x86)\n"
        " Estado Inicial: ar_enable = %d, ar_camera = %d (addons/aperture_runner.cfg)\n"
        "======================================================\n\n",
        kPortalBuildId, settings.enabled ? 1 : 0, settings.camera ? 1 : 0);

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

    // G4: brazos de Faith (viewmodel). VSERVERTOOLS002 en server.dll, VEngineServer021 en engine.dll.
    {
        int rc_tools = 0, rc_engine = 0;
        void* tools = gameServerFactory ? gameServerFactory("VSERVERTOOLS002", &rc_tools) : nullptr;
        void* engine = interfaceFactory ? interfaceFactory("VEngineServer021", &rc_engine) : nullptr;
        if (tools && rc_tools == 0 && engine && rc_engine == 0) {
            FaithViewModel::Instance().Init(tools, engine);
            LogMsg("[ApertureRunner] VSERVERTOOLS002 y VEngineServer021 adquiridos (G4, brazos de Faith).\n");
        } else {
            LogMsg("[ApertureRunner] AVISO: sin VSERVERTOOLS002/VEngineServer021: no habra brazos de Faith.\n");
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
    // Tecla H = ar_personaje (Faith <-> Chell). Portal no usa la H; el bind queda en config.cfg.
    FaithViewModel::Instance().ServerCommand("bind h ar_personaje\n");
}

void ApertureRunnerServerPlugin::GameFrame(bool /*simulating*/) {
}

void ApertureRunnerServerPlugin::LevelShutdown() {
    LogMsg("[ApertureRunner] LevelShutdown en mapa %s\n", current_map_);
    FaithDriver::Instance().OnLevelShutdown();
    FaithViewModel::Instance().OnLevelShutdown();
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
        const bool saved = SaveModSettings({faith.Enabled(), faith.CameraEnabled(), faith.FaithLook()});
        LogMsg("[ApertureRunner] ar_enable = %d (%s)%s\n", faith.Enabled() ? 1 : 0,
               faith.Enabled() ? "movimiento de Faith" : "movimiento original de Portal",
               saved ? ", guardado en addons/aperture_runner.cfg" : "");
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
    // ar_personaje [faith|chell|1|0]: cambia el personaje que se ve (sin argumento alterna). Tecla H.
    if (std::strcmp(cmd, "ar_personaje") == 0) {
        bool faith_look = !faith.FaithLook();
        if (arg) faith_look = std::strcmp(arg, "chell") != 0 && std::strcmp(arg, "0") != 0;
        faith.SetFaithLook(faith_look);
        SaveModSettings({faith.Enabled(), faith.CameraEnabled(), faith.FaithLook()});
        LogMsg("[ApertureRunner] Personaje: %s (el parkour sigue activo%s)\n", faith_look ? "Faith" : "Chell",
               faith.Enabled() ? "" : "; activalo con ar_enable 1");
        FaithViewModel::Instance().Announce(faith_look ? "Faith (Mirror's Edge)" : "Chell (Portal)");
        return source::kPluginStop;
    }
    if (std::strcmp(cmd, "ar_brazos_seq") == 0) {  // diagnostico G4: fuerza una secuencia (-1 = la de Faith)
        const int seq = arg ? std::atoi(arg) : -1;
        FaithViewModel::Instance().SetOverride(seq);
        LogMsg("[ApertureRunner] ar_brazos_seq = %d\n", seq);
        return source::kPluginStop;
    }
    if (std::strcmp(cmd, "ar_camera") == 0) {  // ar_camera [0|1]: camara de Faith en la vista (G4)
        faith.SetCamera(!arg || std::strcmp(arg, "0") != 0);
        SaveModSettings({faith.Enabled(), faith.CameraEnabled(), faith.FaithLook()});
        LogMsg("[ApertureRunner] ar_camera = %d\n", faith.CameraEnabled() ? 1 : 0);
        return source::kPluginStop;
    }
    // ar_find_ledges <x0> <y0> <x1> <y1> <z_suelo> [paso]: busca sitios de vault y bordes (QA del mapa)
    if (std::strcmp(cmd, "ar_find_ledges") == 0 && CommandArgc(args) >= 6) {
        float v[6] = {0, 0, 0, 0, 0, 16.0f};
        for (int i = 0; i < 6 && i + 1 < CommandArgc(args); ++i) v[i] = static_cast<float>(std::atof(CommandArg(args, i + 1)));
        faith.FindLedges(trace_provider_, v[0], v[1], v[2], v[3], v[4], v[5] > 1.0f ? v[5] : 16.0f);
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
    // A prop_portal may be gone: drop the list (no dangling pointers) and rebuild it next tick.
    FaithDriver::Instance().OnEdictFreed();
}

}  // namespace ar
