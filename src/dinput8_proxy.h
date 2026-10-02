#ifndef DINPUT8_PROXY_H
#define DINPUT8_PROXY_H

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <vector>
#include "xinput_translator.h"

// System dinput8 loader
bool LoadSystemDirectInput8();
extern HMODULE g_hSystemDInput8;

// Forward proxy export declarations
extern "C" {
    HRESULT WINAPI DirectInput8Create_Proxy(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
    HRESULT WINAPI DllCanUnloadNow_Proxy();
    HRESULT WINAPI DllGetClassObject_Proxy(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
    HRESULT WINAPI DllRegisterServer_Proxy();
    HRESULT WINAPI DllUnregisterServer_Proxy();
}

class ProxyIDirectInputEffect : public IDirectInputEffect {
private:
    IDirectInputEffect* m_realEffect;
    ULONG m_refCount;
    DWORD m_userIndex;
    GUID m_guid;
    DIEFFECT m_params;
    std::vector<BYTE> m_typeSpecificData;
    bool m_isPlaying;

public:
    ProxyIDirectInputEffect(IDirectInputEffect* realEffect, DWORD userIndex, REFGUID rguid, LPCDIEFFECT lpeff);
    virtual ~ProxyIDirectInputEffect();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirectInputEffect methods
    STDMETHOD(Initialize)(HINSTANCE hinst, DWORD dwVersion, REFGUID rguid) override;
    STDMETHOD(GetEffectGuid)(LPGUID pguid) override;
    STDMETHOD(GetParameters)(LPDIEFFECT peff, DWORD dwFlags) override;
    STDMETHOD(SetParameters)(LPCDIEFFECT peff, DWORD dwFlags) override;
    STDMETHOD(Start)(DWORD dwIterations, DWORD dwFlags) override;
    STDMETHOD(Stop)() override;
    STDMETHOD(GetEffectStatus)(LPDWORD pdwFlags) override;
    STDMETHOD(Download)() override;
    STDMETHOD(Unload)() override;
    STDMETHOD(Escape)(LPDIEFFESCAPE pesc) override;
};

class ProxyIDirectInputDevice8A : public IDirectInputDevice8A {
private:
    IDirectInputDevice8A* m_realDevice;
    bool m_isGamepad;
    DWORD m_userIndex;
    ULONG m_refCount;
    LONG m_minRange;
    LONG m_maxRange;

    // XInput event buffering for GetDeviceData (Menu Navigation)
    XINPUT_STATE m_lastXState;
    DWORD m_sequenceNum;
    std::vector<DIDEVICEOBJECTDATA> m_bufferedEvents;

    void PollXInputEvents();

public:
    ProxyIDirectInputDevice8A(IDirectInputDevice8A* realDev, bool isGamepad, DWORD userIndex = 0);
    virtual ~ProxyIDirectInputDevice8A();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirectInputDevice8A methods
    STDMETHOD(GetCapabilities)(LPDIDEVCAPS p0) override;
    STDMETHOD(SetProperty)(REFGUID r0, LPCDIPROPHEADER p1) override;
    STDMETHOD(GetProperty)(REFGUID r0, LPDIPROPHEADER p1) override;
    STDMETHOD(Acquire)() override;
    STDMETHOD(Unacquire)() override;
    STDMETHOD(GetDeviceState)(DWORD cbData, LPVOID lpvData) override;
    STDMETHOD(GetDeviceData)(DWORD r0, LPDIDEVICEOBJECTDATA r1, LPDWORD r2, DWORD r3) override;
    STDMETHOD(SetDataFormat)(LPCDIDATAFORMAT r0) override;
    STDMETHOD(SetEventNotification)(HANDLE r0) override;
    STDMETHOD(SetCooperativeLevel)(HWND r0, DWORD r1) override;
    STDMETHOD(GetObjectInfo)(LPDIDEVICEOBJECTINSTANCEA r0, DWORD r1, DWORD r2) override;
    STDMETHOD(GetDeviceInfo)(LPDIDEVICEINSTANCEA r0) override;
    STDMETHOD(RunControlPanel)(HWND r0, DWORD r1) override;
    STDMETHOD(Initialize)(HINSTANCE r0, DWORD r1, REFGUID r2) override;
    STDMETHOD(CreateEffect)(REFGUID r0, LPCDIEFFECT r1, LPDIRECTINPUTEFFECT* r2, LPUNKNOWN r3) override;
    STDMETHOD(EnumEffects)(LPDIENUMEFFECTSCALLBACKA r0, LPVOID r1, DWORD r2) override;
    STDMETHOD(GetEffectInfo)(LPDIEFFECTINFOA r0, REFGUID r1) override;
    STDMETHOD(GetForceFeedbackState)(LPDWORD r0) override;
    STDMETHOD(SendForceFeedbackCommand)(DWORD r0) override;
    STDMETHOD(EnumCreatedEffectObjects)(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK r0, LPVOID r1, DWORD r2) override;
    STDMETHOD(Escape)(LPDIEFFESCAPE r0) override;
    STDMETHOD(Poll)() override;
    STDMETHOD(SendDeviceData)(DWORD r0, LPCDIDEVICEOBJECTDATA r1, LPDWORD r2, DWORD r3) override;
    STDMETHOD(EnumObjects)(LPDIENUMDEVICEOBJECTSCALLBACKA r0, LPVOID r1, DWORD r2) override;
    STDMETHOD(BuildActionMap)(LPDIACTIONFORMATA r0, LPCSTR r1, DWORD r2) override;
    STDMETHOD(SetActionMap)(LPDIACTIONFORMATA r0, LPCSTR r1, DWORD r2) override;
    STDMETHOD(GetImageInfo)(LPDIDEVICEIMAGEINFOHEADERA r0) override;
    STDMETHOD(EnumEffectsInFile)(LPCSTR r0, LPDIENUMEFFECTSINFILECALLBACK r1, LPVOID r2, DWORD r3) override;
    STDMETHOD(WriteEffectToFile)(LPCSTR r0, DWORD r1, LPDIFILEEFFECT r2, DWORD r3) override;
};

class ProxyIDirectInput8A : public IDirectInput8A {
private:
    IDirectInput8A* m_realDInput;
    ULONG m_refCount;

public:
    ProxyIDirectInput8A(IDirectInput8A* realDInput);
    virtual ~ProxyIDirectInput8A();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirectInput8A methods
    STDMETHOD(CreateDevice)(REFGUID r0, LPDIRECTINPUTDEVICE8A* r1, LPUNKNOWN r2) override;
    STDMETHOD(EnumDevices)(DWORD r0, LPDIENUMDEVICESCALLBACKA r1, LPVOID r2, DWORD r3) override;
    STDMETHOD(GetDeviceStatus)(REFGUID r0) override;
    STDMETHOD(RunControlPanel)(HWND r0, DWORD r1) override;
    STDMETHOD(Initialize)(HINSTANCE r0, DWORD r1) override;
    STDMETHOD(FindDevice)(REFGUID r0, LPCSTR r1, LPGUID r2) override;
    STDMETHOD(EnumDevicesBySemantics)(LPCSTR r0, LPDIACTIONFORMATA r1, LPDIENUMDEVICESBYSEMANTICSCBA r2, LPVOID r3, DWORD r4) override;
    STDMETHOD(ConfigureDevices)(LPDICONFIGUREDEVICESCALLBACK r0, LPDICONFIGUREDEVICESPARAMSA r1, DWORD r2, LPVOID r3) override;
};

#endif // DINPUT8_PROXY_H
