#include <Graphics/Renderer.h>
#include <Core/PathManager.h>
#include <Math/MathTypes.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
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
            DirectX::XMFLOAT2 TexCoord;
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

        bool ParseObjFaceIndices(const std::string& InToken, int& OutPositionIndex, int& OutTexCoordIndex, int& OutNormalIndex)
        {
            OutPositionIndex = -1;
            OutTexCoordIndex = -1;
            OutNormalIndex = -1;

            const size_t FirstSlash = InToken.find('/');
            if (FirstSlash == std::string::npos)
            {
                OutPositionIndex = std::stoi(InToken) - 1;
                return OutPositionIndex >= 0;
            }

            OutPositionIndex = std::stoi(InToken.substr(0, FirstSlash)) - 1;

            const size_t SecondSlash = InToken.find('/', FirstSlash + 1);
            if (SecondSlash == std::string::npos)
            {
                if (FirstSlash + 1 < InToken.size())
                {
                    OutTexCoordIndex = std::stoi(InToken.substr(FirstSlash + 1)) - 1;
                }
                return OutPositionIndex >= 0;
            }

            if (SecondSlash > FirstSlash + 1)
            {
                OutTexCoordIndex = std::stoi(InToken.substr(FirstSlash + 1, SecondSlash - FirstSlash - 1)) - 1;
            }

            if (SecondSlash + 1 < InToken.size())
            {
                OutNormalIndex = std::stoi(InToken.substr(SecondSlash + 1)) - 1;
            }

            return OutPositionIndex >= 0;
        }

        std::wstring ReadDiffuseTexturePathFromMtl(const std::filesystem::path& InMtlPath)
        {
            std::ifstream File(InMtlPath);
            if (!File) return L"";

            std::string Line;
            while (std::getline(File, Line))
            {
                std::istringstream LineStream(Line);
                std::string Prefix;
                LineStream >> Prefix;
                if (Prefix == "map_Kd")
                {
                    std::string TextureRelativePath;
                    std::getline(LineStream >> std::ws, TextureRelativePath);
                    if (TextureRelativePath.empty()) return L"";
                    return (InMtlPath.parent_path() / std::filesystem::path(TextureRelativePath)).lexically_normal().wstring();
                }
            }

            return L"";
        }

        bool LoadObjMeshData(
            const std::wstring& InPath,
            std::vector<FMeshVertex>& OutVertices,
            std::vector<uint32_t>& OutIndices,
            std::wstring& OutDiffuseTexturePath)
        {
            std::ifstream File{ std::filesystem::path(InPath) };
            if (!File) return false;

            std::vector<DirectX::XMFLOAT3> Positions;
            std::vector<DirectX::XMFLOAT3> Normals;
            std::vector<DirectX::XMFLOAT2> TexCoords;
            std::filesystem::path MaterialLibraryPath;
            std::string Line;

            while (std::getline(File, Line))
            {
                if (Line.size() < 2) continue;

                std::istringstream LineStream(Line);
                std::string Prefix;
                LineStream >> Prefix;

                if (Prefix == "mtllib")
                {
                    std::string RelativeMaterialPath;
                    std::getline(LineStream >> std::ws, RelativeMaterialPath);
                    if (!RelativeMaterialPath.empty())
                    {
                        MaterialLibraryPath = std::filesystem::path(InPath).parent_path() / std::filesystem::path(RelativeMaterialPath);
                    }
                }
                else if (Prefix == "v")
                {
                    DirectX::XMFLOAT3 Position = {};
                    LineStream >> Position.x >> Position.y >> Position.z;
                    Positions.push_back(Position);
                }
                else if (Prefix == "vt")
                {
                    DirectX::XMFLOAT2 TexCoord = {};
                    LineStream >> TexCoord.x >> TexCoord.y;
                    TexCoord.y = 1.0f - TexCoord.y;
                    TexCoords.push_back(TexCoord);
                }
                else if (Prefix == "vn")
                {
                    DirectX::XMFLOAT3 Normal = {};
                    LineStream >> Normal.x >> Normal.y >> Normal.z;
                    Normals.push_back(Normal);
                }
                else if (Prefix == "f")
                {
                    std::vector<std::string> Tokens;
                    std::string Token;
                    while (LineStream >> Token)
                    {
                        Tokens.push_back(Token);
                    }

                    if (Tokens.size() < 3)
                    {
                        continue;
                    }

                    for (size_t TriangleIndex = 1; TriangleIndex + 1 < Tokens.size(); ++TriangleIndex)
                    {
                        const std::array<std::string, 3> TriangleTokens = { Tokens[0], Tokens[TriangleIndex], Tokens[TriangleIndex + 1] };

                        for (const std::string& FaceToken : TriangleTokens)
                        {
                            int PositionIndex = -1;
                            int TexCoordIndex = -1;
                            int NormalIndex = -1;
                            if (!ParseObjFaceIndices(FaceToken, PositionIndex, TexCoordIndex, NormalIndex)) continue;
                            if (PositionIndex < 0 || static_cast<size_t>(PositionIndex) >= Positions.size()) continue;

                            FMeshVertex Vertex = {};
                            Vertex.Position = Positions[PositionIndex];
                            Vertex.Normal = (NormalIndex >= 0 && static_cast<size_t>(NormalIndex) < Normals.size())
                                ? Normals[NormalIndex]
                                : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
                            Vertex.TexCoord = (TexCoordIndex >= 0 && static_cast<size_t>(TexCoordIndex) < TexCoords.size())
                                ? TexCoords[TexCoordIndex]
                                : DirectX::XMFLOAT2{ 0.0f, 0.0f };

                            OutVertices.push_back(Vertex);
                            OutIndices.push_back(static_cast<uint32_t>(OutIndices.size()));
                        }
                    }
                }
            }

            if (!MaterialLibraryPath.empty())
            {
                OutDiffuseTexturePath = ReadDiffuseTexturePathFromMtl(MaterialLibraryPath);
            }

            return !OutVertices.empty() && !OutIndices.empty();
        }

        bool CreateSolidTexture(
            ID3D11Device* InDevice,
            const DirectX::XMFLOAT4& InColor,
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& OutTextureView)
        {
            const uint8_t PixelData[4] = {
                static_cast<uint8_t>(InColor.x * 255.0f),
                static_cast<uint8_t>(InColor.y * 255.0f),
                static_cast<uint8_t>(InColor.z * 255.0f),
                static_cast<uint8_t>(InColor.w * 255.0f)
            };

            D3D11_TEXTURE2D_DESC TextureDesc = {};
            TextureDesc.Width = 1;
            TextureDesc.Height = 1;
            TextureDesc.MipLevels = 1;
            TextureDesc.ArraySize = 1;
            TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            TextureDesc.SampleDesc.Count = 1;
            TextureDesc.Usage = D3D11_USAGE_DEFAULT;
            TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA InitialData = {};
            InitialData.pSysMem = PixelData;
            InitialData.SysMemPitch = sizeof(PixelData);

            Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
            if (FAILED(InDevice->CreateTexture2D(&TextureDesc, &InitialData, &Texture))) return false;
            return SUCCEEDED(InDevice->CreateShaderResourceView(Texture.Get(), nullptr, &OutTextureView));
        }

        bool LoadTextureWithWIC(
            ID3D11Device* InDevice,
            const std::wstring& InTexturePath,
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& OutTextureView)
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> ImagingFactory;
            if (FAILED(::CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&ImagingFactory))))
            {
                return false;
            }

            Microsoft::WRL::ComPtr<IWICBitmapDecoder> Decoder;
            if (FAILED(ImagingFactory->CreateDecoderFromFilename(
                InTexturePath.c_str(),
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &Decoder)))
            {
                return false;
            }

            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> Frame;
            if (FAILED(Decoder->GetFrame(0, &Frame))) return false;

            Microsoft::WRL::ComPtr<IWICFormatConverter> FormatConverter;
            if (FAILED(ImagingFactory->CreateFormatConverter(&FormatConverter))) return false;
            if (FAILED(FormatConverter->Initialize(
                Frame.Get(),
                GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone,
                nullptr,
                0.0f,
                WICBitmapPaletteTypeCustom)))
            {
                return false;
            }

            UINT Width = 0;
            UINT Height = 0;
            if (FAILED(FormatConverter->GetSize(&Width, &Height)) || Width == 0 || Height == 0) return false;

            std::vector<uint8_t> Pixels(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u);
            if (FAILED(FormatConverter->CopyPixels(nullptr, Width * 4u, static_cast<UINT>(Pixels.size()), Pixels.data())))
            {
                return false;
            }

            D3D11_TEXTURE2D_DESC TextureDesc = {};
            TextureDesc.Width = Width;
            TextureDesc.Height = Height;
            TextureDesc.MipLevels = 1;
            TextureDesc.ArraySize = 1;
            TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            TextureDesc.SampleDesc.Count = 1;
            TextureDesc.Usage = D3D11_USAGE_DEFAULT;
            TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA InitialData = {};
            InitialData.pSysMem = Pixels.data();
            InitialData.SysMemPitch = Width * 4u;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
            if (FAILED(InDevice->CreateTexture2D(&TextureDesc, &InitialData, &Texture))) return false;
            return SUCCEEDED(InDevice->CreateShaderResourceView(Texture.Get(), nullptr, &OutTextureView));
        }

        bool CreateMeshBuffers(
            ID3D11Device* InDevice,
            const std::wstring& InPath,
            URenderer::FMeshResource& OutMeshResource)
        {
            std::vector<FMeshVertex> Vertices;
            std::vector<uint32_t> Indices;
            std::wstring DiffuseTexturePath;
            if (!LoadObjMeshData(InPath, Vertices, Indices, DiffuseTexturePath))
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

            if (!DiffuseTexturePath.empty() && !LoadTextureWithWIC(InDevice, DiffuseTexturePath, OutMeshResource.DiffuseTextureView))
            {
                return false;
            }

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
        SwapChainDesc.BufferCount = 3; // Triple Buffering
        SwapChainDesc.BufferDesc.Width = InWidth;
        SwapChainDesc.BufferDesc.Height = InHeight;
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferDesc.RefreshRate.Numerator = 0; // Uncapped
        SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.OutputWindow = InWindowHandle;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.SampleDesc.Quality = 0;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
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

            Texture2D DiffuseTexture : register(t0);
            SamplerState DiffuseSampler : register(s0);

            struct VS_IN
            {
                float3 Pos : POSITION;
                float3 Norm : NORMAL;
                float2 TexCoord : TEXCOORD0;
            };

            struct PS_IN
            {
                float4 Pos : SV_POSITION;
                float3 Norm : NORMAL;
                float2 TexCoord : TEXCOORD0;
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
                o.TexCoord = i.TexCoord;
                return o;
            }

            float4 PSMain(PS_IN i) : SV_TARGET
            {
                float3 N = normalize(i.Norm);
                float3 L = normalize(-LightDirection.xyz);
                float Diffuse = saturate(dot(N, L)) * 0.75f + 0.25f;
                float4 Albedo = DiffuseTexture.Sample(DiffuseSampler, i.TexCoord);
                return float4(Albedo.rgb * BaseColor.rgb * Diffuse, Albedo.a * BaseColor.a);
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
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
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
        PerObjectDesc.ByteWidth = 16 * 1024 * 1024; // 16MB Large Buffer for Bulk Update
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

        D3D11_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        SamplerDesc.MinLOD = 0.0f;
        SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(Device->CreateSamplerState(&SamplerDesc, &DiffuseSamplerState))) return false;

        D3D11_RASTERIZER_DESC RasterizerDesc = {};
        RasterizerDesc.FillMode = D3D11_FILL_SOLID;
        RasterizerDesc.CullMode = D3D11_CULL_BACK;
        RasterizerDesc.DepthClipEnable = TRUE;
        if (FAILED(Device->CreateRasterizerState(&RasterizerDesc, &DefaultRasterizerState))) return false;

        D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
        DepthDesc.DepthEnable = TRUE;
        DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        DepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(Device->CreateDepthStencilState(&DepthDesc, &DefaultDepthStencilState))) return false;

        if (!CreateSolidTexture(Device.Get(), { 1.0f, 1.0f, 1.0f, 1.0f }, DefaultWhiteTextureView)) return false;

        const std::wstring MeshBasePath = Core::FPathManager::GetMeshPath();
        if (!CreateMeshBuffers(Device.Get(), MeshBasePath + L"apple_mid.obj", MeshResources[0])) return false;
        if (!CreateMeshBuffers(Device.Get(), MeshBasePath + L"bitten_apple_mid.obj", MeshResources[1])) return false;

        if (!MeshResources[0].DiffuseTextureView) MeshResources[0].DiffuseTextureView = DefaultWhiteTextureView;
        if (!MeshResources[1].DiffuseTextureView) MeshResources[1].DiffuseTextureView = DefaultWhiteTextureView;

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

        PerObjectRingBufferOffset = 0;
    }

    void URenderer::RenderScene(const Scene::USceneManager& InSceneManager)
    {
        const Scene::FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        if (!SceneData || !PerFrameBuffer || !PerObjectBuffer || !MaterialBuffer) return;

        const uint32_t SourceCount = SceneData->RenderCount;
        if (SourceCount == 0) return;

        const uint32_t AlignedConstantSize = 256;
        
        // [전략] 벌크 업데이트: 5만 번의 Map을 단 1회로 단축
        D3D11_MAPPED_SUBRESOURCE PerObjectMap = {};
        if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &PerObjectMap))) return;
        
        uint8_t* DestStart = static_cast<uint8_t*>(PerObjectMap.pData);
        
        // 가시적 객체들의 데이터를 일괄 복사하고 오프셋 저장
        thread_local std::vector<uint32_t> ObjectOffsets;
        if (ObjectOffsets.size() < Scene::FSceneDataSOA::MAX_OBJECTS) ObjectOffsets.resize(Scene::FSceneDataSOA::MAX_OBJECTS);

        thread_local std::array<std::vector<uint32_t>, MAX_MESH_TYPES> Buckets;
        for (auto& Bucket : Buckets) { Bucket.clear(); Bucket.reserve(SourceCount / MAX_MESH_TYPES + 1); }

        for (uint32_t i = 0; i < SourceCount; ++i)
        {
            const uint32_t ObjectIndex = SceneData->RenderQueue[i];
            const uint32_t MeshID = SceneData->MeshIDs[ObjectIndex];
            if (MeshID >= MAX_MESH_TYPES) continue;
            
            Buckets[MeshID].push_back(ObjectIndex);

            // 데이터 일괄 기록 및 오프셋 계산
            const uint32_t Offset = i * AlignedConstantSize;
            ObjectOffsets[ObjectIndex] = Offset;

            const Math::FPacked3x4Matrix& PackedMatrix = SceneData->WorldMatrices[ObjectIndex];
            FPerObjectConstants* Dest = reinterpret_cast<FPerObjectConstants*>(DestStart + Offset);
            
            DirectX::XMStoreFloat4(&Dest->Row0, PackedMatrix.Row0);
            DirectX::XMStoreFloat4(&Dest->Row1, PackedMatrix.Row1);
            DirectX::XMStoreFloat4(&Dest->Row2, PackedMatrix.Row2);
            Dest->Padding = { 0.0f, 0.0f, 0.0f, 1.0f };
        }
        Context->Unmap(PerObjectBuffer.Get(), 0);

        // --- 여기서부터는 Map 호출 없이 순수 Draw만 수행하여 GPU Command Processor 부담 제거 ---
        const float AspectRatio = (ViewportHeight == 0) ? 1.0f : static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
        const float CosPitch = std::cos(CameraState.PitchRadians);
        const float SinPitch = std::sin(CameraState.PitchRadians);
        const float CosYaw = std::cos(CameraState.YawRadians);
        const float SinYaw = std::sin(CameraState.YawRadians);

        DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&CameraState.Position);
        DirectX::XMVECTOR Forward = DirectX::XMVector3Normalize(DirectX::XMVectorSet(CosPitch * CosYaw, CosPitch * SinYaw, SinPitch, 0.0f));
        DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        DirectX::XMVECTOR CameraTarget = DirectX::XMVectorAdd(CameraPosition, Forward);

        const DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(CameraPosition, CameraTarget, WorldUp);
        const DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(CameraState.FOVDegrees), AspectRatio, CameraState.NearClip, CameraState.FarClip);
        const DirectX::XMMATRIX ViewProj = View * Projection;

        D3D11_MAPPED_SUBRESOURCE PerFrameMap = {};
        if (SUCCEEDED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &PerFrameMap)))
        {
            FPerFrameConstants PerFrameConstants = {};
            DirectX::XMStoreFloat4x4(&PerFrameConstants.ViewProj, ViewProj);
            PerFrameConstants.LightDirection = { 0.4f, 0.5f, -1.0f, 0.0f };
            std::memcpy(PerFrameMap.pData, &PerFrameConstants, sizeof(PerFrameConstants));
            Context->Unmap(PerFrameBuffer.Get(), 0);
        }

        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0);
        Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());

        Context->RSSetState(DefaultRasterizerState.Get());
        Context->OMSetDepthStencilState(DefaultDepthStencilState.Get(), 0);

        for (uint32_t MeshID = 0; MeshID < MAX_MESH_TYPES; ++MeshID)
        {
            const std::vector<uint32_t>& Objects = Buckets[MeshID];
            const FMeshResource& MeshResource = MeshResources[MeshID];
            if (Objects.empty() || !MeshResource.VertexBuffer || !MeshResource.IndexBuffer || MeshResource.IndexCount == 0) continue;

            D3D11_MAPPED_SUBRESOURCE MaterialMap = {};
            if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MaterialMap)))
            {
                FMaterialConstants MaterialConstants = { { 1.0f, 1.0f, 1.0f, 1.0f } };
                std::memcpy(MaterialMap.pData, &MaterialConstants, sizeof(MaterialConstants));
                Context->Unmap(MaterialBuffer.Get(), 0);
            }
            Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf());

            ID3D11ShaderResourceView* TextureView = MeshResource.DiffuseTextureView ? MeshResource.DiffuseTextureView.Get() : DefaultWhiteTextureView.Get();
            Context->PSSetShaderResources(0, 1, &TextureView);

            UINT Stride = sizeof(FMeshVertex), Offset = 0;
            Context->IASetVertexBuffers(0, 1, MeshResource.VertexBuffer.GetAddressOf(), &Stride, &Offset);
            Context->IASetIndexBuffer(MeshResource.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            for (uint32_t ObjectIndex : Objects)
            {
                // [성능 핵심] 더 이상의 Map 호출 없음. VSSetConstantBuffers1 오프셋만 교체.
                if (Context1)
                {
                    UINT OffsetInConstants = ObjectOffsets[ObjectIndex] / 16;
                    UINT ConstantsCount = AlignedConstantSize / 16;
                    Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &OffsetInConstants, &ConstantsCount);
                }
                else
                {
                    Context->VSSetConstantBuffers(1, 1, PerObjectBuffer.GetAddressOf());
                }

                Context->DrawIndexed(MeshResource.IndexCount, 0, 0);
            }
        }
    }

    void URenderer::EndFrame()
    {
        SwapChain->Present(0, 0);
    }
}
