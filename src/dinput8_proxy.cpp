#include "dinput8_proxy.h"
#include <shlobj.h>
#include <cstdio>
#include <algorithm>

HMODULE g_hSystemDInput8 = nullptr;
typedef HRESULT(WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static PFN_DirectInput8Create pfnSystemDirectInput8Create = nullptr;

bool LoadSystemDirectInput8() {
    if (g_hSystemDInput8) return true;

    wchar_t sysPath[MAX_PATH];
    GetSystemDirectoryW(sysPath, MAX_PATH);
    wcscat_s(sysPath, MAX_PATH, L"\\dinput8.dll");

    g_hSystemDInput8 = LoadLibraryW(sysPath);
    if (g_hSystemDInput8) {
        pfnSystemDirectInput8Create = (PFN_DirectInput8Create)GetProcAddress(g_hSystemDInput8, "DirectInput8Create");
        return (pfnSystemDirectInput8Create != nullptr);
    }
    return false;
}

extern "C" HRESULT WINAPI DirectInput8Create_Proxy(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter) {
    if (!LoadSystemDirectInput8()) {
        return DIERR_INVALIDPARAM;
    }

    IDirectInput8A* realDInput = nullptr;
    HRESULT hr = pfnSystemDirectInput8Create(hinst, dwVersion, riidltf, (LPVOID*)&realDInput, punkOuter);
    if (SUCCEEDED(hr) && realDInput) {
        *ppvOut = new ProxyIDirectInput8A(realDInput);
        return DI_OK;
    }
    return hr;
}

extern "C" HRESULT WINAPI DllCanUnloadNow_Proxy() {
    if (!LoadSystemDirectInput8()) return S_FALSE;
    typedef HRESULT(WINAPI* PFN_CanUnload)();
    PFN_CanUnload fn = (PFN_CanUnload)GetProcAddress(g_hSystemDInput8, "DllCanUnloadNow");
    return fn ? fn() : S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject_Proxy(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (!LoadSystemDirectInput8()) return CLASS_E_CLASSNOTAVAILABLE;
    typedef HRESULT(WINAPI* PFN_GetClass)(REFCLSID, REFIID, LPVOID*);
    PFN_GetClass fn = (PFN_GetClass)GetProcAddress(g_hSystemDInput8, "DllGetClassObject");
    return fn ? fn(rclsid, riid, ppv) : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI DllRegisterServer_Proxy() {
    if (!LoadSystemDirectInput8()) return E_FAIL;
    typedef HRESULT(WINAPI* PFN_Reg)();
    PFN_Reg fn = (PFN_Reg)GetProcAddress(g_hSystemDInput8, "DllRegisterServer");
    return fn ? fn() : E_FAIL;
}

extern "C" HRESULT WINAPI DllUnregisterServer_Proxy() {
    if (!LoadSystemDirectInput8()) return E_FAIL;
    typedef HRESULT(WINAPI* PFN_Unreg)();
    PFN_Unreg fn = (PFN_Unreg)GetProcAddress(g_hSystemDInput8, "DllUnregisterServer");
    return fn ? fn() : E_FAIL;
}

// ---------------------------------------------------------------------------
// ProxyIDirectInput8A Implementation
// ---------------------------------------------------------------------------

ProxyIDirectInput8A::ProxyIDirectInput8A(IDirectInput8A* realDInput)
    : m_realDInput(realDInput), m_refCount(1) {}

ProxyIDirectInput8A::~ProxyIDirectInput8A() {
    if (m_realDInput) {
        m_realDInput->Release();
    }
}

STDMETHODIMP ProxyIDirectInput8A::QueryInterface(REFIID riid, LPVOID* ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirectInput8A) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return m_realDInput->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) ProxyIDirectInput8A::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ProxyIDirectInput8A::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

STDMETHODIMP ProxyIDirectInput8A::CreateDevice(REFGUID r0, LPDIRECTINPUTDEVICE8A* r1, LPUNKNOWN r2) {
    if (!m_realDInput) return E_FAIL;

    bool isKeyboardOrMouse = (r0 == GUID_SysKeyboard || r0 == GUID_SysMouse);
    IDirectInputDevice8A* realDevice = nullptr;
    HRESULT hr = m_realDInput->CreateDevice(r0, &realDevice, r2);

    if (SUCCEEDED(hr) && realDevice) {
        if (isKeyboardOrMouse) {
            *r1 = realDevice;
        } else {
            *r1 = new ProxyIDirectInputDevice8A(realDevice, true, 0);
        }
        return DI_OK;
    }
    return hr;
}

STDMETHODIMP ProxyIDirectInput8A::EnumDevices(DWORD r0, LPDIENUMDEVICESCALLBACKA r1, LPVOID r2, DWORD r3) {
    return m_realDInput->EnumDevices(r0, r1, r2, r3);
}

STDMETHODIMP ProxyIDirectInput8A::GetDeviceStatus(REFGUID r0) { return m_realDInput->GetDeviceStatus(r0); }
STDMETHODIMP ProxyIDirectInput8A::RunControlPanel(HWND r0, DWORD r1) { return m_realDInput->RunControlPanel(r0, r1); }
STDMETHODIMP ProxyIDirectInput8A::Initialize(HINSTANCE r0, DWORD r1) { return m_realDInput->Initialize(r0, r1); }
STDMETHODIMP ProxyIDirectInput8A::FindDevice(REFGUID r0, LPCSTR r1, LPGUID r2) { return m_realDInput->FindDevice(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInput8A::EnumDevicesBySemantics(LPCSTR r0, LPDIACTIONFORMATA r1, LPDIENUMDEVICESBYSEMANTICSCBA r2, LPVOID r3, DWORD r4) {
    return m_realDInput->EnumDevicesBySemantics(r0, r1, r2, r3, r4);
}
STDMETHODIMP ProxyIDirectInput8A::ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK r0, LPDICONFIGUREDEVICESPARAMSA r1, DWORD r2, LPVOID r3) {
    return m_realDInput->ConfigureDevices(r0, r1, r2, r3);
}

// ---------------------------------------------------------------------------
// ProxyIDirectInputDevice8A Implementation
// ---------------------------------------------------------------------------

ProxyIDirectInputDevice8A::ProxyIDirectInputDevice8A(IDirectInputDevice8A* realDev, bool isGamepad, DWORD userIndex)
    : m_realDevice(realDev), m_isGamepad(isGamepad), m_userIndex(userIndex), m_refCount(1),
      m_minRange(-32768), m_maxRange(32767), m_sequenceNum(0) {
    ZeroMemory(&m_lastXState, sizeof(XINPUT_STATE));
}

ProxyIDirectInputDevice8A::~ProxyIDirectInputDevice8A() {
    if (m_realDevice) {
        m_realDevice->Release();
    }
}

STDMETHODIMP ProxyIDirectInputDevice8A::QueryInterface(REFIID riid, LPVOID* ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirectInputDevice8A) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return m_realDevice->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) ProxyIDirectInputDevice8A::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ProxyIDirectInputDevice8A::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

STDMETHODIMP ProxyIDirectInputDevice8A::SetProperty(REFGUID r0, LPCDIPROPHEADER p1) {
    if (p1 && (&r0 == &DIPROP_RANGE)) {
        LPDIPROPRANGE prange = (LPDIPROPRANGE)p1;
        m_minRange = prange->lMin;
        m_maxRange = prange->lMax;
    }
    return m_realDevice->SetProperty(r0, p1);
}

STDMETHODIMP ProxyIDirectInputDevice8A::GetProperty(REFGUID r0, LPDIPROPHEADER p1) {
    return m_realDevice->GetProperty(r0, p1);
}

void ProxyIDirectInputDevice8A::PollXInputEvents() {
    if (!m_isGamepad || !XInputTranslator::IsConnected(m_userIndex)) return;

    // Fetch the raw XInput state so we can compare wButtons bitmasks properly.
    // We use a locally-cached function pointer to avoid repeated GetProcAddress overhead.
    typedef DWORD(WINAPI* PFN_XInputGetState)(DWORD, XINPUT_STATE*);
    static PFN_XInputGetState s_pfnXInputGetState = nullptr;
    if (!s_pfnXInputGetState) {
        HMODULE hXInput = LoadLibraryA("xinput1_4.dll");
        if (!hXInput) hXInput = LoadLibraryA("xinput1_3.dll");
        if (!hXInput) hXInput = LoadLibraryA("xinput9_1_0.dll");
        if (hXInput) s_pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(hXInput, "XInputGetState");
    }
    if (!s_pfnXInputGetState) return;

    XINPUT_STATE currXState;
    ZeroMemory(&currXState, sizeof(currXState));
    if (s_pfnXInputGetState(m_userIndex, &currXState) != ERROR_SUCCESS) return;

    // Button map: XInput bitmask -> DInput button offset
    struct ButtonMapping {
        WORD  xinputMask;
        DWORD dinputOffset;
    } buttons[] = {
        { XINPUT_GAMEPAD_A,              DIJOFS_BUTTON(0) },
        { XINPUT_GAMEPAD_B,              DIJOFS_BUTTON(1) },
        { XINPUT_GAMEPAD_X,              DIJOFS_BUTTON(2) },
        { XINPUT_GAMEPAD_Y,              DIJOFS_BUTTON(3) },
        { XINPUT_GAMEPAD_LEFT_SHOULDER,  DIJOFS_BUTTON(4) },
        { XINPUT_GAMEPAD_RIGHT_SHOULDER, DIJOFS_BUTTON(5) },
        { XINPUT_GAMEPAD_BACK,           DIJOFS_BUTTON(6) },
        { XINPUT_GAMEPAD_START,          DIJOFS_BUTTON(7) },
        { XINPUT_GAMEPAD_LEFT_THUMB,     DIJOFS_BUTTON(8) },
        { XINPUT_GAMEPAD_RIGHT_THUMB,    DIJOFS_BUTTON(9) },
    };

    // Compare stored previous state vs current state and emit press/release events
    WORD lastB = m_lastXState.Gamepad.wButtons;
    WORD currB = currXState.Gamepad.wButtons;

    for (int i = 0; i < 10; i++) {
        bool lastPressed = (lastB & buttons[i].xinputMask) != 0;
        bool currPressed = (currB & buttons[i].xinputMask) != 0;

        if (lastPressed != currPressed) {
            DIDEVICEOBJECTDATA data;
            data.dwOfs       = buttons[i].dinputOffset;
            data.dwData      = currPressed ? 0x80 : 0x00;
            data.dwTimeStamp = GetTickCount();
            data.dwSequence  = m_sequenceNum++;
            data.uAppData    = 0;
            m_bufferedEvents.push_back(data);
        }
    }

    // Advance last-state so next poll detects new transitions
    m_lastXState = currXState;
}

STDMETHODIMP ProxyIDirectInputDevice8A::GetDeviceState(DWORD cbData, LPVOID lpvData) {
    if (m_isGamepad && lpvData && XInputTranslator::IsConnected(m_userIndex)) {
        if (cbData == sizeof(DIJOYSTATE2)) {
            DIJOYSTATE2* pState = (DIJOYSTATE2*)lpvData;
            if (XInputTranslator::GetTranslatedState(m_userIndex, pState, m_minRange, m_maxRange)) {
                return DI_OK;
            }
        } else if (cbData == sizeof(DIJOYSTATE)) {
            DIJOYSTATE2 js2;
            if (XInputTranslator::GetTranslatedState(m_userIndex, &js2, m_minRange, m_maxRange)) {
                DIJOYSTATE* pState = (DIJOYSTATE*)lpvData;
                pState->lX = js2.lX;
                pState->lY = js2.lY;
                pState->lZ = js2.lZ;
                pState->lRx = js2.lRx;
                pState->lRy = js2.lRy;
                pState->lRz = js2.lRz;
                pState->rglSlider[0] = js2.rglSlider[0];
                pState->rglSlider[1] = js2.rglSlider[1];
                memcpy(pState->rgdwPOV, js2.rgdwPOV, sizeof(pState->rgdwPOV));
                memcpy(pState->rgbButtons, js2.rgbButtons, 32);
                return DI_OK;
            }
        }
    }
    return m_realDevice->GetDeviceState(cbData, lpvData);
}

STDMETHODIMP ProxyIDirectInputDevice8A::GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA r1, LPDWORD r2, DWORD r3) {
    if (m_isGamepad && r2 && XInputTranslator::IsConnected(m_userIndex)) {
        PollXInputEvents();

        if (!r1) {
            // Count query
            *r2 = (DWORD)m_bufferedEvents.size();
            return DI_OK;
        }

        DWORD available = (DWORD)m_bufferedEvents.size();
        DWORD toCopy = (std::min)(*r2, available);

        for (DWORD i = 0; i < toCopy; i++) {
            r1[i] = m_bufferedEvents[i];
        }

        if (!(r3 & DIGDD_PEEK)) {
            m_bufferedEvents.erase(m_bufferedEvents.begin(), m_bufferedEvents.begin() + toCopy);
        }

        *r2 = toCopy;
        return DI_OK;
    }
    return m_realDevice->GetDeviceData(cbObjectData, r1, r2, r3);
}

// ---------------------------------------------------------------------------
// ProxyIDirectInputEffect Implementation (XInput Vibration)
// ---------------------------------------------------------------------------

ProxyIDirectInputEffect::ProxyIDirectInputEffect(IDirectInputEffect* realEffect, DWORD userIndex, REFGUID rguid, LPCDIEFFECT lpeff)
    : m_realEffect(realEffect), m_refCount(1), m_userIndex(userIndex), m_guid(rguid), m_isPlaying(false) {
    ZeroMemory(&m_params, sizeof(DIEFFECT));
    if (lpeff) {
        m_params = *lpeff;
        if (lpeff->lpvTypeSpecificParams && lpeff->cbTypeSpecificParams > 0) {
            m_typeSpecificData.resize(lpeff->cbTypeSpecificParams);
            memcpy(m_typeSpecificData.data(), lpeff->lpvTypeSpecificParams, lpeff->cbTypeSpecificParams);
            m_params.lpvTypeSpecificParams = m_typeSpecificData.data();
        }
    }
}

ProxyIDirectInputEffect::~ProxyIDirectInputEffect() {
    Stop();
    if (m_realEffect) {
        m_realEffect->Release();
    }
}

STDMETHODIMP ProxyIDirectInputEffect::QueryInterface(REFIID riid, LPVOID* ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirectInputEffect) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    if (m_realEffect) return m_realEffect->QueryInterface(riid, ppvObj);
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) ProxyIDirectInputEffect::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ProxyIDirectInputEffect::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ProxyIDirectInputEffect::Initialize(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) {
    m_guid = rguid;
    return m_realEffect ? m_realEffect->Initialize(hinst, dwVersion, rguid) : DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::GetEffectGuid(LPGUID pguid) {
    if (pguid) *pguid = m_guid;
    return DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::GetParameters(LPDIEFFECT peff, DWORD dwFlags) {
    if (peff) *peff = m_params;
    return DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::SetParameters(LPCDIEFFECT peff, DWORD dwFlags) {
    if (peff) {
        m_params = *peff;
        if (peff->lpvTypeSpecificParams && peff->cbTypeSpecificParams > 0) {
            m_typeSpecificData.resize(peff->cbTypeSpecificParams);
            memcpy(m_typeSpecificData.data(), peff->lpvTypeSpecificParams, peff->cbTypeSpecificParams);
            m_params.lpvTypeSpecificParams = m_typeSpecificData.data();
        }
        if (m_isPlaying) {
            Start(1, 0);
        }
    }
    return m_realEffect ? m_realEffect->SetParameters(peff, dwFlags) : DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::Start(DWORD dwIterations, DWORD dwFlags) {
    float magNorm = 0.8f; // default medium vibration

    // Inspect type specific parameters for magnitude (Periodic vs Constant Force)
    if (m_params.lpvTypeSpecificParams && m_typeSpecificData.size() >= sizeof(DWORD)) {
        if (m_guid == GUID_Sine || m_guid == GUID_Square || m_guid == GUID_Triangle || 
            m_guid == GUID_SawtoothUp || m_guid == GUID_SawtoothDown) {
            if (m_typeSpecificData.size() >= sizeof(DIPERIODIC)) {
                DIPERIODIC* p = reinterpret_cast<DIPERIODIC*>(m_typeSpecificData.data());
                magNorm = (float)p->dwMagnitude / 10000.0f;
            }
        } else if (m_guid == GUID_ConstantForce) {
            if (m_typeSpecificData.size() >= sizeof(DICONSTANTFORCE)) {
                DICONSTANTFORCE* c = reinterpret_cast<DICONSTANTFORCE*>(m_typeSpecificData.data());
                magNorm = (float)std::abs(c->lMagnitude) / 10000.0f;
            }
        } else if (m_guid == GUID_RampForce) {
            if (m_typeSpecificData.size() >= sizeof(DIRAMPFORCE)) {
                DIRAMPFORCE* r = reinterpret_cast<DIRAMPFORCE*>(m_typeSpecificData.data());
                LONG avgMag = (std::abs(r->lStart) + std::abs(r->lEnd)) / 2;
                magNorm = (float)avgMag / 10000.0f;
            }
        }
    }

    if (m_params.dwGain > 0 && m_params.dwGain <= 10000) {
        magNorm *= ((float)m_params.dwGain / 10000.0f);
    }

    magNorm = (std::min)((std::max)(magNorm, 0.0f), 1.0f);

    WORD leftMotor  = (WORD)(magNorm * 65535.0f);
    WORD rightMotor = (WORD)(magNorm * 52428.0f); // 80% on high-frequency motor for balanced punch

    XInputTranslator::SetVibration(m_userIndex, leftMotor, rightMotor);
    m_isPlaying = true;

    if (m_realEffect) m_realEffect->Start(dwIterations, dwFlags);
    return DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::Stop() {
    m_isPlaying = false;
    XInputTranslator::StopVibration(m_userIndex);
    if (m_realEffect) m_realEffect->Stop();
    return DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::GetEffectStatus(LPDWORD pdwFlags) {
    if (pdwFlags) {
        *pdwFlags = m_isPlaying ? DIEGES_PLAYING : 0;
    }
    return DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::Download() {
    return m_realEffect ? m_realEffect->Download() : DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::Unload() {
    Stop();
    return m_realEffect ? m_realEffect->Unload() : DI_OK;
}

STDMETHODIMP ProxyIDirectInputEffect::Escape(LPDIEFFESCAPE pesc) {
    return m_realEffect ? m_realEffect->Escape(pesc) : DI_OK;
}

// ---------------------------------------------------------------------------
// ProxyIDirectInputDevice8A Methods
// ---------------------------------------------------------------------------

STDMETHODIMP ProxyIDirectInputDevice8A::GetCapabilities(LPDIDEVCAPS p0) {
    HRESULT hr = m_realDevice ? m_realDevice->GetCapabilities(p0) : DI_OK;
    if (p0 && m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        p0->dwFlags |= DIDC_FORCEFEEDBACK | DIDC_ATTACHED;
        p0->dwFFSamplePeriod = 1000;
        p0->dwFFMinTimeResolution = 1000;
    }
    return hr;
}

STDMETHODIMP ProxyIDirectInputDevice8A::Acquire() { return m_realDevice->Acquire(); }
STDMETHODIMP ProxyIDirectInputDevice8A::Unacquire() { return m_realDevice->Unacquire(); }
STDMETHODIMP ProxyIDirectInputDevice8A::SetDataFormat(LPCDIDATAFORMAT r0) { return m_realDevice->SetDataFormat(r0); }
STDMETHODIMP ProxyIDirectInputDevice8A::SetEventNotification(HANDLE r0) { return m_realDevice->SetEventNotification(r0); }
STDMETHODIMP ProxyIDirectInputDevice8A::SetCooperativeLevel(HWND r0, DWORD r1) { return m_realDevice->SetCooperativeLevel(r0, r1); }
STDMETHODIMP ProxyIDirectInputDevice8A::GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA r0, DWORD r1, DWORD r2) { return m_realDevice->GetObjectInfo(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInputDevice8A::GetDeviceInfo(LPDIDEVICEINSTANCEA r0) { return m_realDevice->GetDeviceInfo(r0); }
STDMETHODIMP ProxyIDirectInputDevice8A::RunControlPanel(HWND r0, DWORD r1) { return m_realDevice->RunControlPanel(r0, r1); }
STDMETHODIMP ProxyIDirectInputDevice8A::Initialize(HINSTANCE r0, DWORD r1, REFGUID r2) { return m_realDevice->Initialize(r0, r1, r2); }

STDMETHODIMP ProxyIDirectInputDevice8A::CreateEffect(REFGUID r0, LPCDIEFFECT r1, LPDIRECTINPUTEFFECT* r2, LPUNKNOWN r3) {
    if (m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        IDirectInputEffect* realEffect = nullptr;
        if (m_realDevice) {
            m_realDevice->CreateEffect(r0, r1, &realEffect, r3);
        }
        if (r2) {
            *r2 = new ProxyIDirectInputEffect(realEffect, m_userIndex, r0, r1);
            return DI_OK;
        }
    }
    return m_realDevice ? m_realDevice->CreateEffect(r0, r1, r2, r3) : E_FAIL;
}

STDMETHODIMP ProxyIDirectInputDevice8A::EnumEffects(LPDIENUMEFFECTSCALLBACKA r0, LPVOID r1, DWORD r2) {
    if (!r0) return DIERR_INVALIDPARAM;
    if (m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        DIEFFECTINFOA info;
        ZeroMemory(&info, sizeof(info));
        info.dwSize = sizeof(info);
        info.dwStaticParams = DIEP_ALLPARAMS;
        info.dwDynamicParams = DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION | DIEP_DURATION;

        info.guid = GUID_Sine;
        info.dwEffType = DIEFT_PERIODIC;
        strcpy_s(info.tszName, "Sine Vibration");
        if (r0(&info, r1) == DIENUM_STOP) return DI_OK;

        info.guid = GUID_Square;
        info.dwEffType = DIEFT_PERIODIC;
        strcpy_s(info.tszName, "Square Vibration");
        if (r0(&info, r1) == DIENUM_STOP) return DI_OK;

        info.guid = GUID_ConstantForce;
        info.dwEffType = DIEFT_CONSTANTFORCE;
        strcpy_s(info.tszName, "Constant Vibration");
        if (r0(&info, r1) == DIENUM_STOP) return DI_OK;

        info.guid = GUID_RampForce;
        info.dwEffType = DIEFT_RAMPFORCE;
        strcpy_s(info.tszName, "Ramp Vibration");
        if (r0(&info, r1) == DIENUM_STOP) return DI_OK;

        return DI_OK;
    }
    return m_realDevice->EnumEffects(r0, r1, r2);
}

STDMETHODIMP ProxyIDirectInputDevice8A::GetEffectInfo(LPDIEFFECTINFOA r0, REFGUID r1) {
    if (r0 && m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        ZeroMemory(r0, sizeof(DIEFFECTINFOA));
        r0->dwSize = sizeof(DIEFFECTINFOA);
        r0->guid = r1;
        r0->dwEffType = DIEFT_PERIODIC;
        r0->dwStaticParams = DIEP_ALLPARAMS;
        r0->dwDynamicParams = DIEP_TYPESPECIFICPARAMS | DIEP_DIRECTION | DIEP_DURATION;
        strcpy_s(r0->tszName, "Vibration Effect");
        return DI_OK;
    }
    return m_realDevice ? m_realDevice->GetEffectInfo(r0, r1) : DI_OK;
}

STDMETHODIMP ProxyIDirectInputDevice8A::GetForceFeedbackState(LPDWORD r0) {
    if (m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        if (r0) *r0 = DIGFFS_ACTUATORSON | DIGFFS_POWERON;
        return DI_OK;
    }
    return m_realDevice ? m_realDevice->GetForceFeedbackState(r0) : DI_OK;
}

STDMETHODIMP ProxyIDirectInputDevice8A::SendForceFeedbackCommand(DWORD r0) {
    if (m_isGamepad && XInputTranslator::GetConfig().enableVibration) {
        if (r0 & (DISFFC_STOPALL | DISFFC_PAUSE | DISFFC_RESET)) {
            XInputTranslator::StopVibration(m_userIndex);
        }
        return DI_OK;
    }
    return m_realDevice ? m_realDevice->SendForceFeedbackCommand(r0) : DI_OK;
}

STDMETHODIMP ProxyIDirectInputDevice8A::EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK r0, LPVOID r1, DWORD r2) { return m_realDevice->EnumCreatedEffectObjects(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInputDevice8A::Escape(LPDIEFFESCAPE r0) { return m_realDevice->Escape(r0); }
STDMETHODIMP ProxyIDirectInputDevice8A::Poll() { return m_realDevice->Poll(); }
STDMETHODIMP ProxyIDirectInputDevice8A::SendDeviceData(DWORD r0, LPCDIDEVICEOBJECTDATA r1, LPDWORD r2, DWORD r3) { return m_realDevice->SendDeviceData(r0, r1, r2, r3); }
STDMETHODIMP ProxyIDirectInputDevice8A::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA r0, LPVOID r1, DWORD r2) { return m_realDevice->EnumObjects(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInputDevice8A::BuildActionMap(LPDIACTIONFORMATA r0, LPCSTR r1, DWORD r2) { return m_realDevice->BuildActionMap(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInputDevice8A::SetActionMap(LPDIACTIONFORMATA r0, LPCSTR r1, DWORD r2) { return m_realDevice->SetActionMap(r0, r1, r2); }
STDMETHODIMP ProxyIDirectInputDevice8A::GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA r0) { return m_realDevice->GetImageInfo(r0); }
STDMETHODIMP ProxyIDirectInputDevice8A::EnumEffectsInFile(LPCSTR r0, LPDIENUMEFFECTSINFILECALLBACK r1, LPVOID r2, DWORD r3) { return m_realDevice->EnumEffectsInFile(r0, r1, r2, r3); }
STDMETHODIMP ProxyIDirectInputDevice8A::WriteEffectToFile(LPCSTR r0, DWORD r1, LPDIFILEEFFECT r2, DWORD r3) { return m_realDevice->WriteEffectToFile(r0, r1, r2, r3); }
