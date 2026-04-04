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
        struct FPerFrameConstants { DirectX::XMFLOAT4X4 VP; DirectX::XMFLOAT4 CR; DirectX::XMFLOAT4 CU; DirectX::XMFLOAT4 CP; };
        struct FPerObjectConstants { DirectX::XMFLOAT4 R0; DirectX::XMFLOAT4 R1; DirectX::XMFLOAT4 R2; DirectX::XMFLOAT4 PD; };
        struct FMaterialConstants { DirectX::XMFLOAT4 BC; };

        struct FObjVertexKey { int p, t, n; bool operator==(const FObjVertexKey& o) const { return p == o.p && t == o.t && n == o.n; } };
        struct FObjVertexKeyHasher { size_t operator()(const FObjVertexKey& k) const noexcept { return std::hash<int>{}(k.p) ^ (std::hash<int>{}(k.t) << 1) ^ (std::hash<int>{}(k.n) << 2); } };

        bool ParseObjFaceIndices(const std::string& tok, int& p, int& t, int& n)
        {
            p = t = n = -1; size_t s1 = tok.find('/');
            if (s1 == std::string::npos) { p = std::stoi(tok) - 1; return p >= 0; }
            p = std::stoi(tok.substr(0, s1)) - 1; size_t s2 = tok.find('/', s1 + 1);
            if (s2 == std::string::npos) { if (s1 + 1 < tok.size()) t = std::stoi(tok.substr(s1 + 1)) - 1; return p >= 0; }
            if (s2 > s1 + 1) t = std::stoi(tok.substr(s1 + 1, s2 - s1 - 1)) - 1;
            if (s2 + 1 < tok.size()) n = std::stoi(tok.substr(s2 + 1)) - 1;
            return p >= 0;
        }

        std::wstring ReadDiffuseTexturePathFromMtl(const std::filesystem::path& path)
        {
            std::ifstream f(path); if (!f) return L""; std::string line;
            while (std::getline(f, line)) {
                std::istringstream ls(line); std::string pre; ls >> pre;
                if (pre == "map_Kd") { std::string rel; std::getline(ls >> std::ws, rel); if (rel.empty()) return L""; return (path.parent_path() / std::filesystem::path(rel)).lexically_normal().wstring(); }
            }
            return L"";
        }

        bool LoadObjMeshData(const std::wstring& path, std::vector<URenderer::FMeshVertex>& verts, std::vector<uint32_t>& indices, std::wstring& texPath)
        {
            std::ifstream f{ std::filesystem::path(path) }; if (!f) return false;
            std::vector<DirectX::XMFLOAT3> P; std::vector<DirectX::XMFLOAT3> N; std::vector<DirectX::XMFLOAT2> T;
            std::unordered_map<FObjVertexKey, uint32_t, FObjVertexKeyHasher> vm; std::filesystem::path mtlp; std::string line;
            while (std::getline(f, line)) {
                if (line.size() < 2) continue; std::istringstream ls(line); std::string pre; ls >> pre;
                if (pre == "mtllib") { std::string rel; std::getline(ls >> std::ws, rel); if (!rel.empty()) mtlp = std::filesystem::path(path).parent_path() / std::filesystem::path(rel); }
                else if (pre == "v") { DirectX::XMFLOAT3 v = {}; ls >> v.x >> v.y >> v.z; P.push_back(v); }
                else if (pre == "vt") { DirectX::XMFLOAT2 v = {}; ls >> v.x >> v.y; v.y = 1.0f - v.y; T.push_back(v); }
                else if (pre == "vn") { DirectX::XMFLOAT3 v = {}; ls >> v.x >> v.y >> v.z; N.push_back(v); }
                else if (pre == "f") {
                    std::vector<std::string> toks; std::string t; while (ls >> t) toks.push_back(t);
                    if (toks.size() < 3) continue;
                    for (size_t tri = 1; tri + 1 < toks.size(); ++tri) {
                        const std::array<std::string, 3> tt = { toks[0], toks[tri], toks[tri + 1] };
                        for (const std::string& ft : tt) {
                            FObjVertexKey k = {}; if (!ParseObjFaceIndices(ft, k.p, k.t, k.n)) continue;
                            if (k.p < 0 || (size_t)k.p >= P.size()) continue;
                            const auto ex = vm.find(k); if (ex != vm.end()) { indices.push_back(ex->second); continue; }
                            URenderer::FMeshVertex v = {}; v.Position = P[k.p];
                            v.Normal = (k.n >= 0 && (size_t)k.n < N.size()) ? N[k.n] : DirectX::XMFLOAT3{ 0, 0, 1 };
                            v.TexCoord = (k.t >= 0 && (size_t)k.t < T.size()) ? T[k.t] : DirectX::XMFLOAT2{ 0, 0 };
                            const uint32_t vi = (uint32_t)verts.size(); verts.push_back(v); indices.push_back(vi); vm.emplace(k, vi);
                        }
                    }
                }
            }
            if (!mtlp.empty()) texPath = ReadDiffuseTexturePathFromMtl(mtlp);
            return !verts.empty() && !indices.empty();
        }

        bool CreateSolidTexture(ID3D11Device* dev, const DirectX::XMFLOAT4& col, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
        {
            const uint8_t p[4] = { (uint8_t)(col.x * 255), (uint8_t)(col.y * 255), (uint8_t)(col.z * 255), (uint8_t)(col.w * 255) };
            D3D11_TEXTURE2D_DESC td = { 1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
            D3D11_SUBRESOURCE_DATA id = { p, 4, 0 }; Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
            if (FAILED(dev->CreateTexture2D(&td, &id, &tex))) return false;
            return SUCCEEDED(dev->CreateShaderResourceView(tex.Get(), nullptr, &srv));
        }

        bool LoadTextureWithWIC(ID3D11Device* dev, const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
        {
            Microsoft::WRL::ComPtr<IWICImagingFactory> fact; ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fact));
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec; if (FAILED(fact->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &dec))) return false;
            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame; dec->GetFrame(0, &frame);
            Microsoft::WRL::ComPtr<IWICFormatConverter> conv; fact->CreateFormatConverter(&conv);
            conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
            UINT w = 0, h = 0; conv->GetSize(&w, &h); std::vector<uint8_t> pix((size_t)w * h * 4u);
            conv->CopyPixels(nullptr, w * 4u, (UINT)pix.size(), pix.data());
            D3D11_TEXTURE2D_DESC td = { w, h, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
            D3D11_SUBRESOURCE_DATA id = { pix.data(), w * 4u, 0 }; Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
            dev->CreateTexture2D(&td, &id, &tex); return SUCCEEDED(dev->CreateShaderResourceView(tex.Get(), nullptr, &srv));
        }

        bool LoadMeshResource(ID3D11Device* dev, const std::wstring& path, URenderer::FMeshResource& res)
        {
            if (!LoadObjMeshData(path, res.SourceVertices, res.SourceIndices, res.DiffuseTexturePath)) return false;
            float minX = 1e9f, minY = 1e9f, minZ = 1e9f, maxX = -1e9f, maxY = -1e9f, maxZ = -1e9f;
            for (const auto& v : res.SourceVertices) {
                minX = (std::min)(minX, v.Position.x); minY = (std::min)(minY, v.Position.y); minZ = (std::min)(minZ, v.Position.z);
                maxX = (std::max)(maxX, v.Position.x); maxY = (std::max)(maxY, v.Position.y); maxZ = (std::max)(maxZ, v.Position.z);
            }
            res.LocalCenter = { (minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f };
            D3D11_BUFFER_DESC vb = { (UINT)(res.SourceVertices.size() * sizeof(URenderer::FMeshVertex)), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA vd = { res.SourceVertices.data(), 0, 0 }; dev->CreateBuffer(&vb, &vd, &res.VertexBuffer);
            D3D11_BUFFER_DESC ib = { (UINT)(res.SourceIndices.size() * sizeof(uint32_t)), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
            D3D11_SUBRESOURCE_DATA id = { res.SourceIndices.data(), 0, 0 }; dev->CreateBuffer(&ib, &id, &res.IndexBuffer);
            if (!res.DiffuseTexturePath.empty()) LoadTextureWithWIC(dev, res.DiffuseTexturePath, res.DiffuseTextureView);
            res.IndexCount = (uint32_t)res.SourceIndices.size(); return true;
        }
    }

    URenderer::URenderer() = default;
    URenderer::~URenderer() = default;

    bool URenderer::Initialize(HWND InHW, int InW, int InH)
    {
        ViewportWidth = (uint32_t)InW; ViewportHeight = (uint32_t)InH;
        DXGI_SWAP_CHAIN_DESC sd = { { (UINT)InW, (UINT)InH, {0, 0}, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED }, {1, 0}, DXGI_USAGE_RENDER_TARGET_OUTPUT, 3, InHW, TRUE, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING };
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, nullptr, &Context))) return false;
        Context.As(&Context1);
        ComPtr<ID3D11Texture2D> bb; SwapChain->GetBuffer(0, IID_PPV_ARGS(&bb)); Device->CreateRenderTargetView(bb.Get(), nullptr, &MainRenderTargetView);
        D3D11_TEXTURE2D_DESC dd = { (UINT)InW, (UINT)InH, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0 };
        ComPtr<ID3D11Texture2D> dt; Device->CreateTexture2D(&dd, nullptr, &dt); Device->CreateDepthStencilView(dt.Get(), nullptr, &DepthStencilView);
        return CreateDefaultResources();
    }

    bool URenderer::CreateDefaultResources()
    {
        D3D11_BUFFER_DESC bfd = { sizeof(FPerFrameConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&bfd, nullptr, &BakePerFrameBuffer);
        D3D11_BUFFER_DESC bod = { sizeof(FPerObjectConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&bod, nullptr, &BakePerObjectBuffer);

        const char* ShaderSrc = R"(
            cbuffer PF : register(b0) { row_major float4x4 VP; float4 CR; float4 CU; float4 CP; };
            cbuffer PO : register(b1) { float4 R0; float4 R1; float4 R2; float4 PD; };
            cbuffer MD : register(b2) { float4 BC; };
            Texture2D DT : register(t0); SamplerState SS : register(s0);
            struct VI { float3 P : POSITION; float3 N : NORMAL; float2 T : TEXCOORD0; };
            struct PI { float4 P : SV_POSITION; float2 T : TEXCOORD0; };
            PI VSMain(VI i) { PI o; float4 lp = float4(i.P, 1.0f); float3 wp = float3(dot(lp, R0), dot(lp, R1), dot(lp, R2)); o.P = mul(float4(wp, 1.0f), VP); o.T = i.T; return o; }
            float4 PSMain(PI i) : SV_TARGET { float4 c = DT.Sample(SS, i.T) * BC; return float4(c.rgb, 1.0f); }
        )";
        ComPtr<ID3DBlob> VS, PS, Err;
        D3DCompile(ShaderSrc, strlen(ShaderSrc), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err);
        D3DCompile(ShaderSrc, strlen(ShaderSrc), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, &Err);
        Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &VertexShader);
        Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &PixelShader);
        D3D11_INPUT_ELEMENT_DESC lay[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
        Device->CreateInputLayout(lay, 3, VS->GetBufferPointer(), VS->GetBufferSize(), &InputLayout);
        D3D11_BUFFER_DESC pfd = { sizeof(FPerFrameConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&pfd, nullptr, &PerFrameBuffer);
        D3D11_BUFFER_DESC pod = { 64 * 1024 * 1024, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&pod, nullptr, &PerObjectBuffer);
        D3D11_BUFFER_DESC mcd = { sizeof(FMaterialConstants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&mcd, nullptr, &MaterialBuffer);
        D3D11_SAMPLER_DESC ssd = { D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP, 0, 1, D3D11_COMPARISON_NEVER, {0,0,0,0}, 0, D3D11_FLOAT32_MAX };
        Device->CreateSamplerState(&ssd, &DiffuseSamplerState);
        D3D11_RASTERIZER_DESC rd = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
        Device->CreateRasterizerState(&rd, &DefaultRasterizerState);
        D3D11_DEPTH_STENCIL_DESC dsd = { TRUE, D3D11_DEPTH_WRITE_MASK_ALL, D3D11_COMPARISON_LESS_EQUAL, FALSE, 0, 0, {}, {} };
        Device->CreateDepthStencilState(&dsd, &DefaultDepthStencilState);
        CreateSolidTexture(Device.Get(), { 1, 1, 1, 1 }, DefaultWhiteTextureView);
        const std::wstring base = Core::FPathManager::GetMeshPath();
        LoadMeshResource(Device.Get(), base + L"apple_mid.obj", MeshResources[0]);
        LoadMeshResource(Device.Get(), base + L"bitten_apple_mid.obj", MeshResources[1]);

        const char* BBShader = R"(
            cbuffer PF : register(b0) { row_major float4x4 VP; float4 CR; float4 CU; float4 CP; };
            cbuffer PO : register(b1) { float4 R0; float4 R1; float4 R2; float4 PD; };
            Texture2D SN : register(t0); SamplerState SS : register(s0);
            struct VI { float3 P : POSITION; float2 T : TEXCOORD0; };
            struct PI { float4 P : SV_POSITION; float2 T : TEXCOORD0; };
            PI VSMain(VI i) {
                PI o; float3 wp = float3(R0.w, R1.w, R2.w) + PD.xyz;
                float sx = length(float3(R0.x, R1.x, R2.x)), sy = length(float3(R0.y, R1.y, R2.y));
                float3 fp = wp + (i.P.x * sx * 2.5f * CR.xyz) + (i.P.z * sy * 2.5f * CU.xyz);
                o.P = mul(float4(fp, 1.0f), VP); o.T = i.T; return o;
            }
            float4 PSMain(PI i) : SV_TARGET {
                float3 wp = float3(R0.w, R1.w, R2.w);
                float3 of = normalize(float3(R0.x, R1.x, R2.x)), orgt = normalize(float3(R0.y, R1.y, R2.y));
                float3 vd = normalize(CP.xyz - wp); vd.z = 0; vd = normalize(vd);
                float ang = atan2(dot(vd, orgt), dot(vd, of));
                if (ang < 0.0f) ang += 6.2831853f;
                int fIdx = (int)round(ang / 1.570796f) % 4;
                float2 uv = (i.T * 0.5f) + float2(fIdx % 2, fIdx / 2) * 0.5f;
                float4 c = SN.Sample(SS, uv); if (c.a < 0.1f) discard; return c;
            }
        )";
        D3DCompile(BBShader, strlen(BBShader), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err);
        D3DCompile(BBShader, strlen(BBShader), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, &Err);
        Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &BillboardVS);
        Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &BillboardPS);
        D3D11_INPUT_ELEMENT_DESC bbl[] = { { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
        Device->CreateInputLayout(bbl, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &BillboardLayout);
        FBillboardVertex bbv[] = { { {-0.5f, 0, 0.5f}, {0, 0} }, { {0.5f, 0, 0.5f}, {1, 0} }, { {-0.5f, 0, -0.5f}, {0, 1} }, { {0.5f, 0, -0.5f}, {1, 1} } };
        D3D11_BUFFER_DESC bvb = { sizeof(bbv), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA bvd = { bbv, 0, 0 }; Device->CreateBuffer(&bvb, &bvd, &BillboardVB);
        uint32_t bbi[] = { 0, 1, 2, 2, 1, 3 };
        D3D11_BUFFER_DESC bib = { sizeof(bbi), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA bid = { bbi, 0, 0 }; Device->CreateBuffer(&bib, &bid, &BillboardIB);
        BakeImpostor(0); BakeImpostor(1); return true;
    }

    void URenderer::BakeImpostor(uint32_t MeshID)
    {
        const FMeshResource& res = MeshResources[MeshID];
        if (!res.VertexBuffer || !res.IndexBuffer || res.SourceVertices.empty()) return;
        const uint32_t FR = 256, AR = 512;
        D3D11_TEXTURE2D_DESC td = { AR, AR, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0 };
        ComPtr<ID3D11Texture2D> tex; Device->CreateTexture2D(&td, nullptr, &tex);
        ComPtr<ID3D11RenderTargetView> rtv; Device->CreateRenderTargetView(tex.Get(), nullptr, &rtv);
        ComPtr<ID3D11ShaderResourceView> srv; Device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
        D3D11_TEXTURE2D_DESC dd = { AR, AR, 1, 1, DXGI_FORMAT_D24_UNORM_S8_UINT, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0 };
        ComPtr<ID3D11Texture2D> dtx; Device->CreateTexture2D(&dd, nullptr, &dtx);
        ComPtr<ID3D11DepthStencilView> dsv; Device->CreateDepthStencilView(dtx.Get(), nullptr, &dsv);
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH({3, 0, res.LocalCenter.z}, {0, 0, res.LocalCenter.z}, {0, 0, 1});
        DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicLH(2.5f, 2.5f, 0.1f, 10.0f);
        float clr[4] = {0, 0, 0, 0}; Context->ClearRenderTargetView(rtv.Get(), clr); Context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        Context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        ComPtr<ID3D11RasterizerState> NoCull; D3D11_RASTERIZER_DESC rd = { D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE, 0, 0.0f, 0.0f, TRUE, FALSE, FALSE, FALSE };
        Device->CreateRasterizerState(&rd, &NoCull); Context->RSSetState(NoCull.Get()); Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (SUCCEEDED(Context->Map(BakePerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            FPerFrameConstants pf = {}; DirectX::XMStoreFloat4x4(&pf.VP, view * proj); std::memcpy(m.pData, &pf, sizeof(pf));
            Context->Unmap(BakePerFrameBuffer.Get(), 0);
        }
        if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            FMaterialConstants mc = { { 1, 1, 1, 1 } }; std::memcpy(m.pData, &mc, sizeof(mc));
            Context->Unmap(MaterialBuffer.Get(), 0);
        }
        Context->IASetInputLayout(InputLayout.Get()); Context->VSSetShader(VertexShader.Get(), nullptr, 0); Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        Context->VSSetConstantBuffers(0, 1, BakePerFrameBuffer.GetAddressOf()); Context->VSSetConstantBuffers(1, 1, BakePerObjectBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf()); Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());
        ID3D11ShaderResourceView* dsrv = res.DiffuseTextureView.Get(); Context->PSSetShaderResources(0, 1, &dsrv);
        UINT s = sizeof(FMeshVertex), o = 0; Context->IASetVertexBuffers(0, 1, res.VertexBuffer.GetAddressOf(), &s, &o); Context->IASetIndexBuffer(res.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        for (int f = 0; f < 4; ++f) {
            D3D11_VIEWPORT vp = { (float)((f % 2) * FR), (float)((f / 2) * FR), (float)FR, (float)FR, 0, 1 }; Context->RSSetViewports(1, &vp);
            DirectX::XMMATRIX objMat = DirectX::XMMatrixTranslation(-res.LocalCenter.x, -res.LocalCenter.y, -res.LocalCenter.z) * DirectX::XMMatrixRotationZ(f * DirectX::XM_PIDIV2);
            if (SUCCEEDED(Context->Map(BakePerObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
                FPerObjectConstants po = {}; DirectX::XMMATRIX sm = DirectX::XMMatrixTranspose(objMat);
                po.R0 = {DirectX::XMVectorGetX(sm.r[0]), DirectX::XMVectorGetY(sm.r[0]), DirectX::XMVectorGetZ(sm.r[0]), DirectX::XMVectorGetW(sm.r[0])};
                po.R1 = {DirectX::XMVectorGetX(sm.r[1]), DirectX::XMVectorGetY(sm.r[1]), DirectX::XMVectorGetZ(sm.r[1]), DirectX::XMVectorGetW(sm.r[1])};
                po.R2 = {DirectX::XMVectorGetX(sm.r[2]), DirectX::XMVectorGetY(sm.r[2]), DirectX::XMVectorGetZ(sm.r[2]), DirectX::XMVectorGetW(sm.r[2])};
                po.PD = {0,0,0,1}; std::memcpy(m.pData, &po, sizeof(po)); Context->Unmap(BakePerObjectBuffer.Get(), 0);
            }
            Context->DrawIndexed(res.IndexCount, 0, 0);
        }
        ImpostorResources[MeshID].SnapshotTexture = tex; ImpostorResources[MeshID].SnapshotSRV = srv; ImpostorResources[MeshID].bIsBaked = true;
        ID3D11RenderTargetView* nrt = nullptr; Context->OMSetRenderTargets(1, &nrt, nullptr);
    }

    void URenderer::BeginFrame()
    {
        const float c[4] = { 0.03f, 0.03f, 0.06f, 1.0f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), DepthStencilView.Get());
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), c);
        Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp = { 0, 0, (float)ViewportWidth, (float)ViewportHeight, 0, 1 };
        Context->RSSetViewports(1, &vp);
    }

    void URenderer::RenderScene(const Scene::USceneManager& InMgr)
    {
        const Scene::FSceneDataSOA* sd = InMgr.GetSceneData();
        if (!sd || sd->RenderCount == 0 || !PerObjectBuffer) return;
        const uint32_t Aligned = 256, Bulk = sd->RenderCount * Aligned, Cap = 64 * 1024 * 1024;
        D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;
        if (PerObjectRingBufferOffset + Bulk > Cap) { type = D3D11_MAP_WRITE_DISCARD; PerObjectRingBufferOffset = 0; }
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (FAILED(Context->Map(PerObjectBuffer.Get(), 0, type, 0, &m))) return;
        uint8_t* dst = static_cast<uint8_t*>(m.pData) + PerObjectRingBufferOffset;
        const uint32_t bo = PerObjectRingBufferOffset;
        uint32_t gc[20] = {0}, go[20] = {0}, to[20];
        for (uint32_t i = 0; i < sd->RenderCount; ++i) { uint32_t mid = sd->MeshIDs[sd->RenderQueue[i]]; if (mid < 20) gc[mid]++; }
        uint32_t cur = 0; for (int i = 0; i < 20; ++i) { go[i] = cur; cur += gc[i]; }
        std::memcpy(to, go, sizeof(go));
        for (uint32_t i = 0; i < sd->RenderCount; ++i) {
            uint32_t oid = sd->RenderQueue[i], mid = sd->MeshIDs[oid];
            if (mid < 20) {
                uint32_t sidx = to[mid]++; const auto& mat = sd->WorldMatrices[oid];
                FPerObjectConstants* d = (FPerObjectConstants*)(dst + (sidx * Aligned));
                DirectX::XMStoreFloat4(&d->R0, mat.Row0); DirectX::XMStoreFloat4(&d->R1, mat.Row1); DirectX::XMStoreFloat4(&d->R2, mat.Row2);
                d->PD = { MeshResources[mid % MAX_MESH_TYPES].LocalCenter.x, MeshResources[mid % MAX_MESH_TYPES].LocalCenter.y, MeshResources[mid % MAX_MESH_TYPES].LocalCenter.z, 1.0f };
            }
        }
        Context->Unmap(PerObjectBuffer.Get(), 0); PerObjectRingBufferOffset += Bulk;
        float aspect = (ViewportHeight == 0) ? 1.0f : (float)ViewportWidth / (float)ViewportHeight;
        DirectX::XMVECTOR cp = DirectX::XMLoadFloat3(&CameraState.Position);
        DirectX::XMVECTOR fwd = DirectX::XMVector3Normalize(DirectX::XMVectorSet(std::cos(CameraState.PitchRadians) * std::cos(CameraState.YawRadians), std::cos(CameraState.PitchRadians) * std::sin(CameraState.YawRadians), std::sin(CameraState.PitchRadians), 0));
        DirectX::XMVECTOR up = {0, 0, 1}; DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(cp, DirectX::XMVectorAdd(cp, fwd), up);
        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(CameraState.FOVDegrees), aspect, CameraState.NearClip, CameraState.FarClip);
        DirectX::XMVECTOR cr = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, fwd)), tu = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(fwd, cr));
        D3D11_MAPPED_SUBRESOURCE pm = {};
        if (SUCCEEDED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &pm))) {
            FPerFrameConstants pf = {}; DirectX::XMStoreFloat4x4(&pf.VP, view * proj);
            DirectX::XMStoreFloat4(&pf.CR, cr); DirectX::XMStoreFloat4(&pf.CU, tu); DirectX::XMStoreFloat4(&pf.CP, cp);
            std::memcpy(pm.pData, &pf, sizeof(pf)); Context->Unmap(PerFrameBuffer.Get(), 0);
        }
        if (SUCCEEDED(Context->Map(MaterialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &pm))) {
            FMaterialConstants mc = { { 1, 1, 1, 1 } }; std::memcpy(pm.pData, &mc, sizeof(mc)); Context->Unmap(MaterialBuffer.Get(), 0);
        }
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->VSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf()); Context->PSSetConstantBuffers(0, 1, PerFrameBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(2, 1, MaterialBuffer.GetAddressOf()); Context->PSSetSamplers(0, 1, DiffuseSamplerState.GetAddressOf());
        Context->RSSetState(DefaultRasterizerState.Get()); Context->OMSetDepthStencilState(DefaultDepthStencilState.Get(), 0);
        Context->IASetInputLayout(InputLayout.Get()); Context->VSSetShader(VertexShader.Get(), nullptr, 0); Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        for (uint32_t mid = 0; mid < MAX_MESH_TYPES; ++mid) {
            if (gc[mid] == 0) continue;
            const auto& r = MeshResources[mid]; ID3D11ShaderResourceView* srv = r.DiffuseTextureView.Get();
            Context->PSSetShaderResources(0, 1, &srv); UINT s = sizeof(FMeshVertex), o = 0;
            Context->IASetVertexBuffers(0, 1, r.VertexBuffer.GetAddressOf(), &s, &o); Context->IASetIndexBuffer(r.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            for (uint32_t i = go[mid]; i < go[mid] + gc[mid]; ++i) {
                UINT off = (bo + (i * Aligned)) / 16, cnt = Aligned / 16;
                if (Context1) Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                else Context->VSSetConstantBuffers(1, 1, PerObjectBuffer.GetAddressOf());
                Context->DrawIndexed(r.IndexCount, 0, 0);
            }
        }
        Context->IASetInputLayout(BillboardLayout.Get()); Context->VSSetShader(BillboardVS.Get(), nullptr, 0); Context->PSSetShader(BillboardPS.Get(), nullptr, 0);
        for (uint32_t mid = 0; mid < MAX_MESH_TYPES; ++mid) {
            uint32_t bid = mid + 10; if (gc[bid] == 0 || !ImpostorResources[mid].bIsBaked) continue;
            ID3D11ShaderResourceView* srv = ImpostorResources[mid].SnapshotSRV.Get();
            Context->PSSetShaderResources(0, 1, &srv); UINT s = sizeof(FBillboardVertex), o = 0;
            Context->IASetVertexBuffers(0, 1, BillboardVB.GetAddressOf(), &s, &o); Context->IASetIndexBuffer(BillboardIB.Get(), DXGI_FORMAT_R32_UINT, 0);
            for (uint32_t i = go[bid]; i < go[bid] + gc[bid]; ++i) {
                UINT off = (bo + (i * Aligned)) / 16, cnt = Aligned / 16;
                if (Context1) Context1->VSSetConstantBuffers1(1, 1, PerObjectBuffer.GetAddressOf(), &off, &cnt);
                else Context->VSSetConstantBuffers(1, 1, PerObjectBuffer.GetAddressOf());
                Context->DrawIndexed(6, 0, 0);
            }
        }
    }

    void URenderer::EndFrame() { SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING); }
}
