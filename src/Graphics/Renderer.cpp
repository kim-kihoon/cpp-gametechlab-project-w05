#include <Graphics/Renderer.h>
#include <Core/PathManager.h>
#include <Math/MathTypes.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Graphics
{
    namespace
    {
        struct FMeshVertex
        {
            DirectX::XMFLOAT3 Position;
            DirectX::XMFLOAT3 Normal;
        };

        struct FPerFrameConstants
        {
            DirectX::XMFLOAT4X4 ViewProj;
            DirectX::XMFLOAT4 LightDirection;
        };

        struct FPerObjectConstants
        {
            DirectX::XMFLOAT4 Row0;
            DirectX::XMFLOAT4 Row1;
            DirectX::XMFLOAT4 Row2;
            DirectX::XMFLOAT4 Padding;
        };

        struct FMaterialConstants
        {
            DirectX::XMFLOAT4 BaseColor;
        };

        bool ParseObjFaceIndex(const std::string& InToken, int& OutPositionIndex, int& OutNormalIndex)
        {
            OutPositionIndex = -1;
            OutNormalIndex = -1;

            const size_t FirstSlash = InToken.find('/');
            if (FirstSlash == std::string::npos)
            {
                OutPositionIndex = std::stoi(InToken) - 1;
                return OutPositionIndex >= 0;
            }

            OutPositionIndex = std::stoi(InToken.substr(0, FirstSlash)) - 1;

            const size_t LastSlash = InToken.rfind('/');
            if (LastSlash != std::string::npos && LastSlash + 1 < InToken.size())
            {
                OutNormalIndex = std::stoi(InToken.substr(LastSlash + 1)) - 1;
            }

            return OutPositionIndex >= 0;
        }

        bool LoadObjMeshData(const std::wstring& InPath, std::vector<FMeshVertex>& OutVertices, std::vector<uint32_t>& OutIndices)
        {
            std::ifstream File{ std::filesystem::path(InPath) };
            if (!File) return false;

            std::vector<DirectX::XMFLOAT3> Positions;
            std::vector<DirectX::XMFLOAT3> Normals;
            std::string Line;

            while (std::getline(File, Line))
            {
                if (Line.size() < 2) continue;

                std::istringstream LineStream(Line);
                std::string Prefix;
                LineStream >> Prefix;

                if (Prefix == "v")
                {
                    DirectX::XMFLOAT3 Position = {};
                    LineStream >> Position.x >> Position.y >> Position.z;
                    Positions.push_back(Position);
                }
                else if (Prefix == "vn")
                {
                    DirectX::XMFLOAT3 Normal = {};
                    LineStream >> Normal.x >> Normal.y >> Normal.z;
                    Normals.push_back(Normal);
                }
                else if (Prefix == "f")
                {
                    std::array<std::string, 3> Tokens = {};
                    if (!(LineStream >> Tokens[0] >> Tokens[1] >> Tokens[2]))
                    {
                        continue;
                    }

                    for (const std::string& Token : Tokens)
                    {
                        int PositionIndex = -1;
                        int NormalIndex = -1;
                        if (!ParseObjFaceIndex(Token, PositionIndex, NormalIndex)) continue;
                        if (PositionIndex < 0 || static_cast<size_t>(PositionIndex) >= Positions.size()) continue;

                        FMeshVertex Vertex = {};
                        Vertex.Position = Positions[PositionIndex];
                        Vertex.Normal = (NormalIndex >= 0 && static_cast<size_t>(NormalIndex) < Normals.size())
                            ? Normals[NormalIndex]
                            : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };

                        OutVertices.push_back(Vertex);
                        OutIndices.push_back(static_cast<uint32_t>(OutIndices.size()));
                    }
                }
            }

            return !OutVertices.empty() && !OutIndices.empty();
        }

        bool CreateMeshBuffers(
            ID3D11Device* InDevice,
            const std::wstring& InPath,
            URenderer::FMeshResource& OutMeshResource)
        {
            std::vector<FMeshVertex> Vertices;
            std::vector<uint32_t> Indices;
            if (!LoadObjMeshData(InPath, Vertices, Indices))
            {
                return false;
            }

            D3D11_BUFFER_DESC VertexBufferDesc = {};
            VertexBufferDesc.ByteWidth = static_cast<UINT>(Vertices.size() * sizeof(FMeshVertex));
            VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA VertexData = { Vertices.data(), 0, 0 };
            if (FAILED(InDevice->CreateBuffer(&VertexBufferDesc, &VertexData, &OutMeshResource.VertexBuffer))) return false;

            D3D11_BUFFER_DESC IndexBufferDesc = {};
            IndexBufferDesc.ByteWidth = static_cast<UINT>(Indices.size() * sizeof(uint32_t));
            IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            D3D11_SUBRESOURCE_DATA IndexData = { Indices.data(), 0, 0 };
            if (FAILED(InDevice->CreateBuffer(&IndexBufferDesc, &IndexData, &OutMeshResource.IndexBuffer))) return false;

            OutMeshResource.IndexCount = static_cast<uint32_t>(Indices.size());
            return true;
        }
    }

    URenderer::URenderer() = default;
    URenderer::~URenderer() = default;

    bool URenderer::Initialize(HWND InWindowHandle, int InWidth, int InHeight)
    {
        ViewportWidth = static_cast<uint32_t>(InWidth);
        ViewportHeight = static_cast<uint32_t>(InHeight);

        DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
        SwapChainDesc.BufferCount = 2;
        SwapChainDesc.BufferDesc.Width = InWidth;
        SwapChainDesc.BufferDesc.Height = InHeight;
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferDesc.RefreshRate.Numerator = 165;
        SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.OutputWindow = InWindowHandle;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.SampleDesc.Quality = 0;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        SwapChainDesc.Windowed = TRUE;

        const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        const HRESULT Result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            FeatureLevels,
            1,
            D3D11_SDK_VERSION,
            &SwapChainDesc,
            &SwapChain,
            &Device,
            nullptr,
            &Context);
        if (FAILED(Result)) return false;

        Context.As(&Context1);

        ComPtr<ID3D11Texture2D> BackBuffer;
        if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer)))) return false;
        if (FAILED(Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView))) return false;

        D3D11_TEXTURE2D_DESC DepthDesc = {};
        DepthDesc.Width = InWidth;
        DepthDesc.Height = InHeight;
        DepthDesc.MipLevels = 1;
        DepthDesc.ArraySize = 1;
        DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthDesc.SampleDesc.Count = 1;
        DepthDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        ComPtr<ID3D11Texture2D> DepthBuffer;
        if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer))) return false;
        if (FAILED(Device->CreateDepthStencilView(DepthBuffer.Get(), nullptr, &DepthStencilView))) return false;

        return CreateDefaultResources();
    }

    bool URenderer::CreateDefaultResources()
    {
        const char* ShaderSrc = R"(
            cbuffer PerFrame : register(b0)
            {
                row_major float4x4 ViewProj;
                float4 LightDirection;
            };

            cbuffer PerObject : register(b1)
            {
                float4 Row0;
                float4 Row1;
                float4 Row2;
                float4 Padding;
            };

            cbuffer MaterialData : register(b2)
            {
                float4 BaseColor;
            };

            struct VS_IN
            {
                float3 Pos : POSITION;
                float3 Norm : NORMAL;
            };

            struct PS_IN
            {
                float4 Pos : SV_POSITION;
                float3 Norm : NORMAL;
            };

            PS_IN VSMain(VS_IN i)
            {
                PS_IN o;
                float4 LocalPos = float4(i.Pos, 1.0f);
                float4 LocalNorm = float4(i.Norm, 0.0f);

                float3 WorldPos = float3(
                    dot(LocalPos, Row0),
                    dot(LocalPos, Row1),
                    dot(LocalPos, Row2));

                float3 WorldNorm = normalize(float3(
                    dot(LocalNorm, Row0),
                    dot(LocalNorm, Row1),
                    dot(LocalNorm, Row2)));

                o.Pos = mul(float4(WorldPos, 1.0f), ViewProj);
                o.Norm = WorldNorm;
                return o;
            }

            float4 PSMain(PS_IN i) : SV_TARGET
            {
                float3 N = normalize(i.Norm);
                float3 L = normalize(-LightDirection.xyz);
                float Diffuse = saturate(dot(N, L)) * 0.75f + 0.25f;
                return float4(BaseColor.rgb * Diffuse, BaseColor.a);
            }
        )";

        ComPtr<ID3DBlob> VS;
        ComPtr<ID3DBlob> PS;
        ComPtr<ID3DBlob> Err;

        if (FAILED(D3DCompile(ShaderSrc, std::strlen(ShaderSrc), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err))) return false;
        if (FAILED(D3DCompile(ShaderSrc, std::strlen(ShaderSrc), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, &Err))) return false;
        if (FAILED(Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &VertexShader))) return false;
        if (FAILED(Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &PixelShader))) return false;

        D3D11_INPUT_ELEMENT_DESC Layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(Device->CreateInputLayout(Layout, static_cast<UINT>(std::size(Layout)), VS->GetBufferPointer(), VS->GetBufferSize(), &InputLayout)))
        {
            return false;
        }

        D3D11_BUFFER_DESC PerFrameDesc = {};
        PerFrameDesc.ByteWidth = sizeof(FPerFrameConstants);
        PerFrameDesc.Usage = D3D11_USAGE_DYNAMIC;
        PerFrameDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        PerFrameDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&PerFrameDesc, nullptr, &PerFrameBuffer))) return false;

        D3D11_BUFFER_DESC PerObjectDesc = {};
        PerObjectDesc.ByteWidth = sizeof(FPerObjectConstants);
        PerObjectDesc.Usage = D3D11_USAGE_DYNAMIC;
        PerObjectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        PerObjectDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&PerObjectDesc, nullptr, &PerObjectBuffer))) return false;

        D3D11_BUFFER_DESC MaterialDesc = {};
        MaterialDesc.ByteWidth = sizeof(FMaterialConstants);
        MaterialDesc.Usage = D3D11_USAGE_DYNAMIC;
        MaterialDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        MaterialDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(Device->CreateBuffer(&MaterialDesc, nullptr, &MaterialBuffer))) return false;

        const std::wstring MeshBasePath = Core::FPathManager::GetMeshPath();
        if (!CreateMeshBuffers(Device.Get(), MeshBasePath + L"apple_mid.obj", MeshResources[0])) return false;
        if (!CreateMeshBuffers(Device.Get(), MeshBasePath + L"bitten_apple_mid.obj", MeshResources[1])) return false;

        return true;
    }

    void URenderer::BeginFrame()
    {
        const float Color[4] = { 0.03f, 0.03f, 0.06f, 1.0f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), DepthStencilView.Get());
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), Color);
        Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        D3D11_VIEWPORT Viewport = {};
        Viewport.Width = static_cast<float>(ViewportWidth);
        Viewport.Height = static_cast<float>(ViewportHeight);
        Viewport.MinDepth = 0.0f;
        Viewport.MaxDepth = 1.0f;
        Context->RSSetViewports(1, &Viewport);
    }

    void URenderer::RenderScene(const Scene::USceneManager& InSceneManager)
    {
        const Scene::FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        if (!SceneData || !PerFrameBuffer || !PerObjectBuffer || !MaterialBuffer) return;

        const uint32_t SourceCount = (SceneData->RenderCount > 0) ? SceneData->RenderCount : InSceneManager.GetSceneStatistics().TotalObjectCount;
        if (SourceCount == 0) return;

        thread_local std::array<std::vector<uint32_t>, MAX_MESH_TYPES> Buckets;
        for (std::vector<uint32_t>& Bucket : Buckets)
        {
            Bucket.clear();
            Bucket.reserve(SourceCount / MAX_MESH_TYPES + 1);
        }

        for (uint32_t RenderIndex = 0; RenderIndex < SourceCount; ++RenderIndex)
        {
            const uint32_t ObjectIndex = (SceneData->RenderCount > 0) ? SceneData->RenderQueue[RenderIndex] : RenderIndex;
            const uint32_t MeshID = SceneData->MeshIDs[ObjectIndex];
            if (MeshID >= MAX_MESH_TYPES) continue;
            Buckets[MeshID].push_back(ObjectIndex);
        }

        const float AspectRatio = (ViewportHeight == 0) ? 1.0f : static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
        const DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(-60.0f, -60.0f, 45.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 1.0f),
            DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        const DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), AspectRatio, 0.1f, 500.0f);
        const DirectX::XMMATRIX ViewProj = View * Projection;

        D3D11_MAPPED_SUBRESOURCE PerFrameMap = {};
        if (FAILED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &PerFrameMap))) return;
        FPerFrameConstants PerFrameConstants = {};
        DirectX::XMStoreFloat4x4(&PerFrameConstants.ViewProj, ViewProj);
        PerFrameConstants.LightDirection = { 0.4f, 0.5f, -1.0f, 0.0f };
        std::memcpy(PerFrameMap.pData, &PerFrameConstants, sizeof(PerFrameConstants));
        Context->Unmap(PerFrameBuffer.Get(), 0);

        ComPtr<ID3D11RasterizerState> RasterizerState;
        D3D11_RASTERIZER_DESC RasterizerDesc = {};
        RasterizerDesc.FillMode = D3D11_FILL_SOLID;
        RasterizerDesc.CullMode = D3D11_CULL_BACK;
        RasterizerDesc.DepthClipEnable = TRUE;
        if (SUCCEEDED(Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState)))
        {
            Context->RSSetState(RasterizerState.Get());
        }

        ComPtr<ID3D11DepthStencilState> DepthState;
        D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
        DepthDesc.DepthEnable = TRUE;
        DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (SUCCEEDED(Device->CreateDepthStencilState(&DepthDesc, &DepthState)))
        {
            Context->OMSetDepthStencilState(DepthState.Get(), 0);
        }

        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0);
        Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());

        const std::array<DirectX::XMFLOAT4, MAX_MESH_TYPES> MeshColors = {
            DirectX::XMFLOAT4{ 0.84f, 0.15f, 0.10f, 1.0f },
            DirectX::XMFLOAT4{ 0.98f, 0.72f, 0.30f, 1.0f }
        };

        for (uint32_t MeshID = 0; MeshID < MAX_MESH_TYPES; ++MeshID)
        {
            const std::vector<uint32_t>& Objects = Buckets[MeshID];
            const FMeshResource& MeshResource = MeshResources[MeshID];
            if (Objects.empty() || !MeshResource.VertexBuffer || !MeshResource.IndexBuffer || MeshResource.IndexCount == 0) continue;

            D3D11_MAPPED_SUBRESOURCE MaterialMap = {};
            if (FAILED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MaterialMap))) continue;
            FMaterialConstants MaterialConstants = {};
            MaterialConstants.BaseColor = MeshColors[MeshID];
            std::memcpy(MaterialMap.pData, &MaterialConstants, sizeof(MaterialConstants));
            Context->Unmap(MaterialBuffer.Get(), 0);
            Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf());

            UINT Stride = sizeof(FMeshVertex);
            UINT Offset = 0;
            Context->IASetVertexBuffers(0, 1, MeshResource.VertexBuffer.GetAddressOf(), &Stride, &Offset);
            Context->IASetIndexBuffer(MeshResource.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            for (uint32_t ObjectIndex : Objects)
            {
                const Math::FPacked3x4Matrix& PackedMatrix = SceneData->WorldMatrices[ObjectIndex];

                D3D11_MAPPED_SUBRESOURCE PerObjectMap = {};
                if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &PerObjectMap))) continue;

                FPerObjectConstants PerObjectConstants = {};
                DirectX::XMStoreFloat4(&PerObjectConstants.Row0, PackedMatrix.Row0);
                DirectX::XMStoreFloat4(&PerObjectConstants.Row1, PackedMatrix.Row1);
                DirectX::XMStoreFloat4(&PerObjectConstants.Row2, PackedMatrix.Row2);
                PerObjectConstants.Padding = { 0.0f, 0.0f, 0.0f, 1.0f };
                std::memcpy(PerObjectMap.pData, &PerObjectConstants, sizeof(PerObjectConstants));
                Context->Unmap(PerObjectBuffer.Get(), 0);

                Context->VSSetConstantBuffers(1, 1, PerObjectBuffer.GetAddressOf());
                Context->DrawIndexed(MeshResource.IndexCount, 0, 0);
            }
        }
    }

    void URenderer::EndFrame()
    {
        SwapChain->Present(0, 0);
    }
}
