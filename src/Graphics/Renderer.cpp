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
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <chrono>

namespace Graphics
{
    namespace
    {
        struct FPerFrameConstants
        {
            DirectX::XMFLOAT4X4 ViewProj;
            DirectX::XMFLOAT4   CameraRight;
            DirectX::XMFLOAT4   CameraUp;
            DirectX::XMFLOAT4   CameraPos;
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
            size_t operator()(const FObjVertexKey& k) const noexcept
            {
                return std::hash<int>{}(k.PositionIndex)
                    ^ (std::hash<int>{}(k.TexCoordIndex) << 1)
                    ^ (std::hash<int>{}(k.NormalIndex) << 2);
            }
        };

        bool ParseObjFaceIndices(const std::string& tok, int& p, int& t, int& n)
        {
            p = t = n = -1;
            size_t s1 = tok.find('/');
            if (s1 == std::string::npos) { p = std::stoi(tok) - 1; return p >= 0; }
            p = std::stoi(tok.substr(0, s1)) - 1;
            size_t s2 = tok.find('/', s1 + 1);
            if (s2 == std::string::npos)
            {
                if (s1 + 1 < tok.size()) t = std::stoi(tok.substr(s1 + 1)) - 1;
                return p >= 0;
            }
            if (s2 > s1 + 1) t = std::stoi(tok.substr(s1 + 1, s2 - s1 - 1)) - 1;
            if (s2 + 1 < tok.size()) n = std::stoi(tok.substr(s2 + 1)) - 1;
            return p >= 0;
        }

        bool LoadObjMeshData(const std::wstring& path,
            std::vector<URenderer::FMeshVertex>& verts,
            std::vector<uint32_t>& indices,
            std::wstring& texPath)
        {
            std::ifstream f{ std::filesystem::path(path) };
            if (!f) return false;

            std::vector<DirectX::XMFLOAT3> P, N;
            std::vector<DirectX::XMFLOAT2> T;
            std::unordered_map<FObjVertexKey, uint32_t, FObjVertexKeyHasher> vm;
            std::string line;

            while (std::getline(f, line))
            {
                if (line.size() < 2) continue;
                std::istringstream ls(line);
                std::string pre;
                ls >> pre;

                if (pre == "v")
                {
                    DirectX::XMFLOAT3 v = {};
                    ls >> v.x >> v.y >> v.z;
                    P.push_back(v);
                }
                else if (pre == "vt")
                {
                    DirectX::XMFLOAT2 v = {};
                    ls >> v.x >> v.y;
                    v.y = 1.0f - v.y;
                    T.push_back(v);
                }
                else if (pre == "vn")
                {
                    DirectX::XMFLOAT3 v = {};
                    ls >> v.x >> v.y >> v.z;
                    N.push_back(v);
                }
                else if (pre == "f")
                {
                    std::vector<std::string> toks;
                    std::string t;
                    while (ls >> t) toks.push_back(t);

                    for (size_t tri = 1; tri + 1 < toks.size(); ++tri)
                    {
                        const std::array<std::string, 3> tt = { toks[0], toks[tri], toks[tri + 1] };
                        for (const std::string& ft : tt)
                        {
                            FObjVertexKey k = {};
                            if (!ParseObjFaceIndices(ft, k.PositionIndex, k.TexCoordIndex, k.NormalIndex)) continue;
                            if (k.PositionIndex < 0 || (size_t)k.PositionIndex >= P.size()) continue;

                            auto it = vm.find(k);
                            if (it != vm.end()) { indices.push_back(it->second); continue; }

                            URenderer::FMeshVertex v = {};
                            v.Position = P[k.PositionIndex];
                            v.Normal = (k.NormalIndex >= 0 && (size_t)k.NormalIndex < N.size()) ? N[k.NormalIndex] : DirectX::XMFLOAT3{ 0, 0, 1 };
                            v.TexCoord = (k.TexCoordIndex >= 0 && (size_t)k.TexCoordIndex < T.size()) ? T[k.TexCoordIndex] : DirectX::XMFLOAT2{ 0, 0 };

                            uint32_t vi = (uint32_t)verts.size();
                            verts.push_back(v);
                            indices.push_back(vi);
                            vm.emplace(k, vi);
                        }
                    }
                }
            }

            texPath = (path.find(L"bitten") != std::wstring::npos)
                ? L"Data/JungleApples/Bitten_Apple_tgyociqpa_Mid_2K_BaseColor.jpg"
                : L"Data/JungleApples/Freshly_Bitten_Apple_tgzpdhlpa_Mid_2K_BaseColor.jpg";

            return !verts.empty();
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

        bool LoadTextureWithWIC(ID3D11Device* dev,
            const std::wstring& path,
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> fact;
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fact));

            Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
            if (FAILED(fact->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) return false;

            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            dec->GetFrame(0, &frame);

            Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
            fact->CreateFormatConverter(&conv);
            conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

            UINT w, h;
            conv->GetSize(&w, &h);
            std::vector<uint8_t> pix((size_t)w * h * 4);
            conv->CopyPixels(nullptr, w * 4, (UINT)pix.size(), pix.data());

            D3D11_TEXTURE2D_DESC td = { w, h, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
            D3D11_SUBRESOURCE_DATA id = { pix.data(), w * 4, 0 };
            Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
            dev->CreateTexture2D(&td, &id, &tex);
            return SUCCEEDED(dev->CreateShaderResourceView(tex.Get(), nullptr, &srv));
        }

        bool LoadMeshResource(ID3D11Device* dev,
            const std::wstring& path,
            URenderer::FMeshResource& res)
        {
            if (!LoadObjMeshData(path, res.SourceVertices, res.SourceIndices, res.DiffuseTexturePath)) return false;

            float minX = 1e9f, minY = 1e9f, minZ = 1e9f;
            float maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
            for (const auto& v : res.SourceVertices)
            {
                minX = (std::min)(minX, v.Position.x); minY = (std::min)(minY, v.Position.y); minZ = (std::min)(minZ, v.Position.z);
                maxX = (std::max)(maxX, v.Position.x); maxY = (std::max)(maxY, v.Position.y); maxZ = (std::max)(maxZ, v.Position.z);
            }
            res.LocalCenter = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f };

            D3D11_BUFFER_DESC vb = { (UINT)(res.SourceVertices.size() * sizeof(URenderer::FMeshVertex)), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA vd = { res.SourceVertices.data(), 0, 0 };
            dev->CreateBuffer(&vb, &vd, &res.VertexBuffer);

            D3D11_BUFFER_DESC ib = { (UINT)(res.SourceIndices.size() * sizeof(uint32_t)), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA id = { res.SourceIndices.data(), 0, 0 };
            dev->CreateBuffer(&ib, &id, &res.IndexBuffer);

            LoadTextureWithWIC(dev, res.DiffuseTexturePath, res.DiffuseTextureView);
            res.IndexCount = (uint32_t)res.SourceIndices.size();
            return true;
        }
    } // anonymous namespace

    // ============================================================================
    URenderer::URenderer() = default;
    URenderer::~URenderer() = default;

    // ============================================================================
    bool URenderer::Initialize(HWND InWindowHandle, int InWidth, int InHeight)
    {
        ViewportWidth = static_cast<uint32_t>(InWidth);
        ViewportHeight = static_cast<uint32_t>(InHeight);

        DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
        SwapChainDesc.BufferCount = 3; // Triple Buffering
        SwapChainDesc.BufferDesc.Width = InWidth;
        SwapChainDesc.BufferDesc.Height = InHeight;
        SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.BufferDesc.RefreshRate = { 0, 1 }; // Uncapped
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.OutputWindow = InWindowHandle;
        SwapChainDesc.SampleDesc = { 1, 0 };
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        SwapChainDesc.Windowed = TRUE;

        const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            FeatureLevels, 1, D3D11_SDK_VERSION,
            &SwapChainDesc, &SwapChain, &Device, nullptr, &Context)))
            return false;

        Context.As(&Context1);

        // ── RTV ──────────────────────────────────────────────────────────────────
        {
            ComPtr<ID3D11Texture2D> BackBuffer;
            if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer)))) return false;
            if (FAILED(Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView))) return false;
        }

        // ── Depth Buffer (R32_TYPELESS — DSV + SRV 동시 사용) ────────────────────
        {
            ComPtr<ID3D11Texture2D> DepthBuffer;
            D3D11_TEXTURE2D_DESC DepthDesc = {};
            DepthDesc.Width = InWidth;
            DepthDesc.Height = InHeight;
            DepthDesc.MipLevels = 1;
            DepthDesc.ArraySize = 1;
            DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            DepthDesc.SampleDesc = { 1, 0 };
            DepthDesc.Usage = D3D11_USAGE_DEFAULT;
            DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer))) return false;

            // DSV : D32_FLOAT
            D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
            DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            DSVDesc.Texture2D.MipSlice = 0;
            if (FAILED(Device->CreateDepthStencilView(DepthBuffer.Get(), &DSVDesc, &DepthStencilView))) return false;

            // SRV : R32_FLOAT → CS에서 직접 읽기 가능
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.MipLevels = 1;
            if (FAILED(Device->CreateShaderResourceView(DepthBuffer.Get(), &SRVDesc, &DepthCopySRV))) return false;
        }

        // ── HUD ──────────────────────────────────────────────────────────────────
        HUD = std::make_unique<FHUD>();
        if (!HUD->Initialize(Device.Get(), Context.Get())) return false;

        if (!CreateDefaultResources()) return false;

        InitHiZResources(ViewportWidth, ViewportHeight);

        return true;
    }

    // ============================================================================
    void URenderer::RenderHUD()
    {
        if (HUD)
        {
            HUD->Update(CurrentMetrics, ViewportWidth, ViewportHeight);
            HUD->Render();
        }
    }

    // ============================================================================
    void URenderer::Resize(int Width, int Height)
    {
        if (Width == 0 || Height == 0 || !SwapChain) return;

        ViewportWidth = static_cast<uint32_t>(Width);
        ViewportHeight = static_cast<uint32_t>(Height);

        Context->OMSetRenderTargets(0, nullptr, nullptr);
        MainRenderTargetView.Reset();
        DepthStencilView.Reset();

        if (FAILED(SwapChain->ResizeBuffers(0, ViewportWidth, ViewportHeight, DXGI_FORMAT_UNKNOWN,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING)))
            return;

        {
            ComPtr<ID3D11Texture2D> BackBuffer;
            if (SUCCEEDED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer))))
                Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView);
        }

        {
            ComPtr<ID3D11Texture2D> DepthBuffer;
            D3D11_TEXTURE2D_DESC DepthDesc = {};
            DepthDesc.Width = ViewportWidth;
            DepthDesc.Height = ViewportHeight;
            DepthDesc.MipLevels = 1;
            DepthDesc.ArraySize = 1;
            DepthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            DepthDesc.SampleDesc = { 1, 0 };
            DepthDesc.Usage = D3D11_USAGE_DEFAULT;
            DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer))) return;

            D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
            DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            DSVDesc.Texture2D.MipSlice = 0;
            Device->CreateDepthStencilView(DepthBuffer.Get(), &DSVDesc, &DepthStencilView);

            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MostDetailedMip = 0;
            SRVDesc.Texture2D.MipLevels = 1;
            Device->CreateShaderResourceView(DepthBuffer.Get(), &SRVDesc, &DepthCopySRV);
        }

        // Hi-Z 리소스 재생성
        HiZTexture.Reset();
        HiZSRV.Reset();
        HiZMipUAVs.clear();
        HiZMipSRVs.clear();
        InitHiZResources(ViewportWidth, ViewportHeight);
    }

    // ============================================================================
    const URenderer::FMeshResource* URenderer::GetMeshResource(uint32_t MeshID) const
    {
        if (MeshID < MAX_MESH_TYPES) return &MeshResources[MeshID];
        return nullptr;
    }

    // ============================================================================
    bool URenderer::CreateDefaultResources()
    {
        // ── Bake용 임시 cbuffer (BakeImpostor 전용) ───────────────────────────────
        {
            D3D11_BUFFER_DESC bfd = { sizeof(FPerFrameConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
            Device->CreateBuffer(&bfd, nullptr, &BakePerFrameBuffer);
            D3D11_BUFFER_DESC bod = { 4096, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
            Device->CreateBuffer(&bod, nullptr, &BakePerObjectBuffer);
        }

        // ── Main Shader ───────────────────────────────────────────────────────────
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
                float3 Pos      : POSITION;
                float3 Norm     : NORMAL;
                float2 TexCoord : TEXCOORD0;
            };

            struct PS_IN
            {
                float4 Pos          : SV_POSITION;
                float2 TexCoord     : TEXCOORD0;
                float  HighlightRed : COLOR;
            };

            PS_IN VSMain(VS_IN i)
            {
                PS_IN o;
                float4 LocalPos  = float4(i.Pos, 1.0f);
                float3 WorldPos  = float3(dot(LocalPos, Row0), dot(LocalPos, Row1), dot(LocalPos, Row2));
                o.Pos            = mul(float4(WorldPos, 1.0f), ViewProj);
                o.TexCoord       = i.TexCoord;
                o.HighlightRed   = ColorModifier.x;
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
        if (FAILED(Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &PixelShader)))  return false;

        D3D11_INPUT_ELEMENT_DESC Layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        if (FAILED(Device->CreateInputLayout(Layout, static_cast<UINT>(std::size(Layout)), VS->GetBufferPointer(), VS->GetBufferSize(), &InputLayout))) return false;

        // ── Constant Buffers ──────────────────────────────────────────────────────
        {
            D3D11_BUFFER_DESC d = { sizeof(FPerFrameConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
            if (FAILED(Device->CreateBuffer(&d, nullptr, &PerFrameBuffer))) return false;
        }
        {
            D3D11_BUFFER_DESC d = { 64 * 1024 * 1024, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
            if (FAILED(Device->CreateBuffer(&d, nullptr, &PerObjectBuffer))) return false;
        }
        {
            D3D11_BUFFER_DESC d = { sizeof(FMaterialConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
            if (FAILED(Device->CreateBuffer(&d, nullptr, &MaterialBuffer))) return false;
        }

        // ── Sampler / Rasterizer / DepthStencil ──────────────────────────────────
        {
            D3D11_SAMPLER_DESC sd = {};
            sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            sd.MinLOD = 0.0f; sd.MaxLOD = D3D11_FLOAT32_MAX;
            if (FAILED(Device->CreateSamplerState(&sd, &DiffuseSamplerState))) return false;
        }
        {
            D3D11_RASTERIZER_DESC rd = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
            if (FAILED(Device->CreateRasterizerState(&rd, &DefaultRasterizerState))) return false;
        }
        {
            D3D11_DEPTH_STENCIL_DESC dd = { TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL, FALSE, 0, 0, {}, {} };
            if (FAILED(Device->CreateDepthStencilState(&dd, &DefaultDepthStencilState))) return false;
        }

        // ── Default White Texture ─────────────────────────────────────────────────
        if (!CreateSolidTexture(Device.Get(), { 1.0f, 1.0f, 1.0f, 1.0f }, DefaultWhiteTextureView)) return false;

        // ── Mesh Resources ────────────────────────────────────────────────────────
        const std::wstring MeshBasePath = Core::FPathManager::GetMeshPath();
        if (!LoadMeshResource(Device.Get(), MeshBasePath + L"apple_mid.obj", MeshResources[0])) return false;
        if (!LoadMeshResource(Device.Get(), MeshBasePath + L"bitten_apple_mid.obj", MeshResources[1])) return false;
        if (!MeshResources[0].DiffuseTextureView) MeshResources[0].DiffuseTextureView = DefaultWhiteTextureView;
        if (!MeshResources[1].DiffuseTextureView) MeshResources[1].DiffuseTextureView = DefaultWhiteTextureView;

        // ── Billboard Shader ──────────────────────────────────────────────────────
        const char* BBShader = R"(
            cbuffer PerFrame : register(b0) { row_major float4x4 VP; float4 CR; float4 CU; float4 CP; };
            cbuffer PerObject : register(b1) { float4 R0; float4 R1; float4 R2; float4 PD; };
            Texture2D SN : register(t0);
            SamplerState SS : register(s0);

            struct VI { float3 P : POSITION; float2 T : TEXCOORD0; };
            struct PI { float4 P : SV_POSITION; float2 T : TEXCOORD0; };

            PI VSMain(VI i)
            {
                PI o;
                float3 wp = float3(R0.w, R1.w, R2.w) + PD.xyz;
                float sx = length(float3(R0.x, R1.x, R2.x));
                float sy = length(float3(R0.y, R1.y, R2.y));
                float3 fp = wp + (i.P.x * sx * 2.5f * CR.xyz) + (i.P.z * sy * 2.5f * CU.xyz);
                o.P = mul(float4(fp, 1.0f), VP);
                o.T = i.T;
                return o;
            }

            float4 PSMain(PI i) : SV_TARGET
            {
                float3 wp = float3(R0.w, R1.w, R2.w);

                // 1. 사과의 3D 회전축 추출
                float3 X = normalize(float3(R0.x, R1.x, R2.x));
                float3 Y = normalize(float3(R0.y, R1.y, R2.y));
                float3 Z = normalize(float3(R0.z, R1.z, R2.z));

                // 2. 카메라 방향 벡터
                float3 V = normalize(CP.xyz - wp);

                // 3. 6개 면과의 내적 계산
                float d_pX = dot(V, X); float d_mX = -d_pX;
                float d_pY = dot(V, Y); float d_mY = -d_pY;
                float d_pZ = dot(V, Z); float d_mZ = -d_pZ;

                // 4. 가장 카메라를 정면으로 바라보는 면 찾기
                int f = 0; float maxDot = d_pX;              // Frame 0: +X면
                if (d_mY > maxDot) { maxDot = d_mY; f = 1; } // Frame 1: -Y면
                if (d_mX > maxDot) { maxDot = d_mX; f = 2; } // Frame 2: -X면
                if (d_pY > maxDot) { maxDot = d_pY; f = 3; } // Frame 3: +Y면
                if (d_pZ > maxDot) { maxDot = d_pZ; f = 4; } // Frame 4: +Z면(윗면)
                if (d_mZ > maxDot) { maxDot = d_mZ; f = 5; } // Frame 5: -Z면(아랫면)

                // 5. 1024x512 아틀라스(4x2) UV 맵핑
                float2 uv = (i.T * float2(0.25f, 0.5f)) + float2(f % 4, f / 4) * float2(0.25f, 0.5f);
                float4 c = SN.Sample(SS, uv);
                if (c.a < 0.1f) discard;
                return c;
            }
        )";

        D3DCompile(BBShader, strlen(BBShader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err);
        D3DCompile(BBShader, strlen(BBShader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, &Err);
        Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &BillboardVS);
        Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &BillboardPS);

        D3D11_INPUT_ELEMENT_DESC bbl[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        Device->CreateInputLayout(bbl, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &BillboardLayout);

        FBillboardVertex bbv[] = {
            { {-0.5f, 0, 0.5f},  {0, 0} },
            { { 0.5f, 0, 0.5f},  {1, 0} },
            { {-0.5f, 0, -0.5f}, {0, 1} },
            { { 0.5f, 0, -0.5f}, {1, 1} },
        };
        {
            D3D11_BUFFER_DESC bvb = { sizeof(bbv), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA bvd = { bbv, 0, 0 };
            Device->CreateBuffer(&bvb, &bvd, &BillboardVB);
        }
        uint32_t bbi[] = { 0, 1, 2, 2, 1, 3 };
        {
            D3D11_BUFFER_DESC bib = { sizeof(bbi), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA bid = { bbi, 0, 0 };
            Device->CreateBuffer(&bib, &bid, &BillboardIB);
        }

        BakeImpostor(0);
        BakeImpostor(1);

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

        // ── Occlusion Test CS ─────────────────────────────────────────────────────
        const char* HiZCullSrc = R"(
            struct FObjectBounds
            {
                float3 BoundsMin;
                uint   ObjectIndex;
                float3 BoundsMax;
                uint   _pad;
            };

            cbuffer CullParams : register(b0)
            {
                row_major float4x4 ViewProj;
                uint  ObjectCount;
                uint  HiZMipLevels;
                float HiZTexelWidth;
                float HiZTexelHeight;
            };

            StructuredBuffer<FObjectBounds> InBounds        : register(t0);
            Texture2D<float>                HiZTexture      : register(t1);
            SamplerState                    PointClampSamp  : register(s0);
            RWStructuredBuffer<uint>        VisibilityFlags : register(u0);

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

                if (any(ndcMax < -1.0f) || any(ndcMin > 1.0f))
                {
                    VisibilityFlags[b.ObjectIndex] = 0;
                    return;
                }

                ndcMin = clamp(ndcMin, -1.0f, 1.0f);
                ndcMax = clamp(ndcMax, -1.0f, 1.0f);

                float2 uvMin = float2( ndcMin.x * 0.5f + 0.5f, -ndcMax.y * 0.5f + 0.5f);
                float2 uvMax = float2( ndcMax.x * 0.5f + 0.5f, -ndcMin.y * 0.5f + 0.5f);

                float2 sizeUV = uvMax - uvMin;
                float  sizeX  = sizeUV.x * (float)HiZTexelWidth;
                float  sizeY  = sizeUV.y * (float)HiZTexelHeight;
                float  texels = max(sizeX, sizeY);
                uint   mip    = (uint)clamp(floor(log2(texels)), 0.0f, (float)HiZMipLevels);

                float d0 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMin.x, uvMin.y), mip);
                float d1 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMax.x, uvMin.y), mip);
                float d2 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMin.x, uvMax.y), mip);
                float d3 = HiZTexture.SampleLevel(PointClampSamp, float2(uvMax.x, uvMax.y), mip);
                float occluderDepth = max(max(d0, d1), max(d2, d3));

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

    // ============================================================================
    void URenderer::BakeImpostor(uint32_t MeshID)
    {
        const FMeshResource& res = MeshResources[MeshID];
        if (res.SourceVertices.empty()) return;

        // ── Atlas RTV / DSV ───────────────────────────────────────────────────────
        ComPtr<ID3D11Texture2D>          tex;
        ComPtr<ID3D11RenderTargetView>   rtv;
        ComPtr<ID3D11ShaderResourceView> srv;
        {
            D3D11_TEXTURE2D_DESC td = { 1024, 512, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0 };
            Device->CreateTexture2D(&td, nullptr, &tex);
            Device->CreateRenderTargetView(tex.Get(), nullptr, &rtv);
            Device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
        }
        ComPtr<ID3D11Texture2D>        dtx;
        ComPtr<ID3D11DepthStencilView> dsv;
        {
            D3D11_TEXTURE2D_DESC dd = { 1024, 512, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0 };
            Device->CreateTexture2D(&dd, nullptr, &dtx);
            Device->CreateDepthStencilView(dtx.Get(), nullptr, &dsv);
        }

        float clr[4] = { 0, 0, 0, 0 };
        Context->ClearRenderTargetView(rtv.Get(), clr);
        Context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        Context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());

        // NoCull 래스터라이저
        ComPtr<ID3D11RasterizerState> NoCull;
        {
            D3D11_RASTERIZER_DESC rd = { D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
            Device->CreateRasterizerState(&rd, &NoCull);
        }
        Context->RSSetState(NoCull.Get());
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // PerFrame (ortho from +X)
        {
            DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH({ 3, 0, 0 }, { 0, 0, 0 }, { 0, 0, 1 });
            DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicLH(2.5f, 2.5f, 0.1f, 10.0f);
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(Context->Map(BakePerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                FPerFrameConstants pf = {};
                DirectX::XMStoreFloat4x4(&pf.ViewProj, view * proj);
                memcpy(m.pData, &pf, sizeof(pf));
                Context->Unmap(BakePerFrameBuffer.Get(), 0);
            }
        }

        // Material
        {
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                FMaterialConstants mc = { { 1, 1, 1, 1 } };
                memcpy(m.pData, &mc, sizeof(mc));
                Context->Unmap(MaterialBuffer.Get(), 0);
            }
        }

        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0);
        Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, BakePerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf());
        Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());
        {
            ID3D11ShaderResourceView* dsrv = res.DiffuseTextureView.Get();
            Context->PSSetShaderResources(0, 1, &dsrv);
        }
        UINT s = sizeof(FMeshVertex), o = 0;
        Context->IASetVertexBuffers(0, 1, res.VertexBuffer.GetAddressOf(), &s, &o);
        Context->IASetIndexBuffer(res.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

        // 6-face bake (4x2 atlas)
        for (int f = 0; f < 6; ++f)
        {
            D3D11_VIEWPORT vp = { (float)((f % 4) * 256), (float)((f / 4) * 256), 256.0f, 256.0f, 0, 1 };
            Context->RSSetViewports(1, &vp);

            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(Context->Map(BakePerObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                DirectX::XMMATRIX rotMat;
                switch (f)
                {
                case 0: rotMat = DirectX::XMMatrixIdentity();                              break; // +X
                case 1: rotMat = DirectX::XMMatrixRotationZ(DirectX::XM_PIDIV2);         break; // -Y
                case 2: rotMat = DirectX::XMMatrixRotationZ(DirectX::XM_PI);             break; // -X
                case 3: rotMat = DirectX::XMMatrixRotationZ(-DirectX::XM_PIDIV2);         break; // +Y
                case 4: rotMat = DirectX::XMMatrixRotationY(DirectX::XM_PIDIV2);         break; // +Z (top)
                default:rotMat = DirectX::XMMatrixRotationY(-DirectX::XM_PIDIV2);         break; // -Z (bottom)
                }

                DirectX::XMMATRIX objMat = DirectX::XMMatrixTranslation(
                    -res.LocalCenter.x, -res.LocalCenter.y, -res.LocalCenter.z) * rotMat;

                FPerObjectConstants po = {};
                DirectX::XMMATRIX sm = DirectX::XMMatrixTranspose(objMat);
                DirectX::XMStoreFloat4(&po.Row0, sm.r[0]);
                DirectX::XMStoreFloat4(&po.Row1, sm.r[1]);
                DirectX::XMStoreFloat4(&po.Row2, sm.r[2]);
                po.ColorModifier = { 0, 0, 0, 1 };
                std::memcpy(m.pData, &po, sizeof(po));
                Context->Unmap(BakePerObjectBuffer.Get(), 0);
            }
            Context->VSSetConstantBuffers(1, 1, BakePerObjectBuffer.GetAddressOf());
            Context->DrawIndexed(res.IndexCount, 0, 0);
        }

        ImpostorResources[MeshID].SnapshotTexture = tex;
        ImpostorResources[MeshID].SnapshotSRV = srv;
        ImpostorResources[MeshID].bIsBaked = true;

        ID3D11RenderTargetView* nrt = nullptr;
        Context->OMSetRenderTargets(1, &nrt, nullptr);
    }

    // ============================================================================
    void URenderer::InitHiZResources(uint32_t Width, uint32_t Height)
    {
        HiZWidth = Width;
        HiZHeight = Height;
        HiZMipCount = 1;
        uint32_t sz = std::max<uint32_t>(Width, Height);
        while (sz > 1) { sz >>= 1; ++HiZMipCount; }

        // Hi-Z Texture
        {
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = Width;
            td.Height = Height;
            td.MipLevels = HiZMipCount;
            td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R32_FLOAT;
            td.SampleDesc = { 1, 0 };
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            Device->CreateTexture2D(&td, nullptr, &HiZTexture);
        }

        // 전체 SRV
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

        // Bounds StructuredBuffer
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

        // Visibility RWStructuredBuffer
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

            // CPU Readback Staging (더블 버퍼)
            D3D11_BUFFER_DESC sbd = vbd;
            sbd.Usage = D3D11_USAGE_STAGING;
            sbd.BindFlags = 0;
            sbd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sbd.MiscFlags = 0;
            Device->CreateBuffer(&sbd, nullptr, &VisibilityStagingBuffers[0]);
            Device->CreateBuffer(&sbd, nullptr, &VisibilityStagingBuffers[1]);
        }

        // CullParam / HiZBuildParam cbuffer
        {
            D3D11_BUFFER_DESC cbd = {};
            cbd.ByteWidth = 256;
            cbd.Usage = D3D11_USAGE_DYNAMIC;
            cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            Device->CreateBuffer(&cbd, nullptr, &CullParamBuffer);
            Device->CreateBuffer(&cbd, nullptr, &HiZBuildParamBuffer);
        }

        // PointClamp Sampler
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

    // ============================================================================
    void URenderer::BuildHiZMips()
    {
        // DSV 해제 (같은 텍스처를 SRV로 읽기 위해)
        ID3D11RenderTargetView* nullRTV = nullptr;
        Context->OMSetRenderTargets(0, &nullRTV, nullptr);

        // Depth → HiZTexture mip0 복사
        {
            ID3D11Resource* depthRes = nullptr;
            DepthCopySRV->GetResource(&depthRes);
            Context->CopySubresourceRegion(HiZTexture.Get(), 0, 0, 0, 0, depthRes, 0, nullptr);
            depthRes->Release();
        }

        Context->CSSetShader(CSBuildHiZ.Get(), nullptr, 0);

        for (uint32_t m = 1; m < HiZMipCount; ++m)
        {
            const uint32_t SrcW = std::max<uint32_t>(1u, HiZWidth >> (m - 1));
            const uint32_t SrcH = std::max<uint32_t>(1u, HiZHeight >> (m - 1));
            const uint32_t DstW = std::max<uint32_t>(1u, HiZWidth >> m);
            const uint32_t DstH = std::max<uint32_t>(1u, HiZHeight >> m);

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

            // m==1: DepthCopySRV에서 직접 읽기 (mip0 SRV 충돌 방지)
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

    // ============================================================================
    void URenderer::RunOcclusionCull(Scene::FSceneDataSOA* SceneData,
        const DirectX::XMMATRIX& ViewProj,
        std::array<bool, Scene::FSceneDataSOA::MAX_OBJECTS>& OutIsVisible)
    {
        const uint32_t Count = SceneData->RenderCount;
        if (Count == 0) return;

        // 1. Bounds 버퍼 업데이트
        {
            D3D11_MAPPED_SUBRESOURCE mr = {};
            if (FAILED(Context->Map(BoundsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr))) return;

            auto* dst = static_cast<FObjectBoundsGPU*>(mr.pData);
            for (uint32_t i = 0; i < Count; ++i)
            {
                const uint32_t oid = SceneData->RenderQueue[i];
                dst[i].BoundsMin = { SceneData->MinX[oid], SceneData->MinY[oid], SceneData->MinZ[oid] };
                dst[i].BoundsMax = { SceneData->MaxX[oid], SceneData->MaxY[oid], SceneData->MaxZ[oid] };
                dst[i].ObjectIndex = oid;
                dst[i]._pad = 0;
            }
            Context->Unmap(BoundsBuffer.Get(), 0);
        }

        // 2. CullParams 업데이트
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

        // 3. Compute Shader 실행
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

        // 4. GPU → CPU Readback (더블 버퍼링)
        Context->CopyResource(VisibilityStagingBuffers[StagingWriteIndex].Get(),
            VisibilityBuffer.Get());

        // 첫 프레임: 전부 Visible
        if (bFirstFrame)
        {
            bFirstFrame = false;
            for (uint32_t i = 0; i < Count; ++i)
                OutIsVisible[SceneData->RenderQueue[i]] = true;
            std::swap(StagingReadIndex, StagingWriteIndex);
            return;
        }

        // 5. 이전 프레임 결과 읽기 (GPU stall 없음)
        D3D11_MAPPED_SUBRESOURCE mr = {};
        if (FAILED(Context->Map(VisibilityStagingBuffers[StagingReadIndex].Get(),
            0, D3D11_MAP_READ, 0, &mr)))
        {
            std::swap(StagingReadIndex, StagingWriteIndex);
            return;
        }

        const uint32_t* flags = static_cast<const uint32_t*>(mr.pData);
        for (uint32_t i = 0; i < Count; ++i)
        {
            const uint32_t oid = SceneData->RenderQueue[i];
            if (flags[oid] != 0)
                OutIsVisible[oid] = true;
        }

        Context->Unmap(VisibilityStagingBuffers[StagingReadIndex].Get(), 0);
        std::swap(StagingReadIndex, StagingWriteIndex);
    }

    // ============================================================================
    void URenderer::BeginFrame()
    {
        const float Color[4] = { 0.03f, 0.03f, 0.06f, 1.0f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), DepthStencilView.Get());
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), Color);
        Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (float)ViewportWidth, (float)ViewportHeight, 0.0f, 1.0f };
        Context->RSSetViewports(1, &Viewport);
    }

    // ============================================================================
    void URenderer::RenderScene(const Scene::USceneManager& InSceneManager)
    {
        uint32_t DrawCount = 0;
        Scene::FSceneDataSOA* SceneData =
            const_cast<Scene::FSceneDataSOA*>(InSceneManager.GetSceneData());

        const Scene::FSceneSelectionData& Selection = InSceneManager.GetSelectionData();
        const uint32_t                    SelectedObjID = Selection.bHasSelection ? Selection.ObjectIndex : 0xFFFFFFFF;

        if (!SceneData || !PerFrameBuffer || !PerObjectBuffer || !MaterialBuffer) return;
        if (SceneData->RenderCount == 0) return;

        constexpr uint32_t AlignedConstantSize = 256;
        constexpr uint32_t BufferCapacity = 64 * 1024 * 1024;

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
                DirectX::XMStoreFloat4(&pf.CameraRight, DirectX::XMMatrixTranspose(view).r[0]);
                DirectX::XMStoreFloat4(&pf.CameraUp, DirectX::XMMatrixTranspose(view).r[1]);
                DirectX::XMStoreFloat4(&pf.CameraPos, camPos);
                memcpy(pfmap.pData, &pf, sizeof(pf));
                Context->Unmap(PerFrameBuffer.Get(), 0);
            }
        }

        // Material
        {
            D3D11_MAPPED_SUBRESOURCE m = {};
            if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                FMaterialConstants mc = { { 1, 1, 1, 1 } };
                memcpy(m.pData, &mc, sizeof(mc));
                Context->Unmap(MaterialBuffer.Get(), 0);
            }
        }

        const uint32_t TotalCount = SceneData->RenderCount;

        auto t0 = std::chrono::high_resolution_clock::now();

        // ── 이전 프레임 결과로 Visible / Invisible 분리 ───────────────────────────
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
                FPerObjectConstants* dest = reinterpret_cast<FPerObjectConstants*>(base + i * AlignedConstantSize);
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
                DrawCount++;
            }
        }

        auto t2 = std::chrono::high_resolution_clock::now();

        // ── Hi-Z 빌드 ────────────────────────────────────────────────────────────
        BuildHiZMips();

        auto t3 = std::chrono::high_resolution_clock::now();

        // ── Occlusion Cull (전체 오브젝트 대상) ──────────────────────────────────
        std::fill(PrevIsVisible.begin(), PrevIsVisible.end(), false);
        RunOcclusionCull(SceneData, viewProj, PrevIsVisible);
        bHasPrevFrame = true;

        auto t4 = std::chrono::high_resolution_clock::now();

        // ── Pass 2: 최종 컬러 렌더 (PrevVisible만) ───────────────────────────────
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

        {
            uint32_t cur = 0;
            for (uint32_t i = 0; i < MAX_MESH_TYPES; ++i) { MeshOffsets[i] = cur; cur += MeshCounts[i]; }
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

        // 메시별 배치 드로우
        for (uint32_t mid = 0; mid < MAX_MESH_TYPES; ++mid)
        {
            if (MeshCounts[mid] == 0) continue;
            const FMeshResource& res = MeshResources[mid];
            if (!res.VertexBuffer || !res.IndexBuffer) continue;

            {
                D3D11_MAPPED_SUBRESOURCE matmap = {};
                if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &matmap)))
                {
                    FMaterialConstants mc = { { 1, 1, 1, 1 } };
                    memcpy(matmap.pData, &mc, sizeof(mc));
                    Context->Unmap(MaterialBuffer.Get(), 0);
                }
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
                DrawCount++;
            }
        }

        // ── Billboard 렌더 ────────────────────────────────────────────────────────
        // (Billboard는 기존 RenderScene의 고-밀도 버전 로직과 동일하게 유지)
        Context->IASetInputLayout(BillboardLayout.Get());
        Context->VSSetShader(BillboardVS.Get(), nullptr, 0);
        Context->PSSetShader(BillboardPS.Get(), nullptr, 0);

        for (uint32_t mid = 0; mid < 2; ++mid)
        {
            if (!ImpostorResources[mid].bIsBaked) continue;
            const uint32_t bid = mid + 10;
            if (MeshCounts[bid] == 0) continue;

            Context->PSSetShaderResources(0, 1, ImpostorResources[mid].SnapshotSRV.GetAddressOf());
            UINT stride = sizeof(FBillboardVertex), offset = 0;
            Context->IASetVertexBuffers(0, 1, BillboardVB.GetAddressOf(), &stride, &offset);
            Context->IASetIndexBuffer(BillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);

            for (uint32_t i = MeshOffsets[bid]; i < MeshOffsets[bid] + MeshCounts[bid]; ++i)
            {
                if (Context1)
                {
                    UINT off = (FinalBase + i * AlignedConstantSize) / 16;
                    UINT cnt = AlignedConstantSize / 16;
                    Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                    Context1->PSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                }
                Context->DrawIndexed(6, 0, 0);
                DrawCount++;
            }
        }

        auto t5 = std::chrono::high_resolution_clock::now();

        // ── 타이밍 로그 ──────────────────────────────────────────────────────────
        auto ms = [](auto a, auto b) {
            return std::chrono::duration<float, std::milli>(b - a).count();
            };
        char buf[256];
        sprintf_s(buf, "Split=%.2f  Prepass=%.2f  HiZ=%.2f  Cull=%.2f  Draw=%.2f\n",
            ms(t0, t1), ms(t1, t2), ms(t2, t3), ms(t3, t4), ms(t4, t5));
        OutputDebugStringA(buf);

        sprintf_s(buf, "DrawCount: %u  PrevVisible=%u  PrevInvisible=%u  Total=%u\n",
            DrawCount, PrevVisibleCount, PrevInvisibleCount, TotalCount);
        OutputDebugStringA(buf);
    }

    // ============================================================================
    void URenderer::EndFrame()
    {
        SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }

} // namespace Graphics
