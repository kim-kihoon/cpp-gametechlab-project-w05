#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <Scene/SceneData.h>
#include <Core/PathManager.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstring>

namespace Graphics
{
    URenderer::URenderer() : CurrentBufferOffset(0) {}
    URenderer::~URenderer() {}

    bool URenderer::Initialize(HWND InWindowHandle, int InWidth, int InHeight)
    {
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

        // [디버깅 편의] 임시로 모든 모드에서 창 모드 사용 (클릭 불가 방지)
        SwapChainDesc.Windowed = TRUE;

        const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
        HRESULT Result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, 
            FeatureLevels, 1, D3D11_SDK_VERSION, &SwapChainDesc, &SwapChain, &Device, nullptr, &Context);

        if (FAILED(Result)) return false;
        Context.As(&Context1);

        ComPtr<ID3D11Texture2D> BackBuffer;
        SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
        Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &MainRenderTargetView);

        D3D11_TEXTURE2D_DESC DepthDesc = {};
        DepthDesc.Width = InWidth; DepthDesc.Height = InHeight;
        DepthDesc.MipLevels = 1; DepthDesc.ArraySize = 1;
        DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        DepthDesc.SampleDesc.Count = 1;
        DepthDesc.Usage = D3D11_USAGE_DEFAULT;
        DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> DepthBuffer;
        Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer);
        Device->CreateDepthStencilView(DepthBuffer.Get(), nullptr, &DepthStencilView);

        if (!CreateCircularBuffer()) return false;
        if (!CreateDefaultResources()) return false;

        return true;
    }

    bool URenderer::CreateCircularBuffer()
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth = TOTAL_BUFFER_INSTANCES * sizeof(Math::FPacked3x4Matrix);
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ConstantBuffer));
    }

    bool URenderer::CreateDefaultResources()
    {
        const char* ShaderSrc = R"(
            cbuffer PerObject : register(b0) { float4x3 World; };
            cbuffer PerFrame : register(b1) { float4x4 ViewProj; };
            struct VS_IN { float3 Pos : POSITION; float3 Norm : NORMAL; };
            struct PS_IN { float4 Pos : SV_POSITION; float3 Norm : NORMAL; };
            PS_IN VSMain(VS_IN i) {
                PS_IN o;
                float4x4 w = float4x4(float4(World[0],0), float4(World[1],0), float4(World[2],0), float4(0,0,0,1));
                o.Pos = mul(mul(float4(i.Pos,1), w), ViewProj);
                o.Norm = i.Norm;
                return o;
            }
            float4 PSMain(PS_IN i) : SV_TARGET {
                return float4(0.8, 0.1, 0.1, 1);
            }
        )";
        ComPtr<ID3DBlob> VS, PS, Err;
        D3DCompile(ShaderSrc, strlen(ShaderSrc), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &VS, &Err);
        D3DCompile(ShaderSrc, strlen(ShaderSrc), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &PS, nullptr);
        Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr, &VertexShader);
        Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr, &PixelShader);

        D3D11_INPUT_ELEMENT_DESC Layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        Device->CreateInputLayout(Layout, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &InputLayout);

        D3D11_BUFFER_DESC CBDesc = { 64, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
        Device->CreateBuffer(&CBDesc, nullptr, &PerFrameBuffer);

        float V[] = { -1,-1,-1, 0,0,-1, 1,-1,-1, 0,0,-1, 1,1,-1, 0,0,-1, -1,1,-1, 0,0,-1 };
        uint16_t I[] = { 0,2,1, 0,3,2 };
        D3D11_BUFFER_DESC VBD = { sizeof(V), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA VDI = { V, 0, 0 };
        Device->CreateBuffer(&VBD, &VDI, &CubeVertexBuffer);
        D3D11_BUFFER_DESC IBD = { sizeof(I), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
        D3D11_SUBRESOURCE_DATA IDI = { I, 0, 0 };
        Device->CreateBuffer(&IBD, &IDI, &CubeIndexBuffer);

        return true;
    }

    void URenderer::BeginFrame()
    {
        const float Color[4] = { 0.02f, 0.02f, 0.1f, 1.0f };
        Context->OMSetRenderTargets(1, MainRenderTargetView.GetAddressOf(), DepthStencilView.Get());
        Context->ClearRenderTargetView(MainRenderTargetView.Get(), Color);
        Context->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        if (CurrentBufferOffset + MAX_INSTANCES_PER_FRAME >= TOTAL_BUFFER_INSTANCES) CurrentBufferOffset = 0;
    }

    void URenderer::RenderScene(const Scene::USceneManager& InSceneManager)
    {
        const Scene::FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        // [진단] 렌더 큐가 비어있어도 무조건 테스트 삼각형을 그리도록 로직 변경
        
        // 1. 상태 설정
        ComPtr<ID3D11RasterizerState> RSState;
        D3D11_RASTERIZER_DESC RSDesc = {};
        RSDesc.FillMode = D3D11_FILL_SOLID;
        RSDesc.CullMode = D3D11_CULL_NONE;
        Device->CreateRasterizerState(&RSDesc, &RSState);
        Context->RSSetState(RSState.Get());

        // [핵심 진단] 뎁스를 끄고 그려봅니다. (여기서 보이면 뎁스 문제)
        ComPtr<ID3D11DepthStencilState> DSState;
        D3D11_DEPTH_STENCIL_DESC DSDesc = {};
        DSDesc.DepthEnable = FALSE; 
        Device->CreateDepthStencilState(&DSDesc, &DSState);
        Context->OMSetDepthStencilState(DSState.Get(), 0);

        uint32_t Str = 24, Off = 0;
        Context->IASetVertexBuffers(0, 1, CubeVertexBuffer.GetAddressOf(), &Str, &Off);
        Context->IASetIndexBuffer(CubeIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->IASetInputLayout(InputLayout.Get());
        Context->VSSetShader(VertexShader.Get(), nullptr, 0);
        Context->PSSetShader(PixelShader.Get(), nullptr, 0);
        
        D3D11_MAPPED_SUBRESOURCE Mapped;
        if (SUCCEEDED(Context->Map(PerFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped))) {
            // [진단] 행렬을 Identity로 설정 (화면 가득 채우는 큐브가 보여야 함)
            DirectX::XMMATRIX VP = DirectX::XMMatrixIdentity();
            std::memcpy(Mapped.pData, &VP, 64);
            Context->Unmap(PerFrameBuffer.Get(), 0);
        }
        Context->VSSetConstantBuffers(1, 1, PerFrameBuffer.GetAddressOf());

        // [진단] 첫 번째 행렬만 Identity로 덮어쓰고 딱 하나만 그려봅니다.
        Math::FPacked3x4Matrix IdentityMatrix;
        DirectX::XMMATRIX Identity = DirectX::XMMatrixIdentity();
        IdentityMatrix.Store(Identity);
        
        uint32_t BatchOffset = UpdateInstanceBufferBatch(&IdentityMatrix, 1);
        uint32_t ConstOff = BatchOffset * (sizeof(Math::FPacked3x4Matrix) / 16);
        uint32_t ConstCnt = sizeof(Math::FPacked3x4Matrix) / 16;
        Context1->VSSetConstantBuffers1(0, 1, ConstantBuffer.GetAddressOf(), &ConstOff, &ConstCnt);
        
        Context->DrawIndexed(6, 0, 0);
    }

    void URenderer::EndFrame() { SwapChain->Present(1, 0); }
    uint32_t URenderer::UpdateInstanceBufferBatch(const Math::FPacked3x4Matrix* M, uint32_t C) { return 0; }
}