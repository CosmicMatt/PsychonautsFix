#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include "dinput8_proxy.h"
#include "xinput_translator.h"
#include "display_patcher.h"
#include "config.h"

#pragma comment(lib, "shlwapi.lib")

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        // Determine INI path in same directory as DLL
        wchar_t iniPath[MAX_PATH];
        GetModuleFileNameW(hModule, iniPath, MAX_PATH);
        PathRemoveFileSpecW(iniPath);
        PathAppendW(iniPath, L"PsychoControllerFix.ini");

        // Load configuration
        ModConfig mcfg = Config::Load(iniPath);
        XInputTranslator::SetConfig(mcfg.controller);

        // Initialize XInput system
        XInputTranslator::Initialize();

        // Initialize Display & Borderless Window system
        DisplayPatcher::Initialize(mcfg.display);
        break;
    }
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
