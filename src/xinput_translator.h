#ifndef XINPUT_TRANSLATOR_H
#define XINPUT_TRANSLATOR_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <dinput.h>
#include <cmath>

struct ControllerConfig {
    float deadzoneLeftStick = 0.15f;
    float deadzoneRightStick = 0.15f;
    bool invertRightStickX = false;
    bool invertRightStickY = false;
    float cameraSensitivity = 1.0f;
    float triggerThreshold = 0.12f;
    bool enableXInputProxy = true;
    bool enableVibration = true;
    float vibrationStrength = 1.0f;
};

class XInputTranslator {
public:
    static bool Initialize();
    static bool IsConnected(DWORD userIndex = 0);
    static bool GetTranslatedState(DWORD userIndex, DIJOYSTATE2* outState, LONG minRange = -32768, LONG maxRange = 32767);
    static bool SetVibration(DWORD userIndex, WORD leftMotor, WORD rightMotor);
    static void StopVibration(DWORD userIndex = 0);
    static void SetConfig(const ControllerConfig& cfg);
    static ControllerConfig GetConfig();

private:
    static ControllerConfig g_config;
    static HMODULE g_hXInput;

    typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
    typedef DWORD(WINAPI* PFN_XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
    static PFN_XInputGetState pfnXInputGetState;
    static PFN_XInputSetState pfnXInputSetState;

    static void ApplyRadialDeadzone(SHORT rawX, SHORT rawY, float deadzonePercent, LONG& outX, LONG& outY, LONG minRange, LONG maxRange);
    static DWORD ConvertDPadToPOV(WORD wButtons);
};

#endif // XINPUT_TRANSLATOR_H
