#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <Windowsx.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "app/app_state.h"

using namespace Microsoft::WRL;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct SelectionRect {
    float left = 0.0f;
    float top = 0.0f;
    float right = 1.0f;
    float bottom = 1.0f;
};

enum class DragMode {
    None,
    Create,
    Move,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
};

struct SceneConstants {
    float uvMin[2];
    float uvSize[2];
    float viewOffset[2];
    float viewZoom;
    float _pad0;
};

struct OverlayConstants {
    float selectionRect[4];
    float borderThickness;
    float dimAlpha;
    float _pad[2];
};

HWND g_hwnd = nullptr;
ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGIOutputDuplication> g_duplication;

ComPtr<IDXGISwapChain> g_swapChain;
ComPtr<ID3D11RenderTargetView> g_rtv;
ComPtr<ID3D11SamplerState> g_sampler;
ComPtr<ID3D11VertexShader> g_sceneVS;
ComPtr<ID3D11PixelShader> g_scenePS;
ComPtr<ID3D11PixelShader> g_overlayPS;
ComPtr<ID3D11Buffer> g_sceneConstantBuffer;
ComPtr<ID3D11Buffer> g_overlayConstantBuffer;
ComPtr<ID3D11Texture2D> g_frameTexture;
ComPtr<ID3D11ShaderResourceView> g_frameSRV;
ComPtr<ID3D11BlendState> g_alphaBlend;

UINT g_desktopWidth = 1;
UINT g_desktopHeight = 1;

float g_offsetX = 0.0f;
float g_offsetY = 0.0f;
float g_zoom = 1.0f;

SelectionRect g_selection;
bool g_hasSelection = false;
DragMode g_dragMode = DragMode::None;
POINT g_dragStart = {};
SelectionRect g_dragStartSelection;
app::AppState g_appState;
SelectionRect g_confirmedViewport;
bool g_viewportConfirmed = false;
RECT g_monitorRect = { 0, 0, 1, 1 };
HWND g_confirmButton = nullptr;
HWND g_resetButton = nullptr;
HWND g_toggleInstructionsButton = nullptr;
HWND g_instructionsLabel = nullptr;
bool g_instructionsVisible = true;

constexpr int kConfirmButtonId = 1001;
constexpr int kResetButtonId = 1002;
constexpr int kInstructionsButtonId = 1003;

void NormalizeSelection(SelectionRect& sel);

HMENU MenuIdToHmenu(int id) {
    return reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id));
}

void ApplySettingsToRuntime() {
    g_offsetX = g_appState.settings.viewportOffsetX;
    g_offsetY = g_appState.settings.viewportOffsetY;
    g_zoom = std::clamp(g_appState.settings.zoom, 0.2f, 4.0f);
}

void LayoutUi() {
    if (!g_hwnd || !g_confirmButton) return;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    const int panelW = 360;
    const int buttonW = 100;
    const int top = 18;
    const int left = 18;

    MoveWindow(g_confirmButton, left, top, buttonW, 32, TRUE);
    MoveWindow(g_resetButton, left + buttonW + 8, top, buttonW, 32, TRUE);
    MoveWindow(g_toggleInstructionsButton, left + (buttonW + 8) * 2, top, buttonW + 30, 32, TRUE);

    const int labelTop = top + 44;
    MoveWindow(g_instructionsLabel, left, labelTop, panelW, std::max(120, static_cast<int>(rc.bottom - labelTop - 16)), TRUE);
    ShowWindow(g_instructionsLabel, g_instructionsVisible ? SW_SHOW : SW_HIDE);
}

void ConfirmSelection() {
    SelectionRect chosen = g_hasSelection ? g_selection : SelectionRect{};
    NormalizeSelection(chosen);
    g_confirmedViewport = chosen;
    g_viewportConfirmed = true;
}

void ResetSelection() {
    g_hasSelection = false;
    g_selection = SelectionRect{};
    g_confirmedViewport = SelectionRect{};
    g_viewportConfirmed = false;
    g_offsetX = 0.0f;
    g_offsetY = 0.0f;
    g_zoom = 1.0f;
}

void ToggleSettingsWindow(HINSTANCE) {
    // Placeholder until a dedicated settings UI is implemented.
    // Re-apply persisted settings and save to ensure any runtime updates are kept.
    ApplySettingsToRuntime();
    g_appState.settings.viewportOffsetX = g_offsetX;
    g_appState.settings.viewportOffsetY = g_offsetY;
    g_appState.settings.zoom = g_zoom;
    g_appState.Save();
}

float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

void NormalizeSelection(SelectionRect& sel) {
    if (sel.left > sel.right) std::swap(sel.left, sel.right);
    if (sel.top > sel.bottom) std::swap(sel.top, sel.bottom);

    sel.left = Clamp01(sel.left);
    sel.right = Clamp01(sel.right);
    sel.top = Clamp01(sel.top);
    sel.bottom = Clamp01(sel.bottom);

    const float minSize = 0.01f;
    if ((sel.right - sel.left) < minSize) {
        sel.right = std::min(1.0f, sel.left + minSize);
    }
    if ((sel.bottom - sel.top) < minSize) {
        sel.bottom = std::min(1.0f, sel.top + minSize);
    }
}

float PixelToNormX(int x) {
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    return rc.right > 0 ? Clamp01(static_cast<float>(x) / static_cast<float>(rc.right)) : 0.0f;
}

float PixelToNormY(int y) {
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    return rc.bottom > 0 ? Clamp01(static_cast<float>(y) / static_cast<float>(rc.bottom)) : 0.0f;
}

DragMode HitTestSelection(int x, int y) {
    if (!g_hasSelection) return DragMode::None;

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    const float width = static_cast<float>(rc.right);
    const float height = static_cast<float>(rc.bottom);

    float l = g_selection.left * width;
    float r = g_selection.right * width;
    float t = g_selection.top * height;
    float b = g_selection.bottom * height;

    const float handle = 10.0f;
    bool nearL = std::abs(x - l) <= handle;
    bool nearR = std::abs(x - r) <= handle;
    bool nearT = std::abs(y - t) <= handle;
    bool nearB = std::abs(y - b) <= handle;

    if (nearL && nearT) return DragMode::ResizeTopLeft;
    if (nearR && nearT) return DragMode::ResizeTopRight;
    if (nearL && nearB) return DragMode::ResizeBottomLeft;
    if (nearR && nearB) return DragMode::ResizeBottomRight;
    if (nearL && y >= t && y <= b) return DragMode::ResizeLeft;
    if (nearR && y >= t && y <= b) return DragMode::ResizeRight;
    if (nearT && x >= l && x <= r) return DragMode::ResizeTop;
    if (nearB && x >= l && x <= r) return DragMode::ResizeBottom;
    if (x >= l && x <= r && y >= t && y <= b) return DragMode::Move;

    return DragMode::None;
}

void UpdateDrag(int x, int y) {
    float nx = PixelToNormX(x);
    float ny = PixelToNormY(y);
    float sx = PixelToNormX(g_dragStart.x);
    float sy = PixelToNormY(g_dragStart.y);

    SelectionRect next = g_dragStartSelection;
    switch (g_dragMode) {
        case DragMode::Create:
            next.left = sx;
            next.top = sy;
            next.right = nx;
            next.bottom = ny;
            g_hasSelection = true;
            break;
        case DragMode::Move: {
            float dx = nx - sx;
            float dy = ny - sy;
            float w = g_dragStartSelection.right - g_dragStartSelection.left;
            float h = g_dragStartSelection.bottom - g_dragStartSelection.top;
            next.left = Clamp01(g_dragStartSelection.left + dx);
            next.top = Clamp01(g_dragStartSelection.top + dy);
            next.right = next.left + w;
            next.bottom = next.top + h;
            if (next.right > 1.0f) {
                next.right = 1.0f;
                next.left = next.right - w;
            }
            if (next.bottom > 1.0f) {
                next.bottom = 1.0f;
                next.top = next.bottom - h;
            }
            break;
        }
        case DragMode::ResizeLeft:
        case DragMode::ResizeTopLeft:
        case DragMode::ResizeBottomLeft:
            next.left = nx;
            break;
        default:
            break;
    }

    switch (g_dragMode) {
        case DragMode::ResizeRight:
        case DragMode::ResizeTopRight:
        case DragMode::ResizeBottomRight:
            next.right = nx;
            break;
        default:
            break;
    }

    switch (g_dragMode) {
        case DragMode::ResizeTop:
        case DragMode::ResizeTopLeft:
        case DragMode::ResizeTopRight:
            next.top = ny;
            break;
        default:
            break;
    }

    switch (g_dragMode) {
        case DragMode::ResizeBottom:
        case DragMode::ResizeBottomLeft:
        case DragMode::ResizeBottomRight:
            next.bottom = ny;
            break;
        default:
            break;
    }

    NormalizeSelection(next);
    g_selection = next;
}

void CreateSwapChainResources(UINT width, UINT height) {
    if (!g_swapChain) return;

    g_rtv.Reset();
    g_context->OMSetRenderTargets(0, nullptr, nullptr);
    g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    ComPtr<ID3D11Texture2D> backBuffer;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &g_rtv);
}

void InitPipeline() {
    static const char* kVS = R"(
struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOut main(uint vid : SV_VertexID) {
    VSOut o;
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    o.pos = float4(pos[vid], 0.0, 1.0);
    o.uv = 0.5 * (pos[vid] + 1.0);
    return o;
}
)";

    static const char* kScenePS = R"(
Texture2D srcTex : register(t0);
SamplerState sampLinear : register(s0);
cbuffer SceneCB : register(b0) {
    float2 uvMin;
    float2 uvSize;
    float2 viewOffset;
    float viewZoom;
    float _pad0;
};
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float2 adjustedUv = float2(1.0 - uv.x, uv.y);
    float2 centered = (adjustedUv - 0.5) / max(viewZoom, 0.05) + 0.5 + viewOffset;
    centered = saturate(centered);
    float2 srcUv = uvMin + centered * uvSize;
    return srcTex.Sample(sampLinear, srcUv);
}
)";

    static const char* kOverlayPS = R"(
cbuffer OverlayCB : register(b1) {
    float4 selectionRect;
    float borderThickness;
    float dimAlpha;
    float2 _pad;
};
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET {
    float inRect = step(selectionRect.x, uv.x) * step(uv.x, selectionRect.z) * step(selectionRect.y, uv.y) * step(uv.y, selectionRect.w);
    float vignette = saturate(distance(uv, float2(0.5, 0.5)) * 1.4);
    float3 tint = lerp(float3(0.06, 0.07, 0.12), float3(0.01, 0.01, 0.02), vignette);
    float4 color = float4(tint, (1.0 - inRect) * dimAlpha);

    float leftEdge = abs(uv.x - selectionRect.x) < borderThickness;
    float rightEdge = abs(uv.x - selectionRect.z) < borderThickness;
    float topEdge = abs(uv.y - selectionRect.y) < borderThickness;
    float botEdge = abs(uv.y - selectionRect.w) < borderThickness;

    float onVert = (leftEdge || rightEdge) && uv.y >= selectionRect.y && uv.y <= selectionRect.w;
    float onHorz = (topEdge || botEdge) && uv.x >= selectionRect.x && uv.x <= selectionRect.z;
    if (onVert || onHorz) {
        color = float4(0.1, 0.8, 1.0, 0.95);
    }
    return color;
}
)";

    ComPtr<ID3DBlob> vsBlob, psBlob, overlayBlob, errBlob;
    D3DCompile(kVS, strlen(kVS), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    D3DCompile(kScenePS, strlen(kScenePS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errBlob);
    D3DCompile(kOverlayPS, strlen(kOverlayPS), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &overlayBlob, &errBlob);

    g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_sceneVS);
    g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_scenePS);
    g_device->CreatePixelShader(overlayBlob->GetBufferPointer(), overlayBlob->GetBufferSize(), nullptr, &g_overlayPS);

    D3D11_BUFFER_DESC cbd{};
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.ByteWidth = sizeof(SceneConstants);
    g_device->CreateBuffer(&cbd, nullptr, &g_sceneConstantBuffer);

    cbd.ByteWidth = sizeof(OverlayConstants);
    g_device->CreateBuffer(&cbd, nullptr, &g_overlayConstantBuffer);

    D3D11_SAMPLER_DESC samp{};
    samp.Filter = D3D11_FILTER_ANISOTROPIC;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.MaxAnisotropy = 8;
    samp.MaxLOD = D3D11_FLOAT32_MAX;
    g_device->CreateSamplerState(&samp, &g_sampler);

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_device->CreateBlendState(&blend, &g_alphaBlend);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (g_swapChain) {
                CreateSwapChainResources(LOWORD(lParam), HIWORD(lParam));
            }
            LayoutUi();
            return 0;
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            g_zoom = std::clamp(g_zoom + (delta > 0 ? 0.1f : -0.1f), 0.2f, 4.0f);
            return 0;
        }
        case WM_MBUTTONDOWN:
            SetCapture(hwnd);
            g_dragMode = DragMode::Move;
            g_dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            return 0;
        case WM_MBUTTONUP:
            ReleaseCapture();
            g_dragMode = DragMode::None;
            return 0;
        case WM_RBUTTONDOWN:
            g_offsetX = g_offsetY = 0.0f;
            g_zoom = 1.0f;
            return 0;
        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_dragStart = { x, y };
            g_dragMode = HitTestSelection(x, y);
            g_dragStartSelection = g_selection;
            if (g_dragMode == DragMode::None) {
                g_dragMode = DragMode::Create;
                float nx = PixelToNormX(x);
                float ny = PixelToNormY(y);
                g_selection = { nx, ny, nx, ny };
                g_dragStartSelection = g_selection;
                g_hasSelection = true;
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if ((wParam & MK_LBUTTON) && g_dragMode != DragMode::None) {
                UpdateDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            if ((wParam & MK_MBUTTON) && g_dragMode == DragMode::Move) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                float dx = static_cast<float>(GET_X_LPARAM(lParam) - g_dragStart.x) / std::max(1L, rc.right);
                float dy = static_cast<float>(GET_Y_LPARAM(lParam) - g_dragStart.y) / std::max(1L, rc.bottom);
                g_offsetX = std::clamp(g_offsetX + dx, -1.0f, 1.0f);
                g_offsetY = std::clamp(g_offsetY + dy, -1.0f, 1.0f);
                g_dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            }
            return 0;
        case WM_LBUTTONUP:
            if (g_dragMode != DragMode::None) {
                UpdateDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            g_dragMode = DragMode::None;
            ReleaseCapture();
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            } else if (wParam == VK_RETURN) {
                ConfirmSelection();
            } else if (wParam == 'S') {
                ToggleSettingsWindow(reinterpret_cast<HINSTANCE>(GetWindowLongPtr(hwnd, GWLP_HINSTANCE)));
            } else if (wParam == 'I') {
                g_instructionsVisible = !g_instructionsVisible;
                LayoutUi();
            }
            return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == BN_CLICKED) {
                switch (LOWORD(wParam)) {
                    case kConfirmButtonId:
                        ConfirmSelection();
                        return 0;
                    case kResetButtonId:
                        ResetSelection();
                        return 0;
                    case kInstructionsButtonId:
                        g_instructionsVisible = !g_instructionsVisible;
                        LayoutUi();
                        return 0;
                    default:
                        break;
                }
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void InitWindow(HINSTANCE hInstance) {
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ScreenResizer";
    RegisterClassW(&wc);

    g_monitorRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED,
        L"ScreenResizer", L"Virtual Viewport", WS_POPUP,
        g_monitorRect.left, g_monitorRect.top,
        g_monitorRect.right - g_monitorRect.left,
        g_monitorRect.bottom - g_monitorRect.top,
        nullptr, nullptr, hInstance, nullptr);

    g_confirmButton = CreateWindowW(L"BUTTON", L"Confirm (Enter)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        18, 18, 100, 32, g_hwnd, MenuIdToHmenu(kConfirmButtonId), hInstance, nullptr);
    g_resetButton = CreateWindowW(L"BUTTON", L"Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        126, 18, 100, 32, g_hwnd, MenuIdToHmenu(kResetButtonId), hInstance, nullptr);
    g_toggleInstructionsButton = CreateWindowW(L"BUTTON", L"Instructions",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        234, 18, 130, 32, g_hwnd, MenuIdToHmenu(kInstructionsButtonId), hInstance, nullptr);

    std::wstring instructions =
        L"Screen Resizer Controls\r\n"
        L"- Drag left mouse to create/adjust area\r\n"
        L"- Mouse wheel: zoom\r\n"
        L"- Middle mouse drag: pan\r\n"
        L"- Enter or Confirm: apply selected viewport\r\n"
        L"- I: toggle this instructions panel\r\n"
        L"- Reset: return to full-screen desktop\r\n"
        L"- Esc: exit";
    g_instructionsLabel = CreateWindowW(L"STATIC", instructions.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        18, 62, 360, 200, g_hwnd, nullptr, hInstance, nullptr);

    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    LayoutUi();
    ShowWindow(g_hwnd, SW_SHOW);
}

void InitD3D() {
    D3D_FEATURE_LEVEL featureLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &g_device, &featureLevel, &g_context);

    ComPtr<IDXGIDevice> dxgiDevice;
    g_device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIOutput> output;
    adapter->EnumOutputs(0, &output);
    ComPtr<IDXGIOutput1> output1;
    output.As(&output1);

    output1->DuplicateOutput(g_device.Get(), &g_duplication);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGIFactory> factory;
    adapter->GetParent(__uuidof(IDXGIFactory), &factory);
    factory->CreateSwapChain(g_device.Get(), &scd, &g_swapChain);

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    CreateSwapChainResources(rc.right, rc.bottom);
    InitPipeline();
}

void EnsureFrameResources(ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC srcDesc{};
    source->GetDesc(&srcDesc);

    if (!g_frameTexture || srcDesc.Width != g_desktopWidth || srcDesc.Height != g_desktopHeight) {
        g_frameTexture.Reset();
        g_frameSRV.Reset();

        g_desktopWidth = srcDesc.Width;
        g_desktopHeight = srcDesc.Height;

        D3D11_TEXTURE2D_DESC texDesc = srcDesc;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;
        g_device->CreateTexture2D(&texDesc, nullptr, &g_frameTexture);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        g_device->CreateShaderResourceView(g_frameTexture.Get(), &srvDesc, &g_frameSRV);
    }
}

void CaptureAndRender() {
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    ComPtr<IDXGIResource> resource;

    HRESULT hr = g_duplication->AcquireNextFrame(16, &frameInfo, &resource);
    if (FAILED(hr)) return;

    ComPtr<ID3D11Texture2D> frame;
    resource.As(&frame);
    EnsureFrameResources(frame.Get());
    g_context->CopyResource(g_frameTexture.Get(), frame.Get());

    float clear[4] = { 0, 0, 0, 1 };
    g_context->ClearRenderTargetView(g_rtv.Get(), clear);
    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);

    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(std::max(1L, rc.right));
    vp.Height = static_cast<float>(std::max(1L, rc.bottom));
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &vp);

    SelectionRect selected = g_viewportConfirmed ? g_confirmedViewport : SelectionRect{};
    NormalizeSelection(selected);

    SceneConstants scene{};
    scene.uvMin[0] = selected.left;
    scene.uvMin[1] = selected.top;
    scene.uvSize[0] = selected.right - selected.left;
    scene.uvSize[1] = selected.bottom - selected.top;
    scene.viewOffset[0] = g_offsetX;
    scene.viewOffset[1] = g_offsetY;
    scene.viewZoom = g_zoom;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    g_context->Map(g_sceneConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &scene, sizeof(scene));
    g_context->Unmap(g_sceneConstantBuffer.Get(), 0);

    g_context->VSSetShader(g_sceneVS.Get(), nullptr, 0);
    g_context->PSSetShader(g_scenePS.Get(), nullptr, 0);
    g_context->PSSetShaderResources(0, 1, g_frameSRV.GetAddressOf());
    g_context->PSSetSamplers(0, 1, g_sampler.GetAddressOf());
    g_context->PSSetConstantBuffers(0, 1, g_sceneConstantBuffer.GetAddressOf());
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->Draw(3, 0);

    OverlayConstants overlay{};
    SelectionRect editingRect = g_hasSelection ? g_selection : SelectionRect{};
    NormalizeSelection(editingRect);
    overlay.selectionRect[0] = editingRect.left;
    overlay.selectionRect[1] = editingRect.top;
    overlay.selectionRect[2] = editingRect.right;
    overlay.selectionRect[3] = editingRect.bottom;
    overlay.borderThickness = 2.0f / std::max(1.0f, vp.Width);
    overlay.dimAlpha = g_viewportConfirmed ? 0.0f : 0.52f;

    g_context->Map(g_overlayConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &overlay, sizeof(overlay));
    g_context->Unmap(g_overlayConstantBuffer.Get(), 0);

    float blendFactor[4] = { 0, 0, 0, 0 };
    g_context->OMSetBlendState(g_alphaBlend.Get(), blendFactor, 0xFFFFFFFF);
    g_context->PSSetShader(g_overlayPS.Get(), nullptr, 0);
    g_context->PSSetConstantBuffers(1, 1, g_overlayConstantBuffer.GetAddressOf());
    ID3D11ShaderResourceView* nullSrv = nullptr;
    g_context->PSSetShaderResources(0, 1, &nullSrv);
    g_context->Draw(3, 0);
    g_context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);

    g_swapChain->Present(1, 0);
    g_duplication->ReleaseFrame();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_appState.Initialize();
    InitWindow(hInstance);
    ApplySettingsToRuntime();
    InitD3D();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            CaptureAndRender();
        }
    }

    return 0;
}
