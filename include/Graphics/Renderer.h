#pragma once
#include <Graphics/RendererTypes.h>
#include <array>
#include <d3d11_1.h>
#include <wrl/client.h>

namespace Scene { class USceneManager; }

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    class URenderer
    {
    public:
        struct FMeshResource
        {
            ComPtr<ID3D11Buffer> VertexBuffer;
            ComPtr<ID3D11Buffer> IndexBuffer;
            uint32_t IndexCount = 0;
        };

        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void BeginFrame();
        void EndFrame();
        void RenderScene(const Scene::USceneManager& InSceneManager);

        void SetDebugRenderSettings(const FDebugRenderSettings& InSettings) { DebugSettings = InSettings; }
        const FDebugRenderSettings& GetDebugRenderSettings() const { return DebugSettings; }

        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }
        ID3D11DeviceContext1* GetContext1() { return Context1.Get(); }

    private:
        bool CreateDefaultResources();

    private:
        static constexpr uint32_t MAX_MESH_TYPES = 2;

        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        ComPtr<ID3D11DeviceContext1> Context1;
        ComPtr<IDXGISwapChain> SwapChain;
        ComPtr<ID3D11RenderTargetView> MainRenderTargetView;
        ComPtr<ID3D11DepthStencilView> DepthStencilView;

        ComPtr<ID3D11VertexShader> VertexShader;
        ComPtr<ID3D11PixelShader> PixelShader;
        ComPtr<ID3D11InputLayout> InputLayout;
        ComPtr<ID3D11Buffer> PerFrameBuffer;
        ComPtr<ID3D11Buffer> PerObjectBuffer;
        ComPtr<ID3D11Buffer> MaterialBuffer;

        std::array<FMeshResource, MAX_MESH_TYPES> MeshResources = {};
        uint32_t ViewportWidth = 0;
        uint32_t ViewportHeight = 0;

        FDebugRenderSettings DebugSettings;
    };
}
