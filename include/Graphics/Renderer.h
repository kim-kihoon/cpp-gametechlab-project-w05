#pragma once
#include <Graphics/RendererTypes.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    /**
     * DirectX 11 렌더링 장치 및 디버그 렌더링 상태를 관리하는 클래스.
     */
    class URenderer
    {
    public:
        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void BeginFrame();
        void EndFrame();

        void SetDebugRenderSettings(const FDebugRenderSettings& InSettings);
        const FDebugRenderSettings& GetDebugRenderSettings() const { return DebugRenderSettings; }

        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }

    private:
        FDebugRenderSettings DebugRenderSettings;
        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        ComPtr<IDXGISwapChain> SwapChain;
        ComPtr<ID3D11RenderTargetView> MainRenderTargetView;
    };
}
