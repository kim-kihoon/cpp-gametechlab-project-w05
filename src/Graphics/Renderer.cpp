#include <Graphics/Renderer.h>

namespace Graphics
{
    URenderer::URenderer() {}
    URenderer::~URenderer() {}

    bool URenderer::Initialize(HWND InWindowHandle, int InWidth, int InHeight)
    {
        DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
        SwapChainDesc.BufferCount = 2;
        SwapChainDesc.BufferDesc.Width = InWidth;
        SwapChainDesc.BufferDesc.Height = InHeight;
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferDesc.RefreshRate.Numerator = 165;
        SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.OutputWindow = InWindowHandle;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.SampleDesc.Quality = 0;

#ifdef _DEBUG
        SwapChainDesc.Windowed = TRUE;
#else
        SwapChainDesc.Windowed = FALSE;
#endif

        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        UINT CreateDeviceFlags = 0;
#ifdef _DEBUG
        CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL FeatureLevel;

        HRESULT Result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            CreateDeviceFlags,
            FeatureLevels,
            2,
            D3D11_SDK_VERSION,
            &SwapChainDesc,
            &SwapChain,
            &Device,
            &FeatureLevel,
            &Context);

        if (FAILED(Result))
        {
            return false;
        }

        ComPtr<ID3D11Texture2D> BackBuffer;
        SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
        Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView);

        return true;
    }

    void URenderer::BeginFrame()
    {
        const float ClearColor[4] = { 0.02f, 0.02f, 0.1f, 1.00f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), nullptr);
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), ClearColor);
    }

    void URenderer::EndFrame()
    {
        SwapChain->Present(0, 0);
    }

    void URenderer::SetDebugRenderSettings(const FDebugRenderSettings& InSettings)
    {
        DebugRenderSettings = InSettings;
    }
}
