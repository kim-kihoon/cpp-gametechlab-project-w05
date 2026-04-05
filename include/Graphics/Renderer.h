#pragma once
#include <DirectXMath.h>
#include <Graphics/RendererTypes.h>
#include <array>
#include <d3d11_1.h>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Scene
{
class USceneManager;
}

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

    struct FBillboardVertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 TexCoord;
    };

    struct FImpostorResource
    {
        ComPtr<ID3D11Texture2D> SnapshotTexture;
        ComPtr<ID3D11ShaderResourceView> SnapshotSRV;
        bool bIsBaked = false;
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
        DirectX::XMFLOAT3 LocalCenter = {0, 0, 0};
    };

    URenderer();
    ~URenderer();

    bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
    void BeginFrame();
    void EndFrame();
    void RenderScene(const Scene::USceneManager& InSceneManager);
    void SetCameraState(const FCameraState& InCameraState)
    {
        CameraState = InCameraState;
    }
    const FCameraState& GetCameraState() const
    {
        return CameraState;
    }

    void SetDebugRenderSettings(const FDebugRenderSettings& InSettings)
    {
        DebugSettings = InSettings;
    }
    const FDebugRenderSettings& GetDebugRenderSettings() const
    {
        return DebugSettings;
    }

    ID3D11Device* GetDevice()
    {
        return Device.Get();
    }
    ID3D11DeviceContext* GetContext()
    {
        return Context.Get();
    }
    ID3D11DeviceContext1* GetContext1()
    {
        return Context1.Get();
    }

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

    ComPtr<ID3D11VertexShader> BillboardVS;
    ComPtr<ID3D11PixelShader> BillboardPS;
    ComPtr<ID3D11InputLayout> BillboardLayout;
    ComPtr<ID3D11Buffer> BillboardVB;
    ComPtr<ID3D11Buffer> BillboardIB;

    ComPtr<ID3D11RasterizerState> DefaultRasterizerState;
    ComPtr<ID3D11DepthStencilState> DefaultDepthStencilState;

    std::array<FMeshResource, MAX_MESH_TYPES> MeshResources = {};
    std::array<FImpostorResource, MAX_MESH_TYPES> ImpostorResources = {};
    uint32_t ViewportWidth = 0;
    uint32_t ViewportHeight = 0;
    uint32_t PerObjectRingBufferOffset = 0;
    FCameraState CameraState = {};

    FDebugRenderSettings DebugSettings;
    ComPtr<ID3D11Buffer> BakePerFrameBuffer;
    ComPtr<ID3D11Buffer> BakePerObjectBuffer;
    ComPtr<ID3D11Buffer> BakeMatBuffer;

    void BakeImpostor(uint32_t MeshID);
};
} // namespace Graphics
