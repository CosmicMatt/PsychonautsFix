#include "xinput_translator.h"
#include <algorithm>

ControllerConfig XInputTranslator::g_config;
HMODULE XInputTranslator::g_hXInput = nullptr;
XInputTranslator::PFN_XInputGetState XInputTranslator::pfnXInputGetState = nullptr;
XInputTranslator::PFN_XInputSetState XInputTranslator::pfnXInputSetState = nullptr;

bool XInputTranslator::Initialize() {
    if (g_hXInput) return true;

    const wchar_t* dllNames[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll"
    };

    for (const auto* name : dllNames) {
        g_hXInput = LoadLibraryW(name);
        if (g_hXInput) {
            pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(g_hXInput, "XInputGetState");
            pfnXInputSetState = (PFN_XInputSetState)GetProcAddress(g_hXInput, "XInputSetState");
            if (pfnXInputGetState) {
                return true;
            }
            FreeLibrary(g_hXInput);
            g_hXInput = nullptr;
        }
    }
    return false;
}

void XInputTranslator::SetConfig(const ControllerConfig& cfg) {
    g_config = cfg;
}

ControllerConfig XInputTranslator::GetConfig() {
    return g_config;
}

bool XInputTranslator::IsConnected(DWORD userIndex) {
    if (!g_hXInput && !Initialize()) return false;
    if (!pfnXInputGetState) return false;

    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(XINPUT_STATE));
    return (pfnXInputGetState(userIndex, &state) == ERROR_SUCCESS);
}

void XInputTranslator::ApplyRadialDeadzone(SHORT rawX, SHORT rawY, float deadzonePercent, LONG& outX, LONG& outY, LONG minRange, LONG maxRange) {
    LONG center = (minRange + maxRange) / 2;
    LONG halfSpan = (maxRange - minRange) / 2;
    if (halfSpan <= 0) halfSpan = 32767;

    float fx = (float)rawX / 32767.0f;
    float fy = (float)rawY / 32767.0f;
    float mag = std::sqrt(fx * fx + fy * fy);

    if (mag <= deadzonePercent || mag <= 0.0001f) {
        outX = center;
        outY = center;
        return;
    }

    float normMag = (mag - deadzonePercent) / (1.0f - deadzonePercent);
    normMag = (std::min)((std::max)(normMag, 0.0f), 1.0f);

    float dirX = fx / mag;
    float dirY = fy / mag;

    outX = center + (LONG)(dirX * normMag * halfSpan);
    outY = center + (LONG)(dirY * normMag * halfSpan);
}

DWORD XInputTranslator::ConvertDPadToPOV(WORD wButtons) {
    bool up    = (wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    bool down  = (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    bool left  = (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    bool right = (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;

    if (up && right)   return 4500;
    if (down && right) return 13500;
    if (down && left)  return 22500;
    if (up && left)    return 31500;

    if (up)    return 0;
    if (right) return 9000;
    if (down)  return 18000;
    if (left)  return 27000;

    return 0xFFFFFFFF; // Centered
}

bool XInputTranslator::GetTranslatedState(DWORD userIndex, DIJOYSTATE2* outState, LONG minRange, LONG maxRange) {
    if (!g_hXInput && !Initialize()) return false;
    if (!pfnXInputGetState || !outState) return false;

    XINPUT_STATE xstate;
    ZeroMemory(&xstate, sizeof(XINPUT_STATE));
    if (pfnXInputGetState(userIndex, &xstate) != ERROR_SUCCESS) {
        return false;
    }

    ZeroMemory(outState, sizeof(DIJOYSTATE2));
    LONG center = (minRange + maxRange) / 2;

    // Default all axes to center
    outState->lX  = center;
    outState->lY  = center;
    outState->lZ  = center; // ISOLATED CAMERA Z-AXIS (No camera spin!)
    outState->lRx = center;
    outState->lRy = center;
    outState->lRz = center;
    outState->rglSlider[0] = center;
    outState->rglSlider[1] = center;

    // 1. Left Stick -> 360 Analog Movement (lX, lY)
    ApplyRadialDeadzone(xstate.Gamepad.sThumbLX, xstate.Gamepad.sThumbLY,
                        g_config.deadzoneLeftStick,
                        outState->lX, outState->lY, minRange, maxRange);

    // Note: DirectInput Y is inverted compared to XInput (Up is negative/minRange in DInput)
    LONG halfSpan = (maxRange - minRange) / 2;
    outState->lY = center - (outState->lY - center);

    // 2. Right Stick -> Camera Yaw/Pitch (lRx, lRy)
    SHORT rxRaw = xstate.Gamepad.sThumbRX;
    SHORT ryRaw = xstate.Gamepad.sThumbRY;

    if (g_config.invertRightStickX) rxRaw = -rxRaw;
    if (g_config.invertRightStickY) ryRaw = -ryRaw;

    ApplyRadialDeadzone(rxRaw, ryRaw,
                        g_config.deadzoneRightStick,
                        outState->lRx, outState->lRy, minRange, maxRange);

    // Apply sensitivity scaling if non-default
    if (g_config.cameraSensitivity != 1.0f) {
        LONG deltaX = (LONG)((outState->lRx - center) * g_config.cameraSensitivity);
        LONG deltaY = (LONG)((outState->lRy - center) * g_config.cameraSensitivity);
        outState->lRx = (std::min)((std::max)(center + deltaX, minRange), maxRange);
        outState->lRy = (std::min)((std::max)(center + deltaY, minRange), maxRange);
    }

    // DirectInput Y axis inverted for camera pitch
    outState->lRy = center - (outState->lRy - center);

    // CRITICAL: Keep lZ at center (0) so JoyZ never triggers camera spinning!
    outState->lZ = center;

    // 3. D-Pad -> POV 0
    outState->rgdwPOV[0] = ConvertDPadToPOV(xstate.Gamepad.wButtons);
    outState->rgdwPOV[1] = 0xFFFFFFFF;
    outState->rgdwPOV[2] = 0xFFFFFFFF;
    outState->rgdwPOV[3] = 0xFFFFFFFF;

    // 4. Buttons Mapping
    WORD b = xstate.Gamepad.wButtons;
    outState->rgbButtons[0] = (b & XINPUT_GAMEPAD_A) ? 0x80 : 0x00;
    outState->rgbButtons[1] = (b & XINPUT_GAMEPAD_B) ? 0x80 : 0x00;
    outState->rgbButtons[2] = (b & XINPUT_GAMEPAD_X) ? 0x80 : 0x00;
    outState->rgbButtons[3] = (b & XINPUT_GAMEPAD_Y) ? 0x80 : 0x00;
    outState->rgbButtons[4] = (b & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 0x80 : 0x00;
    outState->rgbButtons[5] = (b & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 0x80 : 0x00;
    outState->rgbButtons[6] = (b & XINPUT_GAMEPAD_BACK) ? 0x80 : 0x00;
    outState->rgbButtons[7] = (b & XINPUT_GAMEPAD_START) ? 0x80 : 0x00;
    outState->rgbButtons[8] = (b & XINPUT_GAMEPAD_LEFT_THUMB) ? 0x80 : 0x00;
    outState->rgbButtons[9] = (b & XINPUT_GAMEPAD_RIGHT_THUMB) ? 0x80 : 0x00;

    // Triggers mapped to dedicated digital button indices (10 and 11)
    BYTE triggerThreshByte = (BYTE)(g_config.triggerThreshold * 255.0f);
    outState->rgbButtons[10] = (xstate.Gamepad.bLeftTrigger > triggerThreshByte) ? 0x80 : 0x00;
    outState->rgbButtons[11] = (xstate.Gamepad.bRightTrigger > triggerThreshByte) ? 0x80 : 0x00;

    return true;
}

bool XInputTranslator::SetVibration(DWORD userIndex, WORD leftMotor, WORD rightMotor) {
    if (!g_config.enableVibration) return false;
    if (!g_hXInput && !Initialize()) return false;
    if (!pfnXInputSetState) return false;

    // Apply strength multiplier
    if (g_config.vibrationStrength != 1.0f) {
        float l = (float)leftMotor * g_config.vibrationStrength;
        float r = (float)rightMotor * g_config.vibrationStrength;
        leftMotor  = (WORD)(std::min)((std::max)(l, 0.0f), 65535.0f);
        rightMotor = (WORD)(std::min)((std::max)(r, 0.0f), 65535.0f);
    }

    XINPUT_VIBRATION vib;
    vib.wLeftMotorSpeed = leftMotor;
    vib.wRightMotorSpeed = rightMotor;
    return (pfnXInputSetState(userIndex, &vib) == ERROR_SUCCESS);
}

void XInputTranslator::StopVibration(DWORD userIndex) {
    if (!g_hXInput && !Initialize()) return;
    if (!pfnXInputSetState) return;

    XINPUT_VIBRATION vib;
    vib.wLeftMotorSpeed = 0;
    vib.wRightMotorSpeed = 0;
    pfnXInputSetState(userIndex, &vib);
}

