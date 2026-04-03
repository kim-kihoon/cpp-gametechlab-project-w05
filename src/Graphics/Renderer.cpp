#include <Graphics/Renderer.h>
#include <Math/MathTypes.h>
#include <cstring> // memcpy 사용

namespace Graphics
{
    URenderer::URenderer() : CurrentBufferOffset(0) {}
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
        
        HRESULT Result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags, 
            FeatureLevels, 2, D3D11_SDK_VERSION, &SwapChainDesc, &SwapChain, &Device, &FeatureLevel, &Context);

        if (FAILED(Result)) return false;

        Result = Context.As(&Context1);
        if (FAILED(Result)) return false;

        ComPtr<ID3D11Texture2D> BackBuffer;
        SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
        Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView);

        if (!CreateCircularBuffer()) return false;

        return true;
    }

    bool URenderer::CreateCircularBuffer()
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = TOTAL_BUFFER_INSTANCES * sizeof(Math::FPacked3x4Matrix);
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT Result = Device->CreateBuffer(&Desc, nullptr, &ConstantBuffer);
        return SUCCEEDED(Result);
    }

    void URenderer::BeginFrame()
    {
        const float ClearColor[4] = { 0.02f, 0.02f, 0.1f, 1.00f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), nullptr);
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), ClearColor);

        // 한 바퀴 다 돌 것 같으면 강제 리셋
        if (CurrentBufferOffset + MAX_INSTANCES_PER_FRAME >= TOTAL_BUFFER_INSTANCES)
        {
            CurrentBufferOffset = 0;
        }
    }

    uint32_t URenderer::UpdateInstanceBufferBatch(const Math::FPacked3x4Matrix* InMatrices, uint32_t InCount)
    {
        if (InCount == 0) return 0;

        D3D11_MAPPED_SUBRESOURCE MappedResource;
        D3D11_MAP MapType = (CurrentBufferOffset == 0) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
        
        if (SUCCEEDED(Context->Map(ConstantBuffer.Get(), 0, MapType, 0, &MappedResource)))
        {
            // [성능] memcpy를 사용하여 CPU 캐시 대역폭을 최대한 활용
            Math::FPacked3x4Matrix* Dest = static_cast<Math::FPacked3x4Matrix*>(MappedResource.pData) + CurrentBufferOffset;
            std::memcpy(Dest, InMatrices, sizeof(Math::FPacked3x4Matrix) * InCount);
            
            Context->Unmap(ConstantBuffer.Get(), 0);
        }

        uint32_t StartOffset = CurrentBufferOffset;
        CurrentBufferOffset += InCount;
        
        return StartOffset;
    }

    void URenderer::EndFrame()
    {
        SwapChain->Present(1, 0); 
    }
}