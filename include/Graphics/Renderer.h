#pragma once
#include <d3d11_1.h>
#include <wrl/client.h>
#include <Math/MathTypes.h>
#include <Graphics/RendererTypes.h>

namespace Graphics
{
    using Microsoft::WRL::ComPtr;

    /**
     * Verstappen Engine의 렌더링 핵심 클래스.
     * 5만 번의 드로우 콜 오버헤드를 극복하기 위해 일괄 맵핑 방식을 사용함.
     */
    class URenderer
    {
    public:
        URenderer();
        ~URenderer();

        bool Initialize(HWND InWindowHandle, int InWidth, int InHeight);
        void BeginFrame();
        void EndFrame();
        
        /** 
         * [Hot Path] 가시 객체들의 행렬을 한 번에 GPU 버퍼로 전송 (성능 극대화)
         * @param InMatrices 행렬 데이터 시작 주소
         * @param InCount 전송할 객체 개수
         * @return 버퍼 내 시작 인덱스
         */
        uint32_t UpdateInstanceBufferBatch(const Math::FPacked3x4Matrix* InMatrices, uint32_t InCount);

        void SetDebugRenderSettings(const FDebugRenderSettings& InSettings) { DebugSettings = InSettings; }
        const FDebugRenderSettings& GetDebugRenderSettings() const { return DebugSettings; }

        ID3D11Device* GetDevice() { return Device.Get(); }
        ID3D11DeviceContext* GetContext() { return Context.Get(); }
        ID3D11DeviceContext1* GetContext1() { return Context1.Get(); }

    private:
        bool CreateCircularBuffer();

    private:
        ComPtr<ID3D11Device> Device;
        ComPtr<ID3D11DeviceContext> Context;
        ComPtr<ID3D11DeviceContext1> Context1;
        ComPtr<IDXGISwapChain> SwapChain;
        ComPtr<ID3D11RenderTargetView> MainRenderTargetView;

        static constexpr uint32_t MAX_INSTANCES_PER_FRAME = 50000;
        static constexpr uint32_t TOTAL_BUFFER_INSTANCES = MAX_INSTANCES_PER_FRAME * 3;
        
        ComPtr<ID3D11Buffer> ConstantBuffer;
        uint32_t CurrentBufferOffset = 0;

        FDebugRenderSettings DebugSettings;
    };
}