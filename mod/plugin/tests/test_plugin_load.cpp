#define _CRT_SECURE_NO_WARNINGS 1
#include "source_interfaces.h"
#include "server_trace_provider.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Mock de CPortalGameMovement para validar el hook de vtable en un entorno headless
class MockGameMovement : public ar::source::IGameMovement {
public:
    int process_movement_calls = 0;
    void* last_player = nullptr;
    void* last_move = nullptr;

    ~MockGameMovement() override = default;

    void ProcessMovement(void* pPlayer, void* pMove) override {
        ++process_movement_calls;
        last_player = pPlayer;
        last_move = pMove;
    }

    void StartTrackPredictionErrors(void* /*pPlayer*/) override {}
    void FinishTrackPredictionErrors(void* /*pPlayer*/) override {}
    void DiffPrint(const char* /*fmt*/, ...) override {}
};

static MockGameMovement g_mockGameMovement;

// Mock de IEngineTrace para validar consultas espaciales (Z4) en un entorno headless
class MockEngineTrace : public ar::source::IEngineTrace {
public:
    int trace_ray_calls = 0;

    int GetPointContents(const float* /*vecAbsPosition*/, void** /*ppEntity*/) override { return 0; }
    int GetPointContents_Collideable(void* /*pCollide*/, const float* /*vecAbsPosition*/) override { return 0; }
    void ClipRayToEntity(const ar::source::Ray_t& /*ray*/, unsigned int /*fMask*/, void* /*pEnt*/, ar::source::trace_t* /*pTrace*/) override {}
    void ClipRayToCollideable(const ar::source::Ray_t& /*ray*/, unsigned int /*fMask*/, void* /*pCollide*/, ar::source::trace_t* /*pTrace*/) override {}

    void TraceRay(const ar::source::Ray_t& ray, unsigned int /*fMask*/, ar::source::ITraceFilter* /*pTraceFilter*/, ar::source::trace_t* pTrace) override {
        ++trace_ray_calls;
        if (!pTrace) return;

        pTrace->fraction = 1.0f;
        pTrace->startsolid = false;
        pTrace->allsolid = false;
        pTrace->plane.normal[0] = 0.0f;
        pTrace->plane.normal[1] = 0.0f;
        pTrace->plane.normal[2] = 0.0f;
        pTrace->plane.dist = 0.0f;
        pTrace->m_pEnt = nullptr;

        float center_start_x = ray.m_Start.x;
        float center_start_z = ray.m_Start.z;

        float delta_x = ray.m_Delta.x;
        float delta_z = ray.m_Delta.z;

        float ext_x = ray.m_Extents.x;
        float ext_z = ray.m_Extents.z;

        float orig_start_x = ray.m_Start.x + ray.m_StartOffset.x;
        float orig_start_y = ray.m_Start.y + ray.m_StartOffset.y;
        float orig_start_z = ray.m_Start.z + ray.m_StartOffset.z;

        // 1. Suelo solido en Z <= 0
        float bottom_z = center_start_z - ext_z;
        if (bottom_z < 0.0f) {
            pTrace->startsolid = true;
            pTrace->allsolid = (bottom_z + delta_z < 0.0f);
            pTrace->fraction = 0.0f;
            pTrace->endpos[0] = orig_start_x;
            pTrace->endpos[1] = orig_start_y;
            pTrace->endpos[2] = orig_start_z;
            pTrace->plane.normal[2] = 1.0f;
            return;
        }

        float best_fraction = 1.0f;

        if (delta_z < -1e-6f) {
            float t = (0.0f - bottom_z) / delta_z;
            if (t >= 0.0f && t < best_fraction) {
                best_fraction = t;
                pTrace->plane.normal[0] = 0.0f;
                pTrace->plane.normal[1] = 0.0f;
                pTrace->plane.normal[2] = 1.0f;
                pTrace->m_pEnt = nullptr;
            }
        }

        // 2. Pared vertical solida en X >= 100
        float right_x = center_start_x + ext_x;
        if (delta_x > 1e-6f) {
            float t = (100.0f - right_x) / delta_x;
            if (t >= 0.0f && t < best_fraction) {
                best_fraction = t;
                pTrace->plane.normal[0] = -1.0f;
                pTrace->plane.normal[1] = 0.0f;
                pTrace->plane.normal[2] = 0.0f;
                pTrace->m_pEnt = nullptr;
            }
        }

        pTrace->fraction = best_fraction;
        pTrace->endpos[0] = orig_start_x + delta_x * best_fraction;
        pTrace->endpos[1] = orig_start_y + ray.m_Delta.y * best_fraction;
        pTrace->endpos[2] = orig_start_z + delta_z * best_fraction;
    }
};

static MockEngineTrace g_mockEngineTrace;

// The engine calls TraceRay through slot 4 of the SDK 2013 vtable; slot 5 crashes Portal.
static bool TraceRayIsSlot4() {
    void** vt = *reinterpret_cast<void***>(static_cast<ar::source::IEngineTrace*>(&g_mockEngineTrace));
    auto fn = reinterpret_cast<void(__thiscall*)(void*, const ar::source::Ray_t*, unsigned int, void*, ar::source::trace_t*)>(vt[4]);
    const int before = g_mockEngineTrace.trace_ray_calls;
    ar::source::Ray_t ray;
    ray.Init(ar::Vec3(0, 0, 50), ar::Vec3(0, 0, 40));
    ar::source::trace_t tr;
    fn(&g_mockEngineTrace, &ray, 0, nullptr, &tr);
    return g_mockEngineTrace.trace_ray_calls == before + 1;
}

// Minimal stand-ins for CBaseEntity / IServerNetworkable with the SDK 2013 vtable slots that
// ClassifyHitEntity reads (GetNetworkable = 4, GetClassName = 3).
class FakeNetworkable {
public:
    explicit FakeNetworkable(const char* name) : name_(name) {}
    virtual void* GetEntityHandle() { return nullptr; }
    virtual void* GetServerClass() { return nullptr; }
    virtual void* GetEdict() const { return nullptr; }
    virtual const char* GetClassName() const { return name_; }

private:
    const char* name_;
};

class FakeEntity {
public:
    explicit FakeEntity(const char* name) : net_(name) {}
    virtual ~FakeEntity() = default;
    virtual void SetRefEHandle(const void*) {}
    virtual const void* GetRefEHandle() const { return nullptr; }
    virtual void* GetCollideable() { return nullptr; }
    virtual FakeNetworkable* GetNetworkable() { return &net_; }

private:
    FakeNetworkable net_;
};

static void* MockEngineFactory(const char* pName, int* pReturnCode) {
    if (pName && std::strcmp(pName, ar::source::kEngineTraceServerVersion) == 0) {
        if (pReturnCode) *pReturnCode = 0;
        return &g_mockEngineTrace;
    }
    if (pReturnCode) *pReturnCode = 1;
    return nullptr;
}

static void* MockGameServerFactory(const char* pName, int* pReturnCode) {
    if (pName && std::strcmp(pName, ar::source::kGameMovementVersion) == 0) {
        if (pReturnCode) *pReturnCode = 0;
        return &g_mockGameMovement;
    }
    if (pReturnCode) *pReturnCode = 1;
    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Uso: test_plugin_load <ruta-a-aperture_runner.dll>\n");
        return 1;
    }

    const char* dll_path = argv[1];
    std::printf("[test_plugin_load] Cargando dinamicamente: %s\n", dll_path);

#ifdef _WIN32
    HMODULE hModule = LoadLibraryA(dll_path);
    if (!hModule) {
        std::fprintf(stderr, "[test_plugin_load] Error al cargar la DLL (codigo %lu)\n", GetLastError());
        return 1;
    }

    auto pfnCreateInterface = reinterpret_cast<ar::source::CreateInterfaceFn>(
        GetProcAddress(hModule, "CreateInterface")
    );
    if (!pfnCreateInterface) {
        std::fprintf(stderr, "[test_plugin_load] Error: CreateInterface no esta exportada en la DLL\n");
        FreeLibrary(hModule);
        return 1;
    }

    int returnCode = -1;
    void* iface = pfnCreateInterface(ar::source::kServerPluginCallbacksVersion, &returnCode);
    if (!iface || returnCode != 0) {
        std::fprintf(stderr, "[test_plugin_load] Error: CreateInterface fallo para %s (rc=%d)\n",
                     ar::source::kServerPluginCallbacksVersion, returnCode);
        FreeLibrary(hModule);
        return 1;
    }

    auto* plugin = static_cast<ar::source::IServerPluginCallbacks*>(iface);
    const char* desc = plugin->GetPluginDescription();
    if (!desc || std::strstr(desc, "ApertureRunner") == nullptr) {
        std::fprintf(stderr, "[test_plugin_load] Error: Descripcion invalida: %s\n", desc ? desc : "(null)");
        FreeLibrary(hModule);
        return 1;
    }

    // 1. Registrar estado original de la vtable del mock
    void** pVTable = *reinterpret_cast<void***>(&g_mockGameMovement);
    void* originalSlot1 = pVTable[1];

    // 2. Simular carga pasando MockEngineFactory y MockGameServerFactory
    bool ok = plugin->Load(MockEngineFactory, MockGameServerFactory);
    if (!ok) {
        std::fprintf(stderr, "[test_plugin_load] Error: plugin->Load() retorno false\n");
        FreeLibrary(hModule);
        return 1;
    }

    // 3. Verificar que la ranura 1 de la vtable fue hookeada
    void* hookedSlot1 = pVTable[1];
    if (hookedSlot1 == originalSlot1) {
        std::fprintf(stderr, "[test_plugin_load] Error: La vtable de IGameMovement no fue modificada por el hook\n");
        FreeLibrary(hModule);
        return 1;
    }

    // 4. Invocar ProcessMovement a traves del mock (debe pasar por Hooked_ProcessMovement y delegar al mock)
    int dummyPlayer = 0x1234;
    int dummyMove = 0x5678;
    // Llamar por un puntero volatile a la interfaz para evitar desvirtualizacion
    ar::source::IGameMovement* volatile gm = &g_mockGameMovement;
    gm->ProcessMovement(&dummyPlayer, &dummyMove);

    if (g_mockGameMovement.process_movement_calls != 1 ||
        g_mockGameMovement.last_player != &dummyPlayer ||
        g_mockGameMovement.last_move != &dummyMove) {
        std::fprintf(stderr, "[test_plugin_load] Error: La llamada passthrough fallo o no paso los argumentos esperados\n");
        FreeLibrary(hModule);
        return 1;
    }

    // 5. Validar ServerTraceProvider (Z4)
    std::printf("[test_plugin_load] Verificando consultas espaciales de ServerTraceProvider (Z4)...\n");
    ar::ServerTraceProvider provider(&g_mockEngineTrace);

    // Test 5.1: Rayo vertical hacia abajo golpeando el piso en Z = 0
    ar::TraceResult tr_ray = provider.TraceRay(ar::Vec3(0, 0, 100), ar::Vec3(0, 0, -100));
    if (std::fabs(tr_ray.fraction - 0.5f) > 1e-4f ||
        std::fabs(tr_ray.end_pos.z - 0.0f) > 1e-4f ||
        std::fabs(tr_ray.normal.z - 1.0f) > 1e-4f ||
        tr_ray.kind != ar::TraceHitKind::World ||
        tr_ray.start_solid || tr_ray.all_solid) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceRay piso fallo (frac=%f, z=%f, nz=%f)\n",
                     tr_ray.fraction, tr_ray.end_pos.z, tr_ray.normal.z);
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.2: Swept Hull hacia abajo con bounding box de jugador (mins { -16, -16, 0 }, maxs { 16, 16, 72 })
    ar::TraceResult tr_hull = provider.TraceHull(
        ar::Vec3(0, 0, 100), ar::Vec3(0, 0, -100),
        ar::Vec3(-16, -16, 0), ar::Vec3(16, 16, 72)
    );
    if (std::fabs(tr_hull.fraction - 0.5f) > 1e-4f ||
        std::fabs(tr_hull.end_pos.z - 0.0f) > 1e-4f ||
        std::fabs(tr_hull.normal.z - 1.0f) > 1e-4f ||
        tr_hull.kind != ar::TraceHitKind::World) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceHull piso fallo (frac=%f, z=%f, nz=%f)\n",
                     tr_hull.fraction, tr_hull.end_pos.z, tr_hull.normal.z);
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.3: Swept Hull horizontal hacia una pared en X = 100
    // Start X = 0 (right edge = 16), Target X = 200. Choca en X = 100 (t = 84 / 200 = 0.42)
    ar::TraceResult tr_wall = provider.TraceHull(
        ar::Vec3(0, 0, 10), ar::Vec3(200, 0, 10),
        ar::Vec3(-16, -16, 0), ar::Vec3(16, 16, 72)
    );
    if (std::fabs(tr_wall.fraction - 0.42f) > 1e-4f ||
        std::fabs(tr_wall.end_pos.x - 84.0f) > 1e-4f ||
        std::fabs(tr_wall.normal.x - (-1.0f)) > 1e-4f ||
        tr_wall.kind != ar::TraceHitKind::World) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceHull pared fallo (frac=%f, end_x=%f, nx=%f)\n",
                     tr_wall.fraction, tr_wall.end_pos.x, tr_wall.normal.x);
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.4: Rayo libre sin impacto
    ar::TraceResult tr_free = provider.TraceRay(ar::Vec3(0, 0, 50), ar::Vec3(50, 0, 50));
    if (tr_free.fraction != 1.0f || tr_free.Hit() || tr_free.kind != ar::TraceHitKind::None) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceRay libre detecto impacto falso\n");
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.5: Hull que inicia dentro de un solido (Z < 0)
    ar::TraceResult tr_solid = provider.TraceHull(
        ar::Vec3(0, 0, -10), ar::Vec3(0, 0, -50),
        ar::Vec3(-16, -16, 0), ar::Vec3(16, 16, 72)
    );
    if (!tr_solid.start_solid || tr_solid.fraction != 0.0f || tr_solid.kind != ar::TraceHitKind::World) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceHull start_solid fallo\n");
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.6: Ignorar la entidad del jugador
    int dummyPlayerEnt = 0x9999;
    provider.SetIgnorePlayer(&dummyPlayerEnt);
    if (provider.GetIgnorePlayer() != &dummyPlayerEnt) {
        std::fprintf(stderr, "[test_plugin_load] Error: SetIgnorePlayer no guardo el puntero\n");
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.7: TraceRay vive en la ranura 4 de la vtable (como en Portal 1 / SDK 2013)
    if (!TraceRayIsSlot4()) {
        std::fprintf(stderr, "[test_plugin_load] Error: TraceRay no esta en la ranura 4 de IEngineTrace\n");
        FreeLibrary(hModule);
        return 1;
    }

    // Test 5.8: clasificacion de lo golpeado por su nombre de clase
    struct { const char* name; ar::TraceHitKind kind; } cases[] = {
        {"worldspawn", ar::TraceHitKind::World},
        {"prop_physics", ar::TraceHitKind::Physics},           // el cubo de testchmb_a_00
        {"prop_physics_override", ar::TraceHitKind::Physics},
        {"npc_portal_turret_floor", ar::TraceHitKind::Physics},
        {"player", ar::TraceHitKind::Player},
        {"func_door", ar::TraceHitKind::StaticProp},
        {"prop_dynamic", ar::TraceHitKind::StaticProp},
    };
    for (const auto& c : cases) {
        FakeEntity ent(c.name);
        if (ar::ClassifyHitEntity(&ent) != c.kind) {
            std::fprintf(stderr, "[test_plugin_load] Error: %s clasificado como %d\n", c.name,
                         static_cast<int>(ar::ClassifyHitEntity(&ent)));
            FreeLibrary(hModule);
            return 1;
        }
    }
    if (ar::ClassifyHitEntity(nullptr) != ar::TraceHitKind::World) {
        std::fprintf(stderr, "[test_plugin_load] Error: m_pEnt nulo debe ser World\n");
        FreeLibrary(hModule);
        return 1;
    }

    std::printf("[test_plugin_load] 8/8 comprobaciones de ServerTraceProvider aprobadas.\n");

    // 6. Ciclo de eventos en mapa
    plugin->LevelInit("ar_harness");  // otro nombre: el autotest Z4 solo vale en el mapa real
    plugin->ServerActivate(nullptr, 0, 1);
    plugin->GameFrame(true);

    // Invocar varios ticks mas
    for (int i = 0; i < 10; ++i) {
        gm->ProcessMovement(&dummyPlayer, &dummyMove);
    }
    if (g_mockGameMovement.process_movement_calls != 11) {
        std::fprintf(stderr, "[test_plugin_load] Error: Se esperaban 11 llamadas de movimiento, se obtuvieron %d\n",
                     g_mockGameMovement.process_movement_calls);
        FreeLibrary(hModule);
        return 1;
    }

    plugin->LevelShutdown();

    // 7. Descarga y verificacion de desenganche limpio
    plugin->Unload();
    void* restoredSlot1 = pVTable[1];
    if (restoredSlot1 != originalSlot1) {
        std::fprintf(stderr, "[test_plugin_load] Error: La vtable no fue restaurada tras Unload()\n");
        FreeLibrary(hModule);
        return 1;
    }

    FreeLibrary(hModule);
    std::printf("[test_plugin_load] Exito rotundo: Z4 ServerTraceProvider y Z3 Hook validados al 100%%.\n");
    return 0;
#else
    std::printf("Test omitido en sistemas no-Windows.\n");
    return 0;
#endif
}
