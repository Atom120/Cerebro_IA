#include "source_interfaces.h"
#include <cassert>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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

    // Validar ciclo de vida completo
    bool ok = plugin->Load(nullptr, nullptr);
    if (!ok) {
        std::fprintf(stderr, "[test_plugin_load] Error: plugin->Load() retorno false\n");
        FreeLibrary(hModule);
        return 1;
    }

    plugin->LevelInit("testchmb_a_00");
    plugin->ServerActivate(nullptr, 0, 1);
    plugin->GameFrame(true);
    plugin->LevelShutdown();
    plugin->Unload();

    FreeLibrary(hModule);
    std::printf("[test_plugin_load] Exito: aperture_runner.dll exporta CreateInterface y cumple la ABI ISERVERPLUGINCALLBACKS003.\n");
    return 0;
#else
    std::printf("Test omitido en sistemas no-Windows.\n");
    return 0;
#endif
}
