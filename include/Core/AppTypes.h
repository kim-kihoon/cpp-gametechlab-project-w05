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
        float ElapsedTimeMilliseconds = 0.0f;
        float LastPickingTimeMilliseconds = 0.0f;
        float AccumulatedPickingTimeMilliseconds = 0.0f;
        uint32_t TotalPickingAttempts = 0;
        uint64_t FrameIndex = 0;
    };
}
