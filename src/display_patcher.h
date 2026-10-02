#ifndef DISPLAY_PATCHER_H
#define DISPLAY_PATCHER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <dbghelp.h>
#include <string>

struct DisplayConfig {
    bool enableBorderlessWindow = true;
    bool showAspectRatioInMenu = true;
    int targetFPS = 60;
};

class DisplayPatcher {
public:
    static void Initialize(const DisplayConfig& cfg);
    static std::string AnnotateResolutionString(const char* input);
    static DisplayConfig GetConfig();
    static void LimitFrameRate();

private:
    static DisplayConfig g_config;
    static HWND g_hGameWnd;
    static bool g_hooksInstalled;

    static LARGE_INTEGER g_frequency;
    static LARGE_INTEGER g_lastFrameTime;

    static void InstallIATHooks();
};

class ProxyIDirect3DDevice9 : public IDirect3DDevice9 {
private:
    IDirect3DDevice9* m_realDevice;
    ULONG m_refCount;
    int m_gameW;
    int m_gameH;

public:
    ProxyIDirect3DDevice9(IDirect3DDevice9* realDev, int gameW, int gameH);
    virtual ~ProxyIDirect3DDevice9();

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    STDMETHOD(TestCooperativeLevel)() override;
    STDMETHOD_(UINT, GetAvailableTextureMem)() override;
    STDMETHOD(EvictManagedResources)() override;
    STDMETHOD(GetDirect3D)(IDirect3D9** ppD3D9) override;
    STDMETHOD(GetDeviceCaps)(D3DCAPS9* pCaps) override;
    STDMETHOD(GetDisplayMode)(UINT iObjectIndex, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS *pParameters) override;
    STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) override;
    STDMETHOD_(void, SetCursorPosition)(int X, int Y, DWORD Flags) override;
    STDMETHOD_(BOOL, ShowCursor)(BOOL bShow) override;
    STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) override;
    STDMETHOD(GetSwapChain)(UINT iSwapChain, IDirect3DSwapChain9** ppSwapChain) override;
    STDMETHOD_(UINT, GetNumberOfSwapChains)() override;
    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) override;
    STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) override;
    STDMETHOD(GetBackBuffer)(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE iType, IDirect3DSurface9** ppBackBuffer) override;
    STDMETHOD(GetRasterStatus)(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override;
    STDMETHOD(SetDialogBoxMode)(BOOL bEnableDialogs) override;
    STDMETHOD_(void, SetGammaRamp)(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) override;
    STDMETHOD_(void, GetGammaRamp)(UINT iSwapChain, D3DGAMMARAMP* pRamp) override;
    STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    STDMETHOD(UpdateSurface)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, const POINT* pDestinationPoint) override;
    STDMETHOD(UpdateTexture)(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) override;
    STDMETHOD(GetRenderTargetData)(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) override;
    STDMETHOD(GetFrontBufferData)(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override;
    STDMETHOD(StretchRect)(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect, IDirect3DSurface9* pDestSurface, const RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
    STDMETHOD(ColorFill)(IDirect3DSurface9* pSurface, const RECT* pRect, D3DCOLOR color) override;
    STDMETHOD(CreateOffscreenPlainSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    STDMETHOD(SetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override;
    STDMETHOD(GetRenderTarget)(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) override;
    STDMETHOD(SetDepthStencilSurface)(IDirect3DSurface9* pNewZStencil) override;
    STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface9** ppZStencilSurface) override;
    STDMETHOD(BeginScene)() override;
    STDMETHOD(EndScene)() override;
    STDMETHOD(Clear)(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override;
    STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override;
    STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override;
    STDMETHOD(SetViewport)(const D3DVIEWPORT9* pViewport) override;
    STDMETHOD(GetViewport)(D3DVIEWPORT9* pViewport) override;
    STDMETHOD(SetMaterial)(const D3DMATERIAL9* pMaterial) override;
    STDMETHOD(GetMaterial)(D3DMATERIAL9* pMaterial) override;
    STDMETHOD(SetLight)(DWORD Index, const D3DLIGHT9* pLight) override;
    STDMETHOD(GetLight)(DWORD Index, D3DLIGHT9* pLight) override;
    STDMETHOD(LightEnable)(DWORD Index, BOOL Enable) override;
    STDMETHOD(GetLightEnable)(DWORD Index, BOOL* pEnable) override;
    STDMETHOD(SetClipPlane)(DWORD Index, const float* pPlane) override;
    STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane) override;
    STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value) override;
    STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue) override;
    STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override;
    STDMETHOD(BeginStateBlock)() override;
    STDMETHOD(EndStateBlock)(IDirect3DStateBlock9** ppSB) override;
    STDMETHOD(SetClipStatus)(const D3DCLIPSTATUS9* pClipStatus) override;
    STDMETHOD(GetClipStatus)(D3DCLIPSTATUS9* pClipStatus) override;
    STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override;
    STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture9* pTexture) override;
    STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override;
    STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
    STDMETHOD(GetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override;
    STDMETHOD(SetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override;
    STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override;
    STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, const PALETTEENTRY* pEntries) override;
    STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries) override;
    STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber) override;
    STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override;
    STDMETHOD(SetScissorRect)(const RECT* pRect) override;
    STDMETHOD(GetScissorRect)(RECT* pRect) override;
    STDMETHOD(SetSoftwareVertexProcessing)(BOOL bSoftware) override;
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)() override;
    STDMETHOD(SetNPatchMode)(float nSegments) override;
    STDMETHOD_(float, GetNPatchMode)() override;
    STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
    STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT primitiveCount) override;
    STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) override;
    STDMETHOD(CreateVertexDeclaration)(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) override;
    STDMETHOD(SetVertexDeclaration)(IDirect3DVertexDeclaration9* pDecl) override;
    STDMETHOD(GetVertexDeclaration)(IDirect3DVertexDeclaration9** ppDecl) override;
    STDMETHOD(SetFVF)(DWORD FVF) override;
    STDMETHOD(GetFVF)(DWORD* pFVF) override;
    STDMETHOD(CreateVertexShader)(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) override;
    STDMETHOD(SetVertexShader)(IDirect3DVertexShader9* pShader) override;
    STDMETHOD(GetVertexShader)(IDirect3DVertexShader9** ppShader) override;
    STDMETHOD(SetVertexShaderConstantF)(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) override;
    STDMETHOD(GetVertexShaderConstantF)(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override;
    STDMETHOD(SetVertexShaderConstantI)(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) override;
    STDMETHOD(GetVertexShaderConstantI)(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override;
    STDMETHOD(SetVertexShaderConstantB)(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) override;
    STDMETHOD(GetVertexShaderConstantB)(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override;
    STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) override;
    STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) override;
    STDMETHOD(SetStreamSourceFreq)(UINT StreamNumber, UINT Setting) override;
    STDMETHOD(GetStreamSourceFreq)(UINT StreamNumber, UINT* pSetting) override;
    STDMETHOD(SetIndices)(IDirect3DIndexBuffer9* pIndexData) override;
    STDMETHOD(GetIndices)(IDirect3DIndexBuffer9** ppIndexData) override;
    STDMETHOD(CreatePixelShader)(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) override;
    STDMETHOD(SetPixelShader)(IDirect3DPixelShader9* pShader) override;
    STDMETHOD(GetPixelShader)(IDirect3DPixelShader9** ppShader) override;
    STDMETHOD(SetPixelShaderConstantF)(UINT StartRegister, const float* pConstantData, UINT Vector4fCount) override;
    STDMETHOD(GetPixelShaderConstantF)(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override;
    STDMETHOD(SetPixelShaderConstantI)(UINT StartRegister, const int* pConstantData, UINT Vector4iCount) override;
    STDMETHOD(GetPixelShaderConstantI)(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override;
    STDMETHOD(SetPixelShaderConstantB)(UINT StartRegister, const BOOL* pConstantData, UINT BoolCount) override;
    STDMETHOD(GetPixelShaderConstantB)(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override;
    STDMETHOD(DrawRectPatch)(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) override;
    STDMETHOD(DrawTriPatch)(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) override;
    STDMETHOD(DeletePatch)(UINT Handle) override;
    STDMETHOD(CreateQuery)(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) override;
};

class ProxyIDirect3D9 : public IDirect3D9 {
private:
    IDirect3D9* m_realD3D;
    ULONG m_refCount;

public:
    ProxyIDirect3D9(IDirect3D9* realD3D);
    virtual ~ProxyIDirect3D9();

    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override;
    STDMETHOD_(UINT, GetAdapterCount)() override;
    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) override;
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter, D3DFORMAT Format) override;
    STDMETHOD(EnumAdapterModes)(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override;
    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;
    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override;
    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT PhotoFormat, D3DFORMAT DepthFormat) override;
    STDMETHOD(CheckDeviceFormatConversion)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override;
    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override;
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) override;
    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnDeviceInterface) override;
};

#endif // DISPLAY_PATCHER_H
