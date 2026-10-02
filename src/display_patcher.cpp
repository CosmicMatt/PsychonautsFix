#include "display_patcher.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

DisplayConfig DisplayPatcher::g_config;
HWND DisplayPatcher::g_hGameWnd = nullptr;
bool DisplayPatcher::g_hooksInstalled = false;
LARGE_INTEGER DisplayPatcher::g_frequency = {0};
LARGE_INTEGER DisplayPatcher::g_lastFrameTime = {0};

typedef LONG (WINAPI* PFN_ChangeDisplaySettingsA)(DEVMODEA* lpDevMode, DWORD dwFlags);
typedef HWND (WINAPI* PFN_CreateWindowExA)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
typedef BOOL (WINAPI* PFN_SetWindowPos)(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
typedef IDirect3D9* (WINAPI* PFN_Direct3DCreate9)(UINT SDKVersion);
typedef int (WINAPIV* PFN_wsprintfA)(LPSTR lpOut, LPCSTR lpFmt, ...);
typedef int (__cdecl* PFN_sprintf)(char* buffer, const char* format, ...);
typedef int (__cdecl* PFN_snprintf)(char* buffer, size_t count, const char* format, ...);

static PFN_ChangeDisplaySettingsA g_pfnRealChangeDisplaySettingsA = nullptr;
static PFN_CreateWindowExA g_pfnRealCreateWindowExA = nullptr;
static PFN_SetWindowPos g_pfnRealSetWindowPos = nullptr;
static PFN_Direct3DCreate9 g_pfnRealDirect3DCreate9 = nullptr;
static PFN_wsprintfA g_pfnRealwsprintfA = nullptr;
static PFN_sprintf g_pfnRealsprintf = nullptr;
static PFN_snprintf g_pfnRealsnprintf = nullptr;

LONG WINAPI Hook_ChangeDisplaySettingsA(DEVMODEA* lpDevMode, DWORD dwFlags) {
    if (DisplayPatcher::GetConfig().enableBorderlessWindow) {
        return DISP_CHANGE_SUCCESSFUL;
    }
    return g_pfnRealChangeDisplaySettingsA ? g_pfnRealChangeDisplaySettingsA(lpDevMode, dwFlags) : DISP_CHANGE_SUCCESSFUL;
}

HWND WINAPI Hook_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    if (DisplayPatcher::GetConfig().enableBorderlessWindow) {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_OVERLAPPED);
        dwStyle |= WS_POPUP | WS_VISIBLE;
        dwExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        X = 0;
        Y = 0;
        nWidth = screenW;
        nHeight = screenH;
    }
    HWND hWnd = g_pfnRealCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (hWnd && DisplayPatcher::GetConfig().enableBorderlessWindow) {
        SetWindowPos(hWnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }
    return hWnd;
}

BOOL WINAPI Hook_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    if (DisplayPatcher::GetConfig().enableBorderlessWindow) {
        // Only override for large top-level windows that look like the main game window.
        // Avoid stomping on dialogs, child controls, and tooltips.
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        bool looksLikeMainWindow = (cx > screenW / 4) && (cy > screenH / 4) && !GetParent(hWnd);
        if (looksLikeMainWindow) {
            X = 0; Y = 0;
            cx = screenW;
            cy = screenH;
            uFlags |= SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW;
        }
    }
    return g_pfnRealSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}


IDirect3D9* WINAPI Hook_Direct3DCreate9(UINT SDKVersion) {
    IDirect3D9* realD3D = g_pfnRealDirect3DCreate9 ? g_pfnRealDirect3DCreate9(SDKVersion) : nullptr;
    if (realD3D) {
        return new ProxyIDirect3D9(realD3D);
    }
    return nullptr;
}

int WINAPIV Hook_wsprintfA(LPSTR lpOut, LPCSTR lpFmt, ...) {
    // Format into a local buffer first to avoid overflowing lpOut whose size we don't know.
    char tmp[1024];
    va_list args;
    va_start(args, lpFmt);
    int res = vsprintf_s(tmp, sizeof(tmp), lpFmt, args);
    va_end(args);
    if (res < 0) res = 0;

    if (DisplayPatcher::GetConfig().showAspectRatioInMenu && lpFmt && strstr(lpFmt, "%d")) {
        std::string annotated = DisplayPatcher::AnnotateResolutionString(tmp);
        if (annotated != tmp) {
            strcpy_s(tmp, sizeof(tmp), annotated.c_str());
            res = (int)strlen(tmp);
        }
    }
    // Copy result back — use tmp length as the bound; caller must have allocated at least this much
    // (wsprintfA docs say output buffer must hold MAX_PATH or more for safety)
    memcpy(lpOut, tmp, (size_t)res + 1);
    return res;
}


int __cdecl Hook_sprintf(char* buffer, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = vsprintf_s(buffer, 1024, format, args);
    va_end(args);

    if (DisplayPatcher::GetConfig().showAspectRatioInMenu && format && strstr(format, "%d")) {
        std::string annotated = DisplayPatcher::AnnotateResolutionString(buffer);
        if (annotated != buffer) {
            strcpy_s(buffer, 1024, annotated.c_str());
            res = (int)strlen(buffer);
        }
    }
    return res;
}

int __cdecl Hook_snprintf(char* buffer, size_t count, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int res = vsnprintf_s(buffer, count, _TRUNCATE, format, args);
    va_end(args);

    if (DisplayPatcher::GetConfig().showAspectRatioInMenu && format && strstr(format, "%d")) {
        std::string annotated = DisplayPatcher::AnnotateResolutionString(buffer);
        if (annotated != buffer) {
            strcpy_s(buffer, count, annotated.c_str());
            res = (int)strlen(buffer);
        }
    }
    return res;
}

static bool HookIAT(HMODULE hModule, const char* dllName, const char* funcName, void* hookFunc, void** origFunc) {
    if (!hModule) return false;

    ULONG size = 0;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size);
    if (!pImportDesc) return false;

    for (; pImportDesc->Name; pImportDesc++) {
        const char* szModName = (const char*)((BYTE*)hModule + pImportDesc->Name);
        if (_stricmp(szModName, dllName) == 0) {
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + pImportDesc->FirstThunk);
            DWORD origThunkAddr = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk;
            PIMAGE_THUNK_DATA pOriginalThunk = (PIMAGE_THUNK_DATA)((BYTE*)hModule + origThunkAddr);

            for (; pThunk->u1.Function; pThunk++, pOriginalThunk++) {
                if (IMAGE_SNAP_BY_ORDINAL(pOriginalThunk->u1.Ordinal)) continue;

                PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + pOriginalThunk->u1.AddressOfData);
                if (_stricmp((const char*)pImport->Name, funcName) == 0) {
                    DWORD oldProtect;
                    if (VirtualProtect(&pThunk->u1.Function, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect)) {
                        if (origFunc && !*origFunc) *origFunc = (void*)pThunk->u1.Function;
                        pThunk->u1.Function = (uintptr_t)hookFunc;
                        VirtualProtect(&pThunk->u1.Function, sizeof(uintptr_t), oldProtect, &oldProtect);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void DisplayPatcher::InstallIATHooks() {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) return;

    HookIAT(hExe, "user32.dll", "ChangeDisplaySettingsA", (void*)Hook_ChangeDisplaySettingsA, (void**)&g_pfnRealChangeDisplaySettingsA);
    HookIAT(hExe, "user32.dll", "CreateWindowExA", (void*)Hook_CreateWindowExA, (void**)&g_pfnRealCreateWindowExA);
    HookIAT(hExe, "user32.dll", "SetWindowPos", (void*)Hook_SetWindowPos, (void**)&g_pfnRealSetWindowPos);
    HookIAT(hExe, "user32.dll", "wsprintfA", (void*)Hook_wsprintfA, (void**)&g_pfnRealwsprintfA);
    HookIAT(hExe, "msvcrt.dll", "sprintf", (void*)Hook_sprintf, (void**)&g_pfnRealsprintf);
    HookIAT(hExe, "msvcrt.dll", "_snprintf", (void*)Hook_snprintf, (void**)&g_pfnRealsnprintf);
    HookIAT(hExe, "d3d9.dll", "Direct3DCreate9", (void*)Hook_Direct3DCreate9, (void**)&g_pfnRealDirect3DCreate9);
}

void DisplayPatcher::Initialize(const DisplayConfig& cfg) {
    g_config = cfg;
    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_lastFrameTime);

    if (!g_hooksInstalled) {
        g_hooksInstalled = true;
        InstallIATHooks();
    }
}

DisplayConfig DisplayPatcher::GetConfig() {
    return g_config;
}

void DisplayPatcher::LimitFrameRate() {
    if (g_config.targetFPS <= 0 || g_frequency.QuadPart == 0) return;

    double targetSecondsPerFrame = 1.0 / (double)g_config.targetFPS;
    LARGE_INTEGER currentTime;

    do {
        QueryPerformanceCounter(&currentTime);
        double elapsedSeconds = (double)(currentTime.QuadPart - g_lastFrameTime.QuadPart) / (double)g_frequency.QuadPart;
        if (elapsedSeconds >= targetSecondsPerFrame) break;
        
        double remainingSeconds = targetSecondsPerFrame - elapsedSeconds;
        if (remainingSeconds > 0.002) {
            Sleep(1);
        } else {
            Sleep(0);
        }
    } while (true);

    g_lastFrameTime = currentTime;
}

std::string DisplayPatcher::AnnotateResolutionString(const char* input) {
    if (!input) return "";

    int w = 0, h = 0;
    if (sscanf_s(input, "%d x %d", &w, &h) == 2 || sscanf_s(input, "%dx%d", &w, &h) == 2 || sscanf_s(input, "%d X %d", &w, &h) == 2) {
        if (w > 0 && h > 0) {
            float ratio = (float)w / (float)h;
            std::string tag = "";

            if (std::abs(ratio - (16.0f / 9.0f)) < 0.04f) {
                tag = " (16:9)";
            } else if (std::abs(ratio - (21.0f / 9.0f)) < 0.05f || std::abs(ratio - (64.0f / 27.0f)) < 0.05f) {
                tag = " (21:9 Ultrawide)";
            } else if (std::abs(ratio - (32.0f / 9.0f)) < 0.05f) {
                tag = " (32:9 Super Ultrawide)";
            } else if (std::abs(ratio - (16.0f / 10.0f)) < 0.04f) {
                tag = " (16:10)";
            } else if (std::abs(ratio - (4.0f / 3.0f)) < 0.04f) {
                tag = " (4:3)";
            } else if (std::abs(ratio - (5.0f / 4.0f)) < 0.04f) {
                tag = " (5:4)";
            }

            char buf[256];
            sprintf_s(buf, "%d x %d%s", w, h, tag.c_str());
            return std::string(buf);
        }
    }
    return std::string(input);
}

// ---------------------------------------------------------------------------
// ProxyIDirect3D9 Implementation
// ---------------------------------------------------------------------------

ProxyIDirect3D9::ProxyIDirect3D9(IDirect3D9* realD3D)
    : m_realD3D(realD3D), m_refCount(1) {}

ProxyIDirect3D9::~ProxyIDirect3D9() {
    if (m_realD3D) m_realD3D->Release();
}

STDMETHODIMP ProxyIDirect3D9::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3D9) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return m_realD3D->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) ProxyIDirect3D9::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ProxyIDirect3D9::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ProxyIDirect3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnDeviceInterface) {
    if (!m_realD3D || !pPresentationParameters) return D3DERR_INVALIDCALL;

    int gameW = pPresentationParameters->BackBufferWidth;
    int gameH = pPresentationParameters->BackBufferHeight;

    if (DisplayPatcher::GetConfig().enableBorderlessWindow) {
        pPresentationParameters->Windowed = TRUE;
        pPresentationParameters->FullScreen_RefreshRateInHz = 0;
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        pPresentationParameters->SwapEffect = D3DSWAPEFFECT_DISCARD;
        // Keep internal resolution 1:1 synchronized!
        if (gameW <= 0) gameW = GetSystemMetrics(SM_CXSCREEN);
        if (gameH <= 0) gameH = GetSystemMetrics(SM_CYSCREEN);
        pPresentationParameters->BackBufferWidth = gameW;
        pPresentationParameters->BackBufferHeight = gameH;
    }

    IDirect3DDevice9* realDev = nullptr;
    HRESULT hr = m_realD3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, &realDev);
    if (SUCCEEDED(hr) && realDev) {
        *ppReturnDeviceInterface = new ProxyIDirect3DDevice9(realDev, gameW, gameH);
        return D3D_OK;
    }
    return hr;
}

STDMETHODIMP ProxyIDirect3D9::RegisterSoftwareDevice(void* pInitializeFunction) { return m_realD3D->RegisterSoftwareDevice(pInitializeFunction); }
STDMETHODIMP_(UINT) ProxyIDirect3D9::GetAdapterCount() { return m_realD3D->GetAdapterCount(); }
STDMETHODIMP ProxyIDirect3D9::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) { return m_realD3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier); }
STDMETHODIMP_(UINT) ProxyIDirect3D9::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) { return m_realD3D->GetAdapterModeCount(Adapter, Format); }
STDMETHODIMP ProxyIDirect3D9::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) { return m_realD3D->EnumAdapterModes(Adapter, Format, Mode, pMode); }
STDMETHODIMP ProxyIDirect3D9::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) { return m_realD3D->GetAdapterDisplayMode(Adapter, pMode); }
STDMETHODIMP ProxyIDirect3D9::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) { return m_realD3D->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed); }
STDMETHODIMP ProxyIDirect3D9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) { return m_realD3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat); }
STDMETHODIMP ProxyIDirect3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) { return m_realD3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels); }
STDMETHODIMP ProxyIDirect3D9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT PhotoFormat, D3DFORMAT DepthFormat) { return m_realD3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, PhotoFormat, DepthFormat); }
STDMETHODIMP ProxyIDirect3D9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) { return m_realD3D->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat); }
STDMETHODIMP ProxyIDirect3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) { return m_realD3D->GetDeviceCaps(Adapter, DeviceType, pCaps); }
STDMETHODIMP_(HMONITOR) ProxyIDirect3D9::GetAdapterMonitor(UINT Adapter) { return m_realD3D->GetAdapterMonitor(Adapter); }

// ---------------------------------------------------------------------------
// ProxyIDirect3DDevice9 Implementation
// ---------------------------------------------------------------------------

ProxyIDirect3DDevice9::ProxyIDirect3DDevice9(IDirect3DDevice9* realDev, int gameW, int gameH)
    : m_realDevice(realDev), m_refCount(1), m_gameW(gameW), m_gameH(gameH) {}

ProxyIDirect3DDevice9::~ProxyIDirect3DDevice9() {
    if (m_realDevice) m_realDevice->Release();
}

STDMETHODIMP ProxyIDirect3DDevice9::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9) {
        *ppvObj = this;
        AddRef();
        return S_OK;
    }
    return m_realDevice->QueryInterface(riid, ppvObj);
}

STDMETHODIMP_(ULONG) ProxyIDirect3DDevice9::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ProxyIDirect3DDevice9::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) delete this;
    return ref;
}

STDMETHODIMP ProxyIDirect3DDevice9::SetViewport(const D3DVIEWPORT9* pViewport) {
    // The backbuffer is already the game's chosen resolution (1:1 native), so
    // we pass the viewport through unchanged. The D3D runtime's present blit
    // handles stretching to fill the borderless window — no manual cropping needed.
    return m_realDevice->SetViewport(pViewport);
}


STDMETHODIMP ProxyIDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    return m_realDevice->SetTransform(State, pMatrix);
}

STDMETHODIMP ProxyIDirect3DDevice9::Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    return m_realDevice->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

STDMETHODIMP ProxyIDirect3DDevice9::Present(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    DisplayPatcher::LimitFrameRate();
    return m_realDevice->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

STDMETHODIMP ProxyIDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (DisplayPatcher::GetConfig().enableBorderlessWindow && pPresentationParameters) {
        m_gameW = pPresentationParameters->BackBufferWidth;
        m_gameH = pPresentationParameters->BackBufferHeight;
        pPresentationParameters->Windowed = TRUE;
        pPresentationParameters->FullScreen_RefreshRateInHz = 0;
        pPresentationParameters->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        pPresentationParameters->SwapEffect = D3DSWAPEFFECT_DISCARD;
        if (m_gameW <= 0) m_gameW = GetSystemMetrics(SM_CXSCREEN);
        if (m_gameH <= 0) m_gameH = GetSystemMetrics(SM_CYSCREEN);
        pPresentationParameters->BackBufferWidth = m_gameW;
        pPresentationParameters->BackBufferHeight = m_gameH;
    }
    return m_realDevice->Reset(pPresentationParameters);
}

STDMETHODIMP ProxyIDirect3DDevice9::TestCooperativeLevel() { return m_realDevice->TestCooperativeLevel(); }
STDMETHODIMP_(UINT) ProxyIDirect3DDevice9::GetAvailableTextureMem() { return m_realDevice->GetAvailableTextureMem(); }
STDMETHODIMP ProxyIDirect3DDevice9::EvictManagedResources() { return m_realDevice->EvictManagedResources(); }
STDMETHODIMP ProxyIDirect3DDevice9::GetDirect3D(IDirect3D9** ppD3D9) { return m_realDevice->GetDirect3D(ppD3D9); }
STDMETHODIMP ProxyIDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps) { return m_realDevice->GetDeviceCaps(pCaps); }
STDMETHODIMP ProxyIDirect3DDevice9::GetDisplayMode(UINT iObjectIndex, D3DDISPLAYMODE* pMode) { return m_realDevice->GetDisplayMode(iObjectIndex, pMode); }
STDMETHODIMP ProxyIDirect3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) { return m_realDevice->GetCreationParameters(pParameters); }
STDMETHODIMP ProxyIDirect3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) { return m_realDevice->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap); }
STDMETHODIMP_(void) ProxyIDirect3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags) { m_realDevice->SetCursorPosition(X, Y, Flags); }
STDMETHODIMP_(BOOL) ProxyIDirect3DDevice9::ShowCursor(BOOL bShow) { return m_realDevice->ShowCursor(bShow); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) { return m_realDevice->CreateAdditionalSwapChain(pPresentationParameters, ppSwapChain); }
STDMETHODIMP ProxyIDirect3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** ppSwapChain) { return m_realDevice->GetSwapChain(iSwapChain, ppSwapChain); }
STDMETHODIMP_(UINT) ProxyIDirect3DDevice9::GetNumberOfSwapChains() { return m_realDevice->GetNumberOfSwapChains(); }
STDMETHODIMP ProxyIDirect3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE iType, IDirect3DSurface9** ppBackBuffer) { return m_realDevice->GetBackBuffer(iSwapChain, iBackBuffer, iType, ppBackBuffer); }
STDMETHODIMP ProxyIDirect3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) { return m_realDevice->GetRasterStatus(iSwapChain, pRasterStatus); }
STDMETHODIMP ProxyIDirect3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs) { return m_realDevice->SetDialogBoxMode(bEnableDialogs); }
STDMETHODIMP_(void) ProxyIDirect3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) { m_realDevice->SetGammaRamp(iSwapChain, Flags, pRamp); }
STDMETHODIMP_(void) ProxyIDirect3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) { m_realDevice->GetGammaRamp(iSwapChain, pRamp); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) { return m_realDevice->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) { return m_realDevice->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) { return m_realDevice->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) { return m_realDevice->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) { return m_realDevice->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_realDevice->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_realDevice->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::UpdateSurface(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, const POINT* pDestinationPoint) { return m_realDevice->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestinationPoint); }
STDMETHODIMP ProxyIDirect3DDevice9::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) { return m_realDevice->UpdateTexture(pSourceTexture, pDestinationTexture); }
STDMETHODIMP ProxyIDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) { return m_realDevice->GetRenderTargetData(pRenderTarget, pDestSurface); }
STDMETHODIMP ProxyIDirect3DDevice9::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) { return m_realDevice->GetFrontBufferData(iSwapChain, pDestSurface); }
STDMETHODIMP ProxyIDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) { return m_realDevice->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter); }
STDMETHODIMP ProxyIDirect3DDevice9::ColorFill(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color) { return m_realDevice->ColorFill(pSurface, pRect, color); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) { return m_realDevice->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle); }
STDMETHODIMP ProxyIDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) { return m_realDevice->SetRenderTarget(RenderTargetIndex, pRenderTarget); }
STDMETHODIMP ProxyIDirect3DDevice9::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) { return m_realDevice->GetRenderTarget(RenderTargetIndex, ppRenderTarget); }
STDMETHODIMP ProxyIDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) { return m_realDevice->SetDepthStencilSurface(pNewZStencil); }
STDMETHODIMP ProxyIDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) { return m_realDevice->GetDepthStencilSurface(ppZStencilSurface); }
STDMETHODIMP ProxyIDirect3DDevice9::BeginScene() { return m_realDevice->BeginScene(); }
STDMETHODIMP ProxyIDirect3DDevice9::EndScene() { return m_realDevice->EndScene(); }
STDMETHODIMP ProxyIDirect3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) { return m_realDevice->GetTransform(State, pMatrix); }
STDMETHODIMP ProxyIDirect3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) { return m_realDevice->MultiplyTransform(State, pMatrix); }
STDMETHODIMP ProxyIDirect3DDevice9::GetViewport(D3DVIEWPORT9* pViewport) { return m_realDevice->GetViewport(pViewport); }
STDMETHODIMP ProxyIDirect3DDevice9::SetMaterial(const D3DMATERIAL9* pMaterial) { return m_realDevice->SetMaterial(pMaterial); }
STDMETHODIMP ProxyIDirect3DDevice9::GetMaterial(D3DMATERIAL9* pMaterial) { return m_realDevice->GetMaterial(pMaterial); }
STDMETHODIMP ProxyIDirect3DDevice9::SetLight(DWORD Index, const D3DLIGHT9* pLight) { return m_realDevice->SetLight(Index, pLight); }
STDMETHODIMP ProxyIDirect3DDevice9::GetLight(DWORD Index, D3DLIGHT9* pLight) { return m_realDevice->GetLight(Index, pLight); }
STDMETHODIMP ProxyIDirect3DDevice9::LightEnable(DWORD Index, BOOL Enable) { return m_realDevice->LightEnable(Index, Enable); }
STDMETHODIMP ProxyIDirect3DDevice9::GetLightEnable(DWORD Index, BOOL* pEnable) { return m_realDevice->GetLightEnable(Index, pEnable); }
STDMETHODIMP ProxyIDirect3DDevice9::SetClipPlane(DWORD Index, const float* pPlane) { return m_realDevice->SetClipPlane(Index, pPlane); }
STDMETHODIMP ProxyIDirect3DDevice9::GetClipPlane(DWORD Index, float* pPlane) { return m_realDevice->GetClipPlane(Index, pPlane); }
STDMETHODIMP ProxyIDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) { return m_realDevice->SetRenderState(State, Value); }
STDMETHODIMP ProxyIDirect3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) { return m_realDevice->GetRenderState(State, pValue); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) { return m_realDevice->CreateStateBlock(Type, ppSB); }
STDMETHODIMP ProxyIDirect3DDevice9::BeginStateBlock() { return m_realDevice->BeginStateBlock(); }
STDMETHODIMP ProxyIDirect3DDevice9::EndStateBlock(IDirect3DStateBlock9** ppSB) { return m_realDevice->EndStateBlock(ppSB); }
STDMETHODIMP ProxyIDirect3DDevice9::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) { return m_realDevice->SetClipStatus(pClipStatus); }
STDMETHODIMP ProxyIDirect3DDevice9::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) { return m_realDevice->GetClipStatus(pClipStatus); }
STDMETHODIMP ProxyIDirect3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) { return m_realDevice->GetTexture(Stage, ppTexture); }
STDMETHODIMP ProxyIDirect3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) { return m_realDevice->SetTexture(Stage, pTexture); }
STDMETHODIMP ProxyIDirect3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) { return m_realDevice->GetTextureStageState(Stage, Type, pValue); }
STDMETHODIMP ProxyIDirect3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) { return m_realDevice->SetTextureStageState(Stage, Type, Value); }
STDMETHODIMP ProxyIDirect3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) { return m_realDevice->GetSamplerState(Sampler, Type, pValue); }
STDMETHODIMP ProxyIDirect3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) { return m_realDevice->SetSamplerState(Sampler, Type, Value); }
STDMETHODIMP ProxyIDirect3DDevice9::ValidateDevice(DWORD* pNumPasses) { return m_realDevice->ValidateDevice(pNumPasses); }
STDMETHODIMP ProxyIDirect3DDevice9::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) { return m_realDevice->SetPaletteEntries(PaletteNumber, pEntries); }
STDMETHODIMP ProxyIDirect3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) { return m_realDevice->GetPaletteEntries(PaletteNumber, pEntries); }
STDMETHODIMP ProxyIDirect3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber) { return m_realDevice->SetCurrentTexturePalette(PaletteNumber); }
STDMETHODIMP ProxyIDirect3DDevice9::GetCurrentTexturePalette(UINT* PaletteNumber) { return m_realDevice->GetCurrentTexturePalette(PaletteNumber); }
STDMETHODIMP ProxyIDirect3DDevice9::SetScissorRect(const RECT* pRect) { return m_realDevice->SetScissorRect(pRect); }
STDMETHODIMP ProxyIDirect3DDevice9::GetScissorRect(RECT* pRect) { return m_realDevice->GetScissorRect(pRect); }
STDMETHODIMP ProxyIDirect3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware) { return m_realDevice->SetSoftwareVertexProcessing(bSoftware); }
STDMETHODIMP_(BOOL) ProxyIDirect3DDevice9::GetSoftwareVertexProcessing() { return m_realDevice->GetSoftwareVertexProcessing(); }
STDMETHODIMP ProxyIDirect3DDevice9::SetNPatchMode(float nSegments) { return m_realDevice->SetNPatchMode(nSegments); }
STDMETHODIMP_(float) ProxyIDirect3DDevice9::GetNPatchMode() { return m_realDevice->GetNPatchMode(); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) { return m_realDevice->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT primitiveCount) { return m_realDevice->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, StartIndex, primitiveCount); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) { return m_realDevice->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) { return m_realDevice->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride); }



STDMETHODIMP ProxyIDirect3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) { return m_realDevice->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) { return m_realDevice->CreateVertexDeclaration(pVertexElements, ppDecl); }
STDMETHODIMP ProxyIDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) { return m_realDevice->SetVertexDeclaration(pDecl); }
STDMETHODIMP ProxyIDirect3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) { return m_realDevice->GetVertexDeclaration(ppDecl); }
STDMETHODIMP ProxyIDirect3DDevice9::SetFVF(DWORD FVF) { return m_realDevice->SetFVF(FVF); }
STDMETHODIMP ProxyIDirect3DDevice9::GetFVF(DWORD* pFVF) { return m_realDevice->GetFVF(pFVF); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) { return m_realDevice->CreateVertexShader(pFunction, ppShader); }
STDMETHODIMP ProxyIDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9* pShader) { return m_realDevice->SetVertexShader(pShader); }
STDMETHODIMP ProxyIDirect3DDevice9::GetVertexShader(IDirect3DVertexShader9** ppShader) { return m_realDevice->GetVertexShader(ppShader); }
STDMETHODIMP ProxyIDirect3DDevice9::SetVertexShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) { return m_realDevice->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return m_realDevice->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
STDMETHODIMP ProxyIDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) { return m_realDevice->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return m_realDevice->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
STDMETHODIMP ProxyIDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) { return m_realDevice->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) { return m_realDevice->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
STDMETHODIMP ProxyIDirect3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) { return m_realDevice->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride); }
STDMETHODIMP ProxyIDirect3DDevice9::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) { return m_realDevice->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride); }
STDMETHODIMP ProxyIDirect3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) { return m_realDevice->SetStreamSourceFreq(StreamNumber, Setting); }
STDMETHODIMP ProxyIDirect3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) { return m_realDevice->GetStreamSourceFreq(StreamNumber, pSetting); }
STDMETHODIMP ProxyIDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* pIndexData) { return m_realDevice->SetIndices(pIndexData); }
STDMETHODIMP ProxyIDirect3DDevice9::GetIndices(IDirect3DIndexBuffer9** ppIndexData) { return m_realDevice->GetIndices(ppIndexData); }
STDMETHODIMP ProxyIDirect3DDevice9::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) { return m_realDevice->CreatePixelShader(pFunction, ppShader); }
STDMETHODIMP ProxyIDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9* pShader) { return m_realDevice->SetPixelShader(pShader); }
STDMETHODIMP ProxyIDirect3DDevice9::GetPixelShader(IDirect3DPixelShader9** ppShader) { return m_realDevice->GetPixelShader(ppShader); }
STDMETHODIMP ProxyIDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) { return m_realDevice->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) { return m_realDevice->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
STDMETHODIMP ProxyIDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) { return m_realDevice->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) { return m_realDevice->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
STDMETHODIMP ProxyIDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) { return m_realDevice->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
STDMETHODIMP ProxyIDirect3DDevice9::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) { return m_realDevice->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawRectPatch(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) { return m_realDevice->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo); }
STDMETHODIMP ProxyIDirect3DDevice9::DrawTriPatch(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) { return m_realDevice->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo); }
STDMETHODIMP ProxyIDirect3DDevice9::DeletePatch(UINT Handle) { return m_realDevice->DeletePatch(Handle); }
STDMETHODIMP ProxyIDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) { return m_realDevice->CreateQuery(Type, ppQuery); }
