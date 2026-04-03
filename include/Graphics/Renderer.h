#pragma once
#include <d3d11_1.h>
#include <wrl/client.h>
#include <Math/MathTypes.h>
#include <Graphics/RendererTypes.h>

// [수정] Scene 네임스페이스 전방 선언 (컴파일러에게 존재를 알림)
namespace Scene { class USceneManager; }

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    /**
     * Verstappen Engine의 렌더링 핵심 클래스.
     */
    class URenderer
    {
    public:
        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void BeginFrame();
        void EndFrame();
        
        /** [Hot Path] SceneManager를 인자로 받아 렌더링 수행 */
        void RenderScene(const Scene::USceneManager& InSceneManager);

        /** [Hot Path] 행렬 데이터 일괄 전송 */
        uint32_t UpdateInstanceBufferBatch(const Math::FPacked3x4Matrix* InMatrices, uint32_t InCount);

        void SetDebugRenderSettings(const FDebugRenderSettings& InSettings) { DebugSettings = InSettings; }
        const FDebugRenderSettings& GetDebugRenderSettings() const { return DebugSettings; }

        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }
        ID3D11DeviceContext1* GetContext1() { return Context1.Get(); }

    private:
        bool CreateCircularBuffer();
        bool CreateDefaultResources();

    private:
        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        ComPtr<ID3D11DeviceContext1> Context1;
        ComPtr<IDXGISwapChain> SwapChain;
        ComPtr<ID3D11RenderTargetView> MainRenderTargetView;
        ComPtr<ID3D11DepthStencilView> DepthStencilView;

        // 리소스
        ComPtr<ID3D11VertexShader> VertexShader;
        ComPtr<ID3D11PixelShader> PixelShader;
        ComPtr<ID3D11InputLayout> InputLayout;
        ComPtr<ID3D11Buffer> CubeVertexBuffer;
        ComPtr<ID3D11Buffer> CubeIndexBuffer;
        ComPtr<ID3D11Buffer> PerFrameBuffer;

        static constexpr uint32_t MAX_INSTANCES_PER_FRAME = 50000;
        static constexpr uint32_t TOTAL_BUFFER_INSTANCES = MAX_INSTANCES_PER_FRAME * 3;
        
        ComPtr<ID3D11Buffer> ConstantBuffer;
        uint32_t CurrentBufferOffset = 0;

        FDebugRenderSettings DebugSettings;
    };
}