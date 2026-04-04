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
#include <unordered_map>
#include <vector>

namespace Graphics
{
    namespace
    {
        struct FPerFrameConstants
        {
            DirectX::XMFLOAT4X4 ViewProj;
        };

        struct FPerObjectConstants
        {
            DirectX::XMFLOAT4 Row0;
            DirectX::XMFLOAT4 Row1;
            DirectX::XMFLOAT4 Row2;
            DirectX::XMFLOAT4 ColorModifier;
        };

        struct FMaterialConstants
        {
            DirectX::XMFLOAT4 BaseColor;
        };

        struct FObjVertexKey
        {
            int PositionIndex = -1;
            int TexCoordIndex = -1;
            int NormalIndex = -1;

            bool operator==(const FObjVertexKey& InOther) const
            {
                return PositionIndex == InOther.PositionIndex &&
                    TexCoordIndex == InOther.TexCoordIndex &&
                    NormalIndex == InOther.NormalIndex;
            }
        };

        struct FObjVertexKeyHasher
        {
            size_t operator()(const FObjVertexKey& InKey) const noexcept
            {
                const size_t PositionHash = std::hash<int>{}(InKey.PositionIndex);
                const size_t TexCoordHash = std::hash<int>{}(InKey.TexCoordIndex);
                const size_t NormalHash = std::hash<int>{}(InKey.NormalIndex);
                return PositionHash ^ (TexCoordHash << 1) ^ (NormalHash << 2);
            }
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
            std::vector<URenderer::FMeshVertex>& OutVertices,
            std::vector<uint32_t>& OutIndices,
            std::wstring& OutDiffuseTexturePath)
        {
            std::ifstream File{ std::filesystem::path(InPath) };
            if (!File) return false;

            std::vector<DirectX::XMFLOAT3> Positions;
            std::vector<DirectX::XMFLOAT3> Normals;
            std::vector<DirectX::XMFLOAT2> TexCoords;
            std::unordered_map<FObjVertexKey, uint32_t, FObjVertexKeyHasher> VertexMap;
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
                            FObjVertexKey Key = {};
                            if (!ParseObjFaceIndices(FaceToken, Key.PositionIndex, Key.TexCoordIndex, Key.NormalIndex)) continue;
                            if (Key.PositionIndex < 0 || static_cast<size_t>(Key.PositionIndex) >= Positions.size()) continue;

                            const auto ExistingVertex = VertexMap.find(Key);
                            if (ExistingVertex != VertexMap.end())
                            {
                                OutIndices.push_back(ExistingVertex->second);
                                continue;
                            }

                            URenderer::FMeshVertex Vertex = {};
                            Vertex.Position = Positions[Key.PositionIndex];
                            Vertex.Normal = (Key.NormalIndex >= 0 && static_cast<size_t>(Key.NormalIndex) < Normals.size())
                                ? Normals[Key.NormalIndex]
                                : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
                            Vertex.TexCoord = (Key.TexCoordIndex >= 0 && static_cast<size_t>(Key.TexCoordIndex) < TexCoords.size())
                                ? TexCoords[Key.TexCoordIndex]
                                : DirectX::XMFLOAT2{ 0.0f, 0.0f };

                            const uint32_t VertexIndex = static_cast<uint32_t>(OutVertices.size());
                            OutVertices.push_back(Vertex);
                            OutIndices.push_back(VertexIndex);
                            VertexMap.emplace(Key, VertexIndex);
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

        bool LoadMeshResource(
            ID3D11Device* InDevice,
            const std::wstring& InPath,
            URenderer::FMeshResource& OutMeshResource)
        {
            if (!LoadObjMeshData(InPath, OutMeshResource.SourceVertices, OutMeshResource.SourceIndices, OutMeshResource.DiffuseTexturePath))
            {
                return false;
            }

            D3D11_BUFFER_DESC VertexBufferDesc = {};
            VertexBufferDesc.ByteWidth = static_cast<UINT>(OutMeshResource.SourceVertices.size() * sizeof(URenderer::FMeshVertex));
            VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA VertexData = { OutMeshResource.SourceVertices.data(), 0, 0 };
            if (FAILED(InDevice->CreateBuffer(&VertexBufferDesc, &VertexData, &OutMeshResource.VertexBuffer))) return false;

            D3D11_BUFFER_DESC IndexBufferDesc = {};
            IndexBufferDesc.ByteWidth = static_cast<UINT>(OutMeshResource.SourceIndices.size() * sizeof(uint32_t));
            IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
            IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            D3D11_SUBRESOURCE_DATA IndexData = { OutMeshResource.SourceIndices.data(), 0, 0 };
            if (FAILED(InDevice->CreateBuffer(&IndexBufferDesc, &IndexData, &OutMeshResource.IndexBuffer))) return false;

            if (!OutMeshResource.DiffuseTexturePath.empty() &&
                !LoadTextureWithWIC(InDevice, OutMeshResource.DiffuseTexturePath, OutMeshResource.DiffuseTextureView))
            {
                return false;
            }

            OutMeshResource.IndexCount = static_cast<uint32_t>(OutMeshResource.SourceIndices.size());
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
            };

            cbuffer PerObject : register(b1)
            {
                float4 Row0;
                float4 Row1;
                float4 Row2;
                float4 ColorModifier;
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
                float2 TexCoord : TEXCOORD0;
                float HighlightRed : COLOR;
            };

            PS_IN VSMain(VS_IN i)
            {
                PS_IN o;
                float4 LocalPos = float4(i.Pos, 1.0f);
                float3 WorldPos = float3(dot(LocalPos, Row0), dot(LocalPos, Row1), dot(LocalPos, Row2));
                o.Pos = mul(float4(WorldPos, 1.0f), ViewProj);
                o.TexCoord = i.TexCoord;
                
                o.HighlightRed = ColorModifier.x; 
                return o;
            }

            float4 PSMain(PS_IN i) : SV_TARGET
            {
                float4 BaseColorTarget = DiffuseTexture.Sample(DiffuseSampler, i.TexCoord) * BaseColor;
                return BaseColorTarget + float4(i.HighlightRed, 0.0f, 0.0f, 0.0f); 
            }
        )";

        ComPtr<ID3DBlob> VS, PS, Err;
        if (FAILED(D3DCompile(ShaderSrc, std::strlen(ShaderSrc), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err))) return false;
        if (FAILED(D3DCompile(ShaderSrc, std::strlen(ShaderSrc), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, &Err))) return false;
        if (FAILED(Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &VertexShader))) return false;
        if (FAILED(Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &PixelShader))) return false;

        D3D11_INPUT_ELEMENT_DESC Layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(Device->CreateInputLayout(Layout, static_cast<UINT>(std::size(Layout)), VS->GetBufferPointer(), VS->GetBufferSize(), &InputLayout))) return false;

        D3D11_BUFFER_DESC PerFrameDesc = { sizeof(FPerFrameConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        if (FAILED(Device->CreateBuffer(&PerFrameDesc, nullptr, &PerFrameBuffer))) return false;

        D3D11_BUFFER_DESC PerObjectDesc = { 64 * 1024 * 1024, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        if (FAILED(Device->CreateBuffer(&PerObjectDesc, nullptr, &PerObjectBuffer))) return false;

        D3D11_BUFFER_DESC MaterialDesc = { sizeof(FMaterialConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        if (FAILED(Device->CreateBuffer(&MaterialDesc, nullptr, &MaterialBuffer))) return false;

        D3D11_SAMPLER_DESC SamplerDesc = {};
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        SamplerDesc.MinLOD = 0.0f; SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(Device->CreateSamplerState(&SamplerDesc, &DiffuseSamplerState))) return false;

        D3D11_RASTERIZER_DESC RasterizerDesc = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
        if (FAILED(Device->CreateRasterizerState(&RasterizerDesc, &DefaultRasterizerState))) return false;

        D3D11_DEPTH_STENCIL_DESC DepthDesc = { TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL, FALSE, 0, 0, {}, {} };
        if (FAILED(Device->CreateDepthStencilState(&DepthDesc, &DefaultDepthStencilState))) return false;

        if (!CreateSolidTexture(Device.Get(), { 1.0f, 1.0f, 1.0f, 1.0f }, DefaultWhiteTextureView)) return false;

        const std::wstring MeshBasePath = Core::FPathManager::GetMeshPath();
        if (!LoadMeshResource(Device.Get(), MeshBasePath + L"apple_mid.obj", MeshResources[0])) return false;
        if (!LoadMeshResource(Device.Get(), MeshBasePath + L"bitten_apple_mid.obj", MeshResources[1])) return false;
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
        D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (float)ViewportWidth, (float)ViewportHeight, 0.0f, 1.0f };
        Context->RSSetViewports(1, &Viewport);
    }

    void URenderer::RenderScene(const Scene::USceneManager& InSceneManager)
    {
        const Scene::FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        const Scene::FSceneSelectionData& Selection = InSceneManager.GetSelectionData();
        const uint32_t SelectedObjID = Selection.bHasSelection ? Selection.ObjectIndex : 0xFFFFFFFF;
        if (!SceneData || !PerFrameBuffer || !PerObjectBuffer || !MaterialBuffer) return;

        const uint32_t SourceCount = SceneData->RenderCount;
        if (SourceCount == 0) return;

        const uint32_t AlignedConstantSize = 256;
        const uint32_t BulkSize = SourceCount * AlignedConstantSize;
        const uint32_t BufferCapacity = 64 * 1024 * 1024;

        D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (PerObjectRingBufferOffset + BulkSize > BufferCapacity) { MapType = D3D11_MAP_WRITE_DISCARD; PerObjectRingBufferOffset = 0; }

        D3D11_MAPPED_SUBRESOURCE PerObjectMap = {};
        if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, MapType, 0, &PerObjectMap))) return;
        uint8_t* DestStart = static_cast<uint8_t*>(PerObjectMap.pData) + PerObjectRingBufferOffset;
        const uint32_t BaseFrameOffset = PerObjectRingBufferOffset;

        static uint32_t SortedQueue[Scene::FSceneDataSOA::MAX_OBJECTS];
        uint32_t MeshCounts[MAX_MESH_TYPES] = { 0 }, MeshOffsets[MAX_MESH_TYPES] = { 0 };
        for (uint32_t i = 0; i < SourceCount; ++i) { uint32_t mid = SceneData->MeshIDs[SceneData->RenderQueue[i]]; if (mid < MAX_MESH_TYPES) MeshCounts[mid]++; }
        uint32_t cur = 0; for (uint32_t i = 0; i < MAX_MESH_TYPES; ++i) { MeshOffsets[i] = cur; cur += MeshCounts[i]; }
        uint32_t TempOffsets[MAX_MESH_TYPES]; std::memcpy(TempOffsets, MeshOffsets, sizeof(MeshOffsets));
        for (uint32_t i = 0; i < SourceCount; ++i)
        {
            uint32_t oid = SceneData->RenderQueue[i], mid = SceneData->MeshIDs[oid];
            if (mid < MAX_MESH_TYPES) {
                uint32_t sidx = TempOffsets[mid]++; SortedQueue[sidx] = oid;
                const Math::FPacked3x4Matrix& mat = SceneData->WorldMatrices[oid];
                FPerObjectConstants* dest = reinterpret_cast<FPerObjectConstants*>(DestStart + (sidx * AlignedConstantSize));
                DirectX::XMStoreFloat4(&dest->Row0, mat.Row0); DirectX::XMStoreFloat4(&dest->Row1, mat.Row1);
                DirectX::XMStoreFloat4(&dest->Row2, mat.Row2);
                dest->ColorModifier = (oid == SelectedObjID)
                    ? DirectX::XMFLOAT4{ 0.5f, 0.0f, 0.0f, 0.0f }
                : DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
            }
        }
        Context->Unmap(PerObjectBuffer.Get(), 0);
        PerObjectRingBufferOffset += BulkSize;

        float aspect = (ViewportHeight == 0) ? 1.0f : (float)ViewportWidth / (float)ViewportHeight;
        DirectX::XMVECTOR camPos = DirectX::XMLoadFloat3(&CameraState.Position);
        DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorSet(std::cos(CameraState.PitchRadians) * std::cos(CameraState.YawRadians), std::cos(CameraState.PitchRadians) * std::sin(CameraState.YawRadians), std::sin(CameraState.PitchRadians), 0.0f));
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(camPos, DirectX::XMVectorAdd(camPos, forward), DirectX::XMVectorSet(0, 0, 1, 0));
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(CameraState.FOVDegrees), aspect, CameraState.NearClip, CameraState.FarClip);

        D3D11_MAPPED_SUBRESOURCE pfmap = {};
        if (SUCCEEDED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &pfmap))) {
            FPerFrameConstants pf = {}; DirectX::XMStoreFloat4x4(&pf.ViewProj, view * proj);
            std::memcpy(pfmap.pData, &pf, sizeof(pf)); Context->Unmap(PerFrameBuffer.Get(), 0);
        }

        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0); Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());
        Context->RSSetState(DefaultRasterizerState.Get()); Context->OMSetDepthStencilState(DefaultDepthStencilState.Get(), 0);

        for (uint32_t mid = 0; mid < MAX_MESH_TYPES; ++mid)
        {
            if (MeshCounts[mid] == 0) continue;
            const FMeshResource& res = MeshResources[mid];
            if (!res.VertexBuffer || !res.IndexBuffer) continue;

            D3D11_MAPPED_SUBRESOURCE matmap = {};
            if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matmap))) {
                FMaterialConstants mc = { { 1, 1, 1, 1 } }; std::memcpy(matmap.pData, &mc, sizeof(mc));
                Context->Unmap(MaterialBuffer.Get(), 0);
            }
            Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf());
            ID3D11ShaderResourceView* srv = res.DiffuseTextureView ? res.DiffuseTextureView.Get() : DefaultWhiteTextureView.Get();
            Context->PSSetShaderResources(0, 1, &srv);
            UINT stride = sizeof(FMeshVertex), offset = 0;
            Context->IASetVertexBuffers(0, 1, res.VertexBuffer.GetAddressOf(), &stride, &offset);
            Context->IASetIndexBuffer(res.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            for (uint32_t i = MeshOffsets[mid]; i < MeshOffsets[mid] + MeshCounts[mid]; ++i) {
                if (Context1) {
                    UINT off = (BaseFrameOffset + (i * AlignedConstantSize)) / 16, cnt = AlignedConstantSize / 16;
                    Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                }
                else { Context->VSSetConstantBuffers(1, 1, PerObjectBuffer.GetAddressOf()); }
                Context->DrawIndexed(res.IndexCount, 0, 0);
            }
        }
    }

    void URenderer::EndFrame() { SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING); }
}
