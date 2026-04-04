#pragma once
#include <Graphics/RendererTypes.h>
#include <Graphics/HUD.h>
#include <Core/AppTypes.h>
#include <DirectXMath.h>
#include <array>
#include <d3d11_1.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "Math/MathTypes.h"

namespace Scene { class USceneManager; }

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    class URenderer
    {
    public:
        struct FMeshVertex
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Normal;
            DirectX::XMFLOAT2 TexCoord;
        };

        struct FBVHNode
        {
            Math::FBox Bounds;
            uint32_t LeftChild = 0;
            uint32_t RightChild = 0;
            uint32_t TriangleIndex = 0;
            uint32_t TriangleCount = 0;

            bool IsLeaf() const { return TriangleCount > 0; }
        };

        struct FBVH
        {
            std::vector<FBVHNode> Nodes;
            std::vector<uint32_t> TriangleIndices; // Indices of triangles (first vertex in SourceIndices)
        };

        struct FMeshResource
        {
            std::vector<FMeshVertex> SourceVertices;
            std::vector<uint32_t> SourceIndices;
            ComPtr<ID3D11Buffer> VertexBuffer;
            ComPtr<ID3D11Buffer> IndexBuffer;
            ComPtr<ID3D11ShaderResourceView> DiffuseTextureView;
            std::wstring DiffuseTexturePath;
            uint32_t IndexCount = 0;
            uint32_t ObjectCount = 0;
            FBVH MeshBVH;

            void BuildBVH();
            bool Raycast(const Math::FRay& LocalRay, float& OutT) const;
        };

        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void Resize(int Width, int Height);
        void BeginFrame();
        void EndFrame();
        void RenderScene(const Scene::USceneManager& InSceneManager);
        void RenderHUD(); // 추가: HUD 렌더링 호출용

        void SetCameraState(const FCameraState& InCameraState) { CameraState = InCameraState; }
        const FCameraState& GetCameraState() const { return CameraState; }

        void SetDebugRenderSettings(const FDebugRenderSettings& InSettings) { DebugSettings = InSettings; }
        const FDebugRenderSettings& GetDebugRenderSettings() const { return DebugSettings; }

        // 추가: 성능 데이터 전달용
        void UpdatePerformanceMetrics(const Core::FFramePerformanceMetrics& InMetrics) { CurrentMetrics = InMetrics; }

        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }
        ID3D11DeviceContext1* GetContext1() { return Context1.Get(); }

        const FMeshResource* GetMeshResource(uint32_t MeshID) const;

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
        ComPtr<ID3D11SamplerState> DiffuseSamplerState;
        ComPtr<ID3D11ShaderResourceView> DefaultWhiteTextureView;

        ComPtr<ID3D11RasterizerState> DefaultRasterizerState;
        ComPtr<ID3D11DepthStencilState> DefaultDepthStencilState;

        std::array<FMeshResource, MAX_MESH_TYPES> MeshResources = {};
        uint32_t ViewportWidth = 0;
        uint32_t ViewportHeight = 0;
        uint32_t PerObjectRingBufferOffset = 0;
        FCameraState CameraState = {};

        FDebugRenderSettings DebugSettings;
        Core::FFramePerformanceMetrics CurrentMetrics; // 추가: HUD에 전달할 데이터
        std::unique_ptr<FHUD> HUD; // 추가: HUD 객체
    };
}
