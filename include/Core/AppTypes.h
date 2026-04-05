#pragma once
#include <cstdint>

namespace Core
{
    /**
     * 프레임 단위 성능 계측 결과를 저장하는 구조체.
     */
    struct FFramePerformanceMetrics
    {
        float DeltaTimeSeconds = 0.0f;
        float FramesPerSecond = 0.0f;
        
        // Picking 관련 정밀 계측 (Cycles 단위)
        uint64_t LastPickingCycles = 0;
        uint64_t TotalPickingCycles = 0;

        uint64_t FrameIndex = 0;

        uint64_t RenderedObjectCount = 0;
    };
}
