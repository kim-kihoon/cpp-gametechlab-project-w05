#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    /**
     * DirectX 11 렌더링을 담당하는 핵심 클래스.
     */
    class URenderer
    {
    public:
        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void BeginFrame();
        void EndFrame();
        
        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }

    private:
        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        ComPtr<IDXGISwapChain> SwapChain;
        ComPtr<ID3D11RenderTargetView> MainRenderTargetView;
    };
}