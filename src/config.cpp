#include "config.h"
#include <cwchar>

static float GetPrivateProfileFloatW(const wchar_t* section, const wchar_t* key, float defaultValue, const wchar_t* filePath) {
    wchar_t buf[64];
    wchar_t defaultStr[64];
    swprintf_s(defaultStr, L"%f", defaultValue);
    GetPrivateProfileStringW(section, key, defaultStr, buf, 64, filePath);
    return (float)_wtof(buf);
}

static bool GetPrivateProfileBoolW(const wchar_t* section, const wchar_t* key, bool defaultValue, const wchar_t* filePath) {
    wchar_t buf[32];
    GetPrivateProfileStringW(section, key, defaultValue ? L"true" : L"false", buf, 32, filePath);
    return (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
}

ModConfig Config::Load(const wchar_t* iniPath) {
    ModConfig mcfg;

    // Controller settings
    mcfg.controller.deadzoneLeftStick   = GetPrivateProfileFloatW(L"Controller", L"DeadzoneLeftStick", 0.15f, iniPath);
    mcfg.controller.deadzoneRightStick  = GetPrivateProfileFloatW(L"Controller", L"DeadzoneRightStick", 0.15f, iniPath);
    mcfg.controller.invertRightStickX   = GetPrivateProfileBoolW(L"Controller", L"InvertRightStickX", false, iniPath);
    mcfg.controller.invertRightStickY   = GetPrivateProfileBoolW(L"Controller", L"InvertRightStickY", false, iniPath);
    mcfg.controller.cameraSensitivity   = GetPrivateProfileFloatW(L"Controller", L"CameraSensitivity", 1.0f, iniPath);
    mcfg.controller.triggerThreshold    = GetPrivateProfileFloatW(L"Controller", L"TriggerThreshold", 0.12f, iniPath);
    mcfg.controller.enableVibration     = GetPrivateProfileBoolW(L"Controller", L"EnableVibration", true, iniPath);
    mcfg.controller.vibrationStrength   = GetPrivateProfileFloatW(L"Controller", L"VibrationStrength", 1.0f, iniPath);
    mcfg.controller.enableXInputProxy   = GetPrivateProfileBoolW(L"System", L"EnableXInputProxy", true, iniPath);

    // Display & Borderless settings
    mcfg.display.enableBorderlessWindow = GetPrivateProfileBoolW(L"Display", L"EnableBorderlessWindow", true, iniPath);
    mcfg.display.showAspectRatioInMenu  = GetPrivateProfileBoolW(L"Display", L"ShowAspectRatioInMenu", true, iniPath);
    mcfg.display.targetFPS              = (int)GetPrivateProfileIntW(L"Display", L"TargetFPS", 60, iniPath);

    return mcfg;
}
