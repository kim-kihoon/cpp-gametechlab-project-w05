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

        // ... (기존 SwapChain 설정 생략)
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

        ComPtr<ID3D11Texture2D> DepthBuffer;
        {
            D3D11_TEXTURE2D_DESC DepthDesc = {};
            DepthDesc.Width = InWidth;
            DepthDesc.Height = InHeight;
            DepthDesc.MipLevels = 1;
            DepthDesc.ArraySize = 1;
            DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;   // SRV+DSV 동시 사용
            DepthDesc.SampleDesc.Count = 1;
            DepthDesc.Usage = D3D11_USAGE_DEFAULT;
            DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer))) return false;
        }

        // DSV: D32_FLOAT로 해석
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
            DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            DSVDesc.Texture2D.MipSlice = 0;
            if (FAILED(Device->CreateDepthStencilView(DepthBuffer.Get(), &DSVDesc, &DepthStencilView))) return false;
        }

        // SRV: R32_FLOAT로 해석 → CS에서 직접 읽기 가능
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.MipLevels = 1;
            if (FAILED(Device->CreateShaderResourceView(DepthBuffer.Get(), &SRVDesc, &DepthCopySRV))) return false;
        }

        // HUD 초기화
        HUD = std::make_unique<FHUD>();
        if (!HUD->Initialize(Device.Get(), Context.Get())) return false;

        if (!CreateDefaultResources()) return false;

        InitHiZResources(ViewportWidth, ViewportHeight);

        return true;
    }

    void URenderer::RenderHUD()
    {
        if (HUD)
        {
            HUD->Update(CurrentMetrics, ViewportWidth, ViewportHeight);
            HUD->Render();
        }
    }

    void URenderer::Resize(int Width, int Height)
    {
        if (Width == 0 || Height == 0 || !SwapChain) return;

        ViewportWidth = static_cast<uint32_t>(Width);
        ViewportHeight = static_cast<uint32_t>(Height);

        Context->OMSetRenderTargets(0, nullptr, nullptr);
        MainRenderTargetView.Reset();
        DepthStencilView.Reset();

        if (FAILED(SwapChain->ResizeBuffers(0, ViewportWidth, ViewportHeight, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)))
        {
            return;
        }

        ComPtr<ID3D11Texture2D> BackBuffer;
        if (SUCCEEDED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer))))
        {
            Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView);
        }

        ComPtr<ID3D11Texture2D> DepthBuffer;
        {
            D3D11_TEXTURE2D_DESC DepthDesc = {};
            DepthDesc.Width = ViewportWidth;
            DepthDesc.Height = ViewportHeight;
            DepthDesc.MipLevels = 1;
            DepthDesc.ArraySize = 1;
            DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            DepthDesc.SampleDesc.Count = 1;
            DepthDesc.Usage = D3D11_USAGE_DEFAULT;
            DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer))) return;
        }
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
            DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            DSVDesc.Texture2D.MipSlice = 0;
            Device->CreateDepthStencilView(DepthBuffer.Get(), &DSVDesc, &DepthStencilView);
        }
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.MipLevels = 1;
            Device->CreateShaderResourceView(DepthBuffer.Get(), &SRVDesc, &DepthCopySRV);
        }

        // Hi-Z 리소스도 재생성
        HiZTexture.Reset();
        HiZSRV.Reset();
        HiZMipUAVs.clear();
        HiZMipSRVs.clear();
        InitHiZResources(ViewportWidth, ViewportHeight);
    }

    const URenderer::FMeshResource* URenderer::GetMeshResource(uint32_t MeshID) const
    {
        if (MeshID < MAX_MESH_TYPES) return &MeshResources[MeshID];
        return nullptr;
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

        // ── Hi-Z Build CS ─────────────────────────────────────────────────────────
        const char* HiZBuildSrc = R"(
            Texture2D<float>   SrcDepth : register(t0);
            RWTexture2D<float> DstMip   : register(u0);

            cbuffer MipParams : register(b0)
            {
                uint SrcW;
                uint SrcH;
                uint2 _pad;
            };

            [numthreads(8, 8, 1)]
            void CSBuildHiZ(uint3 DTid : SV_DispatchThreadID)
            {
                uint2 dst = DTid.xy;
                uint2 src = dst * 2;

                float d0 = SrcDepth.Load(int3(src + uint2(0, 0), 0));
                float d1 = SrcDepth.Load(int3(src + uint2(1, 0), 0));
                float d2 = SrcDepth.Load(int3(src + uint2(0, 1), 0));
                float d3 = SrcDepth.Load(int3(src + uint2(1, 1), 0));

                // 경계 밖 텍셀은 0으로 처리 (src 크기 초과 방지)
                if (src.x + 1 >= SrcW) { d1 = d0; d3 = d2; }
                if (src.y + 1 >= SrcH) { d2 = d0; d3 = d1; }

                DstMip[dst] = max(max(d0, d1), max(d2, d3));
            }
        )";

        ComPtr<ID3DBlob> HiZBuildBlob, HiZBuildErr;
        if (FAILED(D3DCompile(HiZBuildSrc, strlen(HiZBuildSrc), "HiZBuild", nullptr, nullptr,
            "CSBuildHiZ", "cs_5_0", 0, 0, &HiZBuildBlob, &HiZBuildErr)))
        {
            if (HiZBuildErr) OutputDebugStringA((char*)HiZBuildErr->GetBufferPointer());
            return false;
        }
        if (FAILED(Device->CreateComputeShader(HiZBuildBlob->GetBufferPointer(),
            HiZBuildBlob->GetBufferSize(), nullptr, &CSBuildHiZ))) return false;

        // ── Occlusion Test CS ──────────────────────────────────────────────────────
        const char* HiZCullSrc = R"(
            struct FObjectBounds
            {
                float3   BoundsMin;
                uint     ObjectIndex;
                float3   BoundsMax;
                uint     _pad;
            };

            cbuffer CullParams : register(b0)
            {
                row_major float4x4 ViewProj;
                uint  ObjectCount;
                uint  HiZMipLevels;
                float HiZTexelWidth;
                float HiZTexelHeight;
            };

            StructuredBuffer<FObjectBounds>  InBounds        : register(t0);
            Texture2D<float>                 HiZTexture      : register(t1);
            SamplerState                     PointClampSamp  : register(s0);
            RWStructuredBuffer<uint>         VisibilityFlags : register(u0);

            [numthreads(64, 1, 1)]
            void CSTestOcclusion(uint3 DTid : SV_DispatchThreadID)
            {
                uint idx = DTid.x;
                if (idx >= ObjectCount) return;

                FObjectBounds b = InBounds[idx];

                float2 ndcMin =  1.0f;
                float2 ndcMax = -1.0f;
                float  minZ   =  1.0f;

                [unroll]
                for (uint i = 0; i < 8; ++i)
                {
                    float3 corner = float3(
                        (i & 1) ? b.BoundsMax.x : b.BoundsMin.x,
                        (i & 2) ? b.BoundsMax.y : b.BoundsMin.y,
                        (i & 4) ? b.BoundsMax.z : b.BoundsMin.z
                    );
                    float4 clip = mul(float4(corner, 1.0f), ViewProj);

                    // near plane 뒤 코너 → 보수적으로 전체 화면 visible 처리
                    if (clip.w < 1e-5f)
                    {
                        VisibilityFlags[b.ObjectIndex] = 1;
                        return;
                    }

                    float3 ndc = clip.xyz / clip.w;
                    ndcMin = min(ndcMin, ndc.xy);
                    ndcMax = max(ndcMax, ndc.xy);
                    minZ   = min(minZ,   ndc.z);
                }

                // 화면 밖으로 완전히 나간 경우
                if (any(ndcMax < -1.0f) || any(ndcMin > 1.0f))
                {
                    VisibilityFlags[b.ObjectIndex] = 0;
                    return;
                }

                ndcMin = clamp(ndcMin, -1.0f, 1.0f);
                ndcMax = clamp(ndcMax, -1.0f, 1.0f);

                // NDC → UV (D3D11: Y축 반전)
                float2 uvMin = float2( ndcMin.x * 0.5f + 0.5f, -ndcMax.y * 0.5f + 0.5f);
                float2 uvMax = float2( ndcMax.x * 0.5f + 0.5f, -ndcMin.y * 0.5f + 0.5f);

                // 화면 점유 크기로 mip 선택
                float2 sizeUV = uvMax - uvMin;

                // 수정: AABB가 커버하는 픽셀 수의 log2 → 해당 크기를 완전히 커버하는 mip
                float  sizeX  = sizeUV.x * (float)HiZTexelWidth;   // 픽셀 단위 width
                float  sizeY  = sizeUV.y * (float)HiZTexelHeight;  // 픽셀 단위 height
                float  texels = max(sizeX, sizeY);
                uint   mip    = (uint)clamp(floor(log2(texels)), 0.0f, (float)HiZMipLevels);

                // 4코너 샘플 → 보수적 최대값
                float d0 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMin.x, uvMin.y), mip);
                float d1 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMax.x, uvMin.y), mip);
                float d2 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMin.x, uvMax.y), mip);
                float d3 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMax.x, uvMax.y), mip);
                float occluderDepth = max(max(d0, d1), max(d2, d3));

                // 비교: AABB 가장 가까운 점이 occluder보다 뒤에 있으면 culled
                VisibilityFlags[b.ObjectIndex] = (minZ > occluderDepth) ? 0u : 1u;
            }
        )";

        ComPtr<ID3DBlob> HiZCullBlob, HiZCullErr;
        if (FAILED(D3DCompile(HiZCullSrc, strlen(HiZCullSrc), "HiZCull", nullptr, nullptr,
            "CSTestOcclusion", "cs_5_0", 0, 0, &HiZCullBlob, &HiZCullErr)))
        {
            if (HiZCullErr) OutputDebugStringA((char*)HiZCullErr->GetBufferPointer());
            return false;
        }
        if (FAILED(Device->CreateComputeShader(HiZCullBlob->GetBufferPointer(),
            HiZCullBlob->GetBufferSize(), nullptr, &CSTestOcclusion))) return false;

        return true;
    }

    void URenderer::InitHiZResources(uint32_t Width, uint32_t Height)
    {
        HiZWidth = Width;
        HiZHeight = Height;
        HiZMipCount = 1;
        uint32_t sz = std::max<uint32_t>(Width, Height);
        while (sz > 1) { sz >>= 1; ++HiZMipCount; }

        // ── Hi-Z Texture (R32_FLOAT, 모든 mip에 UAV/SRV) ─────────────────────────
        {
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = Width;
            td.Height = Height;
            td.MipLevels = HiZMipCount;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R32_FLOAT;
            td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            Device->CreateTexture2D(&td, nullptr, &HiZTexture);
        }

        // 전체 SRV (Occlusion test CS에서 mip 선택 샘플링용)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
            srvd.Format = DXGI_FORMAT_R32_FLOAT;
            srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvd.Texture2D.MostDetailedMip = 0;
            srvd.Texture2D.MipLevels = HiZMipCount;
            Device->CreateShaderResourceView(HiZTexture.Get(), &srvd, &HiZSRV);
        }

        // mip별 UAV & SRV
        HiZMipUAVs.resize(HiZMipCount);
        HiZMipSRVs.resize(HiZMipCount);
        for (uint32_t m = 0; m < HiZMipCount; ++m)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
            uavd.Format = DXGI_FORMAT_R32_FLOAT;
            uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            uavd.Texture2D.MipSlice = m;
            Device->CreateUnorderedAccessView(HiZTexture.Get(), &uavd, &HiZMipUAVs[m]);

            D3D11_SHADER_RESOURCE_VIEW_DESC msrvd = {};
            msrvd.Format = DXGI_FORMAT_R32_FLOAT;
            msrvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            msrvd.Texture2D.MostDetailedMip = m;
            msrvd.Texture2D.MipLevels = 1;
            Device->CreateShaderResourceView(HiZTexture.Get(), &msrvd, &HiZMipSRVs[m]);
        }

        // ── Bounds StructuredBuffer ───────────────────────────────────────────────
        {
            constexpr uint32_t MaxObj = Scene::FSceneDataSOA::MAX_OBJECTS;

            D3D11_BUFFER_DESC bbd = {};
            bbd.ByteWidth = sizeof(FObjectBoundsGPU) * MaxObj;
            bbd.Usage = D3D11_USAGE_DYNAMIC;
            bbd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            bbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            bbd.StructureByteStride = sizeof(FObjectBoundsGPU);
            Device->CreateBuffer(&bbd, nullptr, &BoundsBuffer);

            D3D11_SHADER_RESOURCE_VIEW_DESC bsrvd = {};
            bsrvd.Format = DXGI_FORMAT_UNKNOWN;
            bsrvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            bsrvd.Buffer.FirstElement = 0;
            bsrvd.Buffer.NumElements = MaxObj;
            Device->CreateShaderResourceView(BoundsBuffer.Get(), &bsrvd, &BoundsSRV);
        }

        // ── Visibility RWStructuredBuffer ─────────────────────────────────────────
        {
            constexpr uint32_t MaxObj = Scene::FSceneDataSOA::MAX_OBJECTS;

            D3D11_BUFFER_DESC vbd = {};
            vbd.ByteWidth = sizeof(uint32_t) * MaxObj;
            vbd.Usage = D3D11_USAGE_DEFAULT;
            vbd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            vbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            vbd.StructureByteStride = sizeof(uint32_t);
            Device->CreateBuffer(&vbd, nullptr, &VisibilityBuffer);

            D3D11_UNORDERED_ACCESS_VIEW_DESC vuavd = {};
            vuavd.Format = DXGI_FORMAT_UNKNOWN;
            vuavd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            vuavd.Buffer.FirstElement = 0;
            vuavd.Buffer.NumElements = MaxObj;
            Device->CreateUnorderedAccessView(VisibilityBuffer.Get(), &vuavd, &VisibilityUAV);

            // CPU Readback용 Staging
            D3D11_BUFFER_DESC sbd = vbd;
            sbd.Usage = D3D11_USAGE_STAGING;
            sbd.BindFlags = 0;
            sbd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sbd.MiscFlags = 0;
            Device->CreateBuffer(&sbd, nullptr, &VisibilityStagingBuffers[0]);
            Device->CreateBuffer(&sbd, nullptr, &VisibilityStagingBuffers[1]);
        }

        // ── CullParam / HiZBuildParam cbuffer ────────────────────────────────────
        {
            D3D11_BUFFER_DESC cbd = {};
            cbd.ByteWidth = 256;
            cbd.Usage = D3D11_USAGE_DYNAMIC;
            cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            Device->CreateBuffer(&cbd, nullptr, &CullParamBuffer);
            Device->CreateBuffer(&cbd, nullptr, &HiZBuildParamBuffer);
        }

        // ── PointClamp Sampler ────────────────────────────────────────────────────
        {
            D3D11_SAMPLER_DESC sd = {};
            sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd.MaxLOD = D3D11_FLOAT32_MAX;
            sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
            Device->CreateSamplerState(&sd, &PointClampSamplerState);
        }
    }

    void URenderer::BuildHiZMips()
    {
        // DSV 해제 (같은 텍스처를 SRV로 읽기 위해)
        ID3D11RenderTargetView* nullRTV = nullptr;
        Context->OMSetRenderTargets(0, &nullRTV, nullptr);

        // Depth → HiZTexture mip0 복사
        {
            ID3D11Resource* depthRes = nullptr;
            DepthCopySRV->GetResource(&depthRes);
            Context->CopySubresourceRegion(
                HiZTexture.Get(), 0, 0, 0, 0,
                depthRes, 0, nullptr);
            depthRes->Release();
        }

        Context->CSSetShader(CSBuildHiZ.Get(), nullptr, 0);

        for (uint32_t m = 1; m < HiZMipCount; ++m)
        {
            const uint32_t SrcW = std::max<uint32_t>(1u, HiZWidth >> (m - 1));
            const uint32_t SrcH = std::max<uint32_t>(1u, HiZHeight >> (m - 1));
            const uint32_t DstW = std::max<uint32_t>(1u, HiZWidth >> m);
            const uint32_t DstH = std::max<uint32_t>(1u, HiZHeight >> m);

            // MipParams 업데이트
            {
                struct FMipParams { uint32_t SrcW, SrcH, _pad[2]; };
                D3D11_MAPPED_SUBRESOURCE mr = {};
                if (SUCCEEDED(Context->Map(HiZBuildParamBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr)))
                {
                    FMipParams p = { SrcW, SrcH, {0, 0} };
                    memcpy(mr.pData, &p, sizeof(p));
                    Context->Unmap(HiZBuildParamBuffer.Get(), 0);
                }
            }

            // m==1일 때는 DepthCopySRV에서 직접 읽기 (HiZTexture mip0 SRV 충돌 방지)
            ID3D11ShaderResourceView* srcSRV = (m == 1)
                ? DepthCopySRV.Get()
                : HiZMipSRVs[m - 1].Get();

            Context->CSSetConstantBuffers(0, 1, HiZBuildParamBuffer.GetAddressOf());
            Context->CSSetShaderResources(0, 1, &srcSRV);
            Context->CSSetUnorderedAccessViews(0, 1, HiZMipUAVs[m].GetAddressOf(), nullptr);

            Context->Dispatch((DstW + 7) / 8, (DstH + 7) / 8, 1);

            ID3D11ShaderResourceView* nullSRV = nullptr;
            ID3D11UnorderedAccessView* nullUAV = nullptr;
            Context->CSSetShaderResources(0, 1, &nullSRV);
            Context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        }

        Context->CSSetShader(nullptr, nullptr, 0);
    }

    void URenderer::RunOcclusionCull(Scene::FSceneDataSOA* SceneData, const DirectX::XMMATRIX& ViewProj, std::array<bool, Scene::FSceneDataSOA::MAX_OBJECTS>& OutIsVisible)
    {
        const uint32_t Count = SceneData->RenderCount;
        if (Count == 0) return;

        // ── 1. Bounds 버퍼 업데이트 ──────────────────────────────────────────────
        {
            D3D11_MAPPED_SUBRESOURCE mr = {};
            if (FAILED(Context->Map(BoundsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr)))
                return;

            auto* dst = static_cast<FObjectBoundsGPU*>(mr.pData);
            for (uint32_t i = 0; i < Count; ++i)
            {
                const uint32_t oid = SceneData->RenderQueue[i];
                dst[i].BoundsMin = { SceneData->MinX[oid],
                                       SceneData->MinY[oid],
                                       SceneData->MinZ[oid] };
                dst[i].BoundsMax = { SceneData->MaxX[oid],
                                       SceneData->MaxY[oid],
                                       SceneData->MaxZ[oid] };
                dst[i].ObjectIndex = oid;
                dst[i]._pad = 0;
            }
            Context->Unmap(BoundsBuffer.Get(), 0);
        }

        // ── 2. CullParams 업데이트 ───────────────────────────────────────────────
        {
            struct alignas(16) FCullParams
            {
                DirectX::XMFLOAT4X4 ViewProj;
                uint32_t             ObjectCount;
                uint32_t             HiZMipLevels;
                float                HiZTexelWidth;
                float                HiZTexelHeight;
            };

            D3D11_MAPPED_SUBRESOURCE mr = {};
            if (SUCCEEDED(Context->Map(CullParamBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr)))
            {
                auto* p = static_cast<FCullParams*>(mr.pData);
                DirectX::XMStoreFloat4x4(&p->ViewProj, ViewProj);
                p->ObjectCount = Count;
                p->HiZMipLevels = HiZMipCount - 1;
                p->HiZTexelWidth = static_cast<float>(HiZWidth);
                p->HiZTexelHeight = static_cast<float>(HiZHeight);
                Context->Unmap(CullParamBuffer.Get(), 0);
            }
        }

        // ── 3. Compute Shader 실행 ───────────────────────────────────────────────
        Context->CSSetShader(CSTestOcclusion.Get(), nullptr, 0);
        Context->CSSetConstantBuffers(0, 1, CullParamBuffer.GetAddressOf());
        Context->CSSetShaderResources(0, 1, BoundsSRV.GetAddressOf());
        Context->CSSetShaderResources(1, 1, HiZSRV.GetAddressOf());
        Context->CSSetSamplers(0, 1, PointClampSamplerState.GetAddressOf());
        UINT clearValue[4] = { 0, 0, 0, 0 };
        Context->ClearUnorderedAccessViewUint(VisibilityUAV.Get(), clearValue);
        Context->CSSetUnorderedAccessViews(0, 1, VisibilityUAV.GetAddressOf(), nullptr);
        Context->Dispatch((Count + 63) / 64, 1, 1);

        // 바인딩 해제
        {
            ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
            ID3D11UnorderedAccessView* nullUAV = nullptr;
            Context->CSSetShaderResources(0, 2, nullSRV);
            Context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
            Context->CSSetShader(nullptr, nullptr, 0);
        }

        // ── 4. GPU → CPU Readback ────────────────────────────────────────────────
        Context->CopyResource(VisibilityStagingBuffers[StagingWriteIndex].Get(),
            VisibilityBuffer.Get());

        // 첫 프레임은 이전 결과가 없으므로 전부 Visible 처리 (Staging 버퍼 스왑만)
        if (bFirstFrame)
        {
            bFirstFrame = false;
            // 첫 프레임은 전부 visible로 초기화
            for (uint32_t i = 0; i < Count; ++i)
                OutIsVisible[SceneData->RenderQueue[i]] = true;
            std::swap(StagingReadIndex, StagingWriteIndex);
            return;
        }

        // ── 6. 이전 프레임 결과로 RenderQueue 압축 (GPU stall 없음) ───────────────
        D3D11_MAPPED_SUBRESOURCE mr = {};
        if (FAILED(Context->Map(VisibilityStagingBuffers[StagingReadIndex].Get(),
            0, D3D11_MAP_READ, 0, &mr)))
        {
            std::swap(StagingReadIndex, StagingWriteIndex);
            return;
        }

        // ── 기존 압축 루프 전체를 교체 ───────────────────────────────────────────
        const uint32_t* flags = static_cast<const uint32_t*>(mr.pData);

        // RenderCount 압축 안 함, OutIsVisible에만 반영
        for (uint32_t i = 0; i < Count; ++i)
        {
            const uint32_t oid = SceneData->RenderQueue[i];
            if (flags[oid] != 0)
                OutIsVisible[oid] = true;  // invisible → visible 승격
        }

        Context->Unmap(VisibilityStagingBuffers[StagingReadIndex].Get(), 0);
        std::swap(StagingReadIndex, StagingWriteIndex);
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
        Scene::FSceneDataSOA* SceneData =
            const_cast<Scene::FSceneDataSOA*>(InSceneManager.GetSceneData());

        const Scene::FSceneSelectionData& Selection = InSceneManager.GetSelectionData();
        const uint32_t SelectedObjID = Selection.bHasSelection ? Selection.ObjectIndex : 0xFFFFFFFF;

        if (!SceneData || !PerFrameBuffer || !PerObjectBuffer || !MaterialBuffer) return;
        if (SceneData->RenderCount == 0) return;

        const uint32_t AlignedConstantSize = 256;
        const uint32_t BufferCapacity = 64 * 1024 * 1024;

        // ── ViewProj 계산 ─────────────────────────────────────────────────────────
        const float aspect = (ViewportHeight == 0)
            ? 1.0f : static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);

        DirectX::XMVECTOR camPos = DirectX::XMLoadFloat3(&CameraState.Position);
        DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorSet(
            std::cos(CameraState.PitchRadians) * std::cos(CameraState.YawRadians),
            std::cos(CameraState.PitchRadians) * std::sin(CameraState.YawRadians),
            std::sin(CameraState.PitchRadians), 0.0f));

        const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
            camPos, DirectX::XMVectorAdd(camPos, forward),
            DirectX::XMVectorSet(0, 0, 1, 0));
        const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(CameraState.FOVDegrees),
            aspect, CameraState.NearClip, CameraState.FarClip);
        const DirectX::XMMATRIX viewProj = view * proj;

        // PerFrameBuffer 업데이트
        {
            D3D11_MAPPED_SUBRESOURCE pfmap = {};
            if (SUCCEEDED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &pfmap)))
            {
                FPerFrameConstants pf = {};
                DirectX::XMStoreFloat4x4(&pf.ViewProj, viewProj);
                memcpy(pfmap.pData, &pf, sizeof(pf));
                Context->Unmap(PerFrameBuffer.Get(), 0);
            }
        }

        const uint32_t TotalCount = SceneData->RenderCount;

        auto t0 = std::chrono::high_resolution_clock::now();
        // ── 이전 프레임 결과로 Visible/Invisible 분리 ────────────────────────────
        static uint32_t PrevVisibleQueue[Scene::FSceneDataSOA::MAX_OBJECTS];
        static uint32_t PrevInvisibleQueue[Scene::FSceneDataSOA::MAX_OBJECTS];
        uint32_t PrevVisibleCount = 0;
        uint32_t PrevInvisibleCount = 0;

        for (uint32_t i = 0; i < TotalCount; ++i)
        {
            const uint32_t oid = SceneData->RenderQueue[i];
            if (!bHasPrevFrame || PrevIsVisible[oid])
                PrevVisibleQueue[PrevVisibleCount++] = oid;
            else
                PrevInvisibleQueue[PrevInvisibleCount++] = oid;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        // ── Pass 1: PrevVisible Depth Prepass ────────────────────────────────────
        {
            Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
            ID3D11RenderTargetView* nullRTV = nullptr;
            Context->OMSetRenderTargets(0, &nullRTV, DepthStencilView.Get());
            Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            Context->IASetInputLayout(InputLayout.Get());
            Context->VSSetShader(VertexShader.Get(), nullptr, 0);
            Context->PSSetShader(nullptr, nullptr, 0);
            Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
            Context->RSSetState(DefaultRasterizerState.Get());
            Context->OMSetDepthStencilState(DefaultDepthStencilState.Get(), 0);

            const uint32_t Bulk = PrevVisibleCount * AlignedConstantSize;
            D3D11_MAP MapType = D3D11_MAP_WRITE_NO_OVERWRITE;
            if (PerObjectRingBufferOffset + Bulk > BufferCapacity)
            {
                MapType = D3D11_MAP_WRITE_DISCARD;
                PerObjectRingBufferOffset = 0;
            }

            D3D11_MAPPED_SUBRESOURCE pm = {};
            if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, MapType, 0, &pm))) return;

            uint8_t* base = static_cast<uint8_t*>(pm.pData) + PerObjectRingBufferOffset;
            for (uint32_t i = 0; i < PrevVisibleCount; ++i)
            {
                const uint32_t oid = PrevVisibleQueue[i];
                const Math::FPacked3x4Matrix& mat = SceneData->WorldMatrices[oid];
                FPerObjectConstants* dest = reinterpret_cast<FPerObjectConstants*>(
                    base + i * AlignedConstantSize);
                DirectX::XMStoreFloat4(&dest->Row0, mat.Row0);
                DirectX::XMStoreFloat4(&dest->Row1, mat.Row1);
                DirectX::XMStoreFloat4(&dest->Row2, mat.Row2);
                dest->ColorModifier = { 0, 0, 0, 0 };
            }
            Context->Unmap(PerObjectBuffer.Get(), 0);

            const uint32_t Pass1Base = PerObjectRingBufferOffset;
            PerObjectRingBufferOffset += Bulk;

            for (uint32_t i = 0; i < PrevVisibleCount; ++i)
            {
                const uint32_t oid = PrevVisibleQueue[i];
                const uint32_t mid = SceneData->MeshIDs[oid];
                if (mid >= MAX_MESH_TYPES) continue;
                const FMeshResource& res = MeshResources[mid];
                if (!res.VertexBuffer || !res.IndexBuffer) continue;

                if (Context1)
                {
                    UINT off = (Pass1Base + i * AlignedConstantSize) / 16;
                    UINT cnt = AlignedConstantSize / 16;
                    Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                }
                UINT stride = sizeof(FMeshVertex), offset = 0;
                Context->IASetVertexBuffers(0, 1, res.VertexBuffer.GetAddressOf(), &stride, &offset);
                Context->IASetIndexBuffer(res.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                Context->DrawIndexed(res.IndexCount, 0, 0);
            }
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        // ── Hi-Z 빌드 ────────────────────────────────────────────────────────────
        BuildHiZMips();
        auto t3 = std::chrono::high_resolution_clock::now();

        // ── PrevInvisible을 비동기 Cull에 넣기 ───────────────────────────────────
        // 전체 오브젝트를 한번에 테스트
        for (uint32_t i = 0; i < TotalCount; ++i)
            SceneData->RenderQueue[i] = SceneData->RenderQueue[i]; // 그대로 유지
        SceneData->RenderCount = TotalCount;

        // PrevIsVisible 초기화
        std::fill(PrevIsVisible.begin(), PrevIsVisible.end(), false);
        RunOcclusionCull(SceneData, viewProj, PrevIsVisible);
        bHasPrevFrame = true;

        auto t4 = std::chrono::high_resolution_clock::now();

        // ── 최종 렌더 (PrevVisible만) ─────────────────────────────────────────────
        const uint32_t FinalCount = PrevVisibleCount;

        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), DepthStencilView.Get());
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0);
        Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());
        Context->RSSetState(DefaultRasterizerState.Get());
        Context->OMSetDepthStencilState(DefaultDepthStencilState.Get(), 0);

        const uint32_t FinalBulk = FinalCount * AlignedConstantSize;
        D3D11_MAP FinalMapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (PerObjectRingBufferOffset + FinalBulk > BufferCapacity)
        {
            FinalMapType = D3D11_MAP_WRITE_DISCARD;
            PerObjectRingBufferOffset = 0;
        }

        D3D11_MAPPED_SUBRESOURCE FinalMap = {};
        if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, FinalMapType, 0, &FinalMap))) return;

        uint8_t* FinalDest = static_cast<uint8_t*>(FinalMap.pData) + PerObjectRingBufferOffset;
        const uint32_t FinalBase = PerObjectRingBufferOffset;

        static uint32_t SortedQueue[Scene::FSceneDataSOA::MAX_OBJECTS];
        uint32_t MeshCounts[MAX_MESH_TYPES] = {};
        uint32_t MeshOffsets[MAX_MESH_TYPES] = {};

        for (uint32_t i = 0; i < FinalCount; ++i)
        {
            const uint32_t mid = SceneData->MeshIDs[PrevVisibleQueue[i]];
            if (mid < MAX_MESH_TYPES) ++MeshCounts[mid];
        }

        uint32_t cur = 0;
        for (uint32_t i = 0; i < MAX_MESH_TYPES; ++i)
        {
            MeshOffsets[i] = cur;
            cur += MeshCounts[i];
        }

        uint32_t TempOffsets[MAX_MESH_TYPES];
        memcpy(TempOffsets, MeshOffsets, sizeof(MeshOffsets));

        for (uint32_t i = 0; i < FinalCount; ++i)
        {
            const uint32_t oid = PrevVisibleQueue[i];
            const uint32_t mid = SceneData->MeshIDs[oid];
            if (mid >= MAX_MESH_TYPES) continue;

            const uint32_t sidx = TempOffsets[mid]++;
            SortedQueue[sidx] = oid;

            const Math::FPacked3x4Matrix& mat = SceneData->WorldMatrices[oid];
            FPerObjectConstants* dest = reinterpret_cast<FPerObjectConstants*>(
                FinalDest + sidx * AlignedConstantSize);
            DirectX::XMStoreFloat4(&dest->Row0, mat.Row0);
            DirectX::XMStoreFloat4(&dest->Row1, mat.Row1);
            DirectX::XMStoreFloat4(&dest->Row2, mat.Row2);
            dest->ColorModifier = (oid == SelectedObjID)
                ? DirectX::XMFLOAT4{ 0.5f, 0.0f, 0.0f, 0.0f }
            : DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.0f };
        }
        Context->Unmap(PerObjectBuffer.Get(), 0);
        PerObjectRingBufferOffset += FinalBulk;

        for (uint32_t mid = 0; mid < MAX_MESH_TYPES; ++mid)
        {
            if (MeshCounts[mid] == 0) continue;
            const FMeshResource& res = MeshResources[mid];
            if (!res.VertexBuffer || !res.IndexBuffer) continue;

            D3D11_MAPPED_SUBRESOURCE matmap = {};
            if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matmap)))
            {
                FMaterialConstants mc = { { 1, 1, 1, 1 } };
                memcpy(matmap.pData, &mc, sizeof(mc));
                Context->Unmap(MaterialBuffer.Get(), 0);
            }
            Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf());

            ID3D11ShaderResourceView* srv = res.DiffuseTextureView
                ? res.DiffuseTextureView.Get()
                : DefaultWhiteTextureView.Get();
            Context->PSSetShaderResources(0, 1, &srv);

            UINT stride = sizeof(FMeshVertex), offset = 0;
            Context->IASetVertexBuffers(0, 1, res.VertexBuffer.GetAddressOf(), &stride, &offset);
            Context->IASetIndexBuffer(res.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

            for (uint32_t i = MeshOffsets[mid]; i < MeshOffsets[mid] + MeshCounts[mid]; ++i)
            {
                if (Context1)
                {
                    UINT off = (FinalBase + i * AlignedConstantSize) / 16;
                    UINT cnt = AlignedConstantSize / 16;
                    Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                }
                Context->DrawIndexed(res.IndexCount, 0, 0);
            }
        }
        auto t5 = std::chrono::high_resolution_clock::now();

        auto ms = [](auto a, auto b) {
            return std::chrono::duration<float, std::milli>(b - a).count();
            };
        char buf[256];
        sprintf_s(buf, "Split=%.2f  Prepass=%.2f  HiZ=%.2f  Cull=%.2f  Draw=%.2f\n",
            ms(t0, t1), ms(t1, t2), ms(t2, t3), ms(t3, t4), ms(t4, t5));
        OutputDebugStringA(buf);

        sprintf_s(buf, "PrevVisible=%u  PrevInvisible=%u  Total=%u\n",
            PrevVisibleCount, PrevInvisibleCount, TotalCount);
        OutputDebugStringA(buf);
    }

    void URenderer::EndFrame() { SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING); }
}
