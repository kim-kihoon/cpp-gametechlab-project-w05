#include <Graphics/Renderer.h>

namespace ExtremeGraphics
{
    Renderer::Renderer() {}
    Renderer::~Renderer() {}

    bool Renderer::Initialize(HWND hWnd, int width, int height)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 165; // 165Hz 타겟
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT createDeviceFlags = 0;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
        
        HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);

        if (FAILED(hr)) return false;

        // Render Target View 생성
        ComPtr<ID3D11Texture2D> pBackBuffer;
        m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        m_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pMainRenderTargetView);

        return true;
    }

    void Renderer::BeginFrame()
    {
        // 화면 지우기 (레드불 감청색 느낌)
        const float clear_color[4] = { 0.02f, 0.02f, 0.1f, 1.00f };
        m_pd3dDeviceContext->OMSetRenderTargets(1, m_pMainRenderTargetView.GetAddressOf(), nullptr);
        m_pd3dDeviceContext->ClearRenderTargetView(m_pMainRenderTargetView.Get(), clear_color);
    }

    void Renderer::EndFrame()
    {
        m_pSwapChain->Present(1, 0); // V-Sync On
    }
}