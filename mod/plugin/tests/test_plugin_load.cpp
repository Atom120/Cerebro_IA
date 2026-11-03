#define _CRT_SECURE_NO_WARNINGS 1
#include "source_interfaces.h"
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

    // 2. Simular carga pasando MockGameServerFactory
    bool ok = plugin->Load(nullptr, MockGameServerFactory);
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
    // Llamar por un puntero volatile a la interfaz: si se llama sobre el objeto concreto, el
    // compilador puede desvirtualizar la llamada y saltarse la vtable (y el hook).
    ar::source::IGameMovement* volatile gm = &g_mockGameMovement;
    gm->ProcessMovement(&dummyPlayer, &dummyMove);

    if (g_mockGameMovement.process_movement_calls != 1 ||
        g_mockGameMovement.last_player != &dummyPlayer ||
        g_mockGameMovement.last_move != &dummyMove) {
        std::fprintf(stderr, "[test_plugin_load] Error: La llamada passthrough fallo o no paso los argumentos esperados\n");
        FreeLibrary(hModule);
        return 1;
    }

    // 5. Ciclo de eventos en mapa
    plugin->LevelInit("testchmb_a_00");
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

    // 6. Descarga y verificacion de desenganche limpio
    plugin->Unload();
    void* restoredSlot1 = pVTable[1];
    if (restoredSlot1 != originalSlot1) {
        std::fprintf(stderr, "[test_plugin_load] Error: La vtable no fue restaurada tras Unload()\n");
        FreeLibrary(hModule);
        return 1;
    }

    FreeLibrary(hModule);
    std::printf("[test_plugin_load] Exito rotundo: Hook de ProcessMovement en passthrough validado (Hito H1 listo).\n");
    return 0;
#else
    std::printf("Test omitido en sistemas no-Windows.\n");
    return 0;
#endif
}
