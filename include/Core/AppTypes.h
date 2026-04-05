#pragma once
#include <cstdint>

namespace Core
{
    enum class ESpatialStructure : uint32_t
    {
        UniformGrid,
        SceneBVH
    };

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
        uint64_t TotalPickCount = 0;

        // Grid 전용 Picking 통계
        uint64_t GridLastPickingCycles = 0;
        uint64_t GridTotalPickingCycles = 0;
        uint64_t GridTotalPickCount = 0;

        // BVH 전용 Picking 통계
        uint64_t BVHLastPickingCycles = 0;
        uint64_t BVHTotalPickingCycles = 0;
        uint64_t BVHTotalPickCount = 0;

        uint64_t FrameIndex = 0;

        // BVH 및 충돌 검사 통계 (매 프레임 리셋)
        uint32_t BVHNodeTestCount = 0;
        uint32_t ObjectAABBTestCount = 0;

        // Uniform Grid 통계
        uint32_t GridCellTestCount = 0;
        uint32_t GridObjectAABBTestCount = 0;

        ESpatialStructure CurrentStructure = ESpatialStructure::UniformGrid;
    };

    // 전역 성능 지표
    extern FFramePerformanceMetrics GPerformanceMetrics;
}
