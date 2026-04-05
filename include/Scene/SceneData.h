#pragma once
#include <Math/MathTypes.h>
#include <array>
#include <malloc.h>

namespace Scene
{
using namespace Math;

/**
 * [전략 반영] 5만 개의 사과를 위한 극한의 SIMD-SoA 구조체.
 * Culling과 Picking 루프에서 최상의 성능을 냅니다.
 */
struct alignas(64) FSceneDataSOA
{
    static constexpr uint32_t MAX_OBJECTS = 50000;

    // [SIMD Hot Path] 선언부에서 {}를 통해 모든 요소를 0으로 초기화 보장
    alignas(64) std::array<float, MAX_OBJECTS> MinX{};
    alignas(64) std::array<float, MAX_OBJECTS> MinY{};
    alignas(64) std::array<float, MAX_OBJECTS> MinZ{};
    alignas(64) std::array<float, MAX_OBJECTS> MaxX{};
    alignas(64) std::array<float, MAX_OBJECTS> MaxY{};
    alignas(64) std::array<float, MAX_OBJECTS> MaxZ{};

    // [Render Hot Path] 압축된 3x4 행렬 (기본 생성자에 의해 초기화)
    alignas(64) std::array<FPacked3x4Matrix, MAX_OBJECTS> WorldMatrices{};

    // Metadata
    alignas(64) std::array<uint32_t, MAX_OBJECTS> MeshIDs{};
    alignas(64) std::array<uint32_t, MAX_OBJECTS> BaseMeshIDs{};
    alignas(64) std::array<uint32_t, MAX_OBJECTS> MaterialIDs{};
    alignas(64) std::array<bool, MAX_OBJECTS> IsVisible{};

    // Render Queue
    alignas(64) std::array<uint32_t, MAX_OBJECTS> RenderQueue{};
    
    uint32_t TotalObjectCount = 0;
    uint32_t RenderCount = 0;

    void* operator new(size_t size)
    {
        return _aligned_malloc(size, 64);
    }
    void operator delete(void* p)
    {
        _aligned_free(p);
    }

    FSceneDataSOA() = default;

    inline void ResetRenderQueue()
    {
        RenderCount = 0;
    }
    inline void AddToRenderQueue(uint32_t Index)
    {
        RenderQueue[RenderCount++] = Index;
    }
};
} // namespace Scene
