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

        // [SIMD Hot Path] AABB를 X, Y, Z 각각의 배열로 분리하여 8개씩 한꺼번에 로드 가능하게 함
        alignas(64) std::array<float, MAX_OBJECTS> MinX;
        alignas(64) std::array<float, MAX_OBJECTS> MinY;
        alignas(64) std::array<float, MAX_OBJECTS> MinZ;
        alignas(64) std::array<float, MAX_OBJECTS> MaxX;
        alignas(64) std::array<float, MAX_OBJECTS> MaxY;
        alignas(64) std::array<float, MAX_OBJECTS> MaxZ;

        // [Render Hot Path] 압축된 3x4 행렬 사용
        alignas(64) std::array<FPacked3x4Matrix, MAX_OBJECTS> WorldMatrices;
        
        // Metadata
        alignas(64) std::array<uint32_t, MAX_OBJECTS> MeshIDs;
        alignas(64) std::array<uint32_t, MAX_OBJECTS> BaseMeshIDs;
        alignas(64) std::array<uint32_t, MAX_OBJECTS> MaterialIDs;
        alignas(64) std::array<bool, MAX_OBJECTS> IsVisible;

        // Render Queue
        alignas(64) std::array<uint32_t, MAX_OBJECTS> RenderQueue;
        uint32_t TotalObjectCount = 0;      // 오브젝트의 총 개수 -> Add가 될 시 추가 필요.
        uint32_t RenderCount = 0;

        void* operator new(size_t size) { return _aligned_malloc(size, 64); }
        void operator delete(void* p) { _aligned_free(p); }

        FSceneDataSOA() : RenderCount(0) {
            IsVisible.fill(false);
        }

        inline void ResetRenderQueue() { RenderCount = 0; }
        inline void AddToRenderQueue(uint32_t Index) {
            RenderQueue[RenderCount++] = Index;
        }
    };
}
