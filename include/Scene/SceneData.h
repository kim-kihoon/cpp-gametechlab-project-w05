#pragma once
#include <Math/MathTypes.h>

namespace ExtremeScene
{
    using namespace ExtremeMath;

    // 5만 개의 오브젝트를 다루기 위한 완벽한 SOA (Structure of Arrays)
    // 개별 객체의 데이터(AABB, Transform 등)를 메모리에 연속적으로 배치하여 캐시 미스 방지.
    // L1 캐시 라인 크기인 64-Byte 경계에 맞춰 배열의 시작을 강제 정렬합니다.
    struct SceneDataSOA
    {
        // 5만 개로 고정. 렌더링 루프에서는 절대 std::vector의 크기가 변하지 않습니다. (Zero-Allocation)
        static constexpr uint32_t MAX_OBJECTS = 50000;

        // [초격차] 캐시 라인 강제 정렬(Cache Line Alignment) - 배열 자체의 시작점을 캐시 라인에 맞춤
        __declspec(align(64)) AlignedMatrix WorldMatrices[MAX_OBJECTS];
        __declspec(align(64)) AlignedAABB BoundingBoxes[MAX_OBJECTS];
        
        // Culling 결과 및 Picking을 위한 활성화 여부
        __declspec(align(64)) bool IsVisible[MAX_OBJECTS]; // Render Queue로 대체 가능하지만 빠른 필터링을 위해 유지
        
        // Material & Mesh ID (상태 정렬, State Sorting 용)
        __declspec(align(64)) uint32_t MeshIDs[MAX_OBJECTS];
        __declspec(align(64)) uint32_t MaterialIDs[MAX_OBJECTS];

        // 렌더링을 위해 가시성 판정이 끝난 객체의 인덱스만 모아두는 큐 (매 프레임 clear)
        std::vector<uint32_t> RenderQueue;

        SceneDataSOA()
        {
            // 메모리 재할당(Re-allocation) 금지! 처음부터 5만개 예약.
            RenderQueue.reserve(MAX_OBJECTS);
        }

        // 초기화 시 모든 데이터를 SOA 배열에 밀어넣음
        void Initialize(uint32_t count)
        {
            // 실제 생성되는 객체 수만큼 초기화 로직 구현 (데이터 로딩은 에셋 관리자가 담당)
        }

        // 매 렌더링 프레임 시작 전 호출
        inline void ResetRenderQueue()
        {
            RenderQueue.clear(); // O(1) 수준으로 빠름. 메모리는 반환하지 않음.
        }
    };
}