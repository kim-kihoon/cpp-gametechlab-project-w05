#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace ExtremeGraphics
{
    using Microsoft::WRL::ComPtr;

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        bool Initialize(HWND hWnd, int width, int height);
        void BeginFrame();
        void EndFrame();
        
        ID3D11Device* GetDevice() { return m_pd3dDevice.Get(); }
        ID3D11DeviceContext* GetContext() { return m_pd3dDeviceContext.Get(); }

    private:
        ComPtr<ID3D11Device> m_pd3dDevice;
        ComPtr<ID3D11DeviceContext> m_pd3dDeviceContext;
        ComPtr<IDXGISwapChain> m_pSwapChain;
        ComPtr<ID3D11RenderTargetView> m_pMainRenderTargetView;
    };
}