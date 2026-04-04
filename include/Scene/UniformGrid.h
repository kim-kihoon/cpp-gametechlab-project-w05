#pragma once
#include <vector>
#include <array>
#include <Math/Frustum.h>
#include <Scene/SceneData.h>

namespace Scene
{
    /**
     * [성능 극대화] 셀 내 객체 목록을 위한 선형 데이터 구조.
     */
    struct FGridCell
    {
        Math::FBox CellBox;
        uint32_t StartIndex; // GlobalIndexBuffer 내 시작 위치
        uint32_t Count;      // 이 셀에 포함된 객체 수
    };

    class UUniformGrid
    {
    private:
        int Width, Height, Depth;
        float CellSize;
        float OriginX;
        float OriginY;
        float OriginZ;
        float InvCellSize;
        std::vector<FGridCell> Cells;
        FSceneDataSOA* SceneData;

        /** [Zero-Alloc] 모든 셀의 인덱스를 통합 관리하는 거대 버퍼 */
        static constexpr uint32_t MAX_GRID_ENTRIES = FSceneDataSOA::MAX_OBJECTS * 8;
        std::array<uint32_t, MAX_GRID_ENTRIES> GlobalIndexBuffer;
        std::array<uint32_t, FSceneDataSOA::MAX_OBJECTS> VisitTokens = {};
        uint32_t CurrentVisitToken = 1;
        uint32_t TotalEntryCount = 0;

        void ReconfigureToSceneBounds();
        void FallbackToSingleCell();

    public:
        UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData);

        /** [성능] 그리드 초기화 및 객체 재배치 준비 */
        void ClearGrid();

        void BuildGrid();

        // void InsertObject(uint32_t ObjectIndex);
        void QueryFrustum(const Math::FFrustum& Frustum, uint32_t* OutIndices, uint32_t& OutCount, uint32_t MaxCapacity);

        /** [최적화] 프러스텀 컬링, LOD(MeshID) 갱신, 렌더 큐 빌드를 한 번에 수행 */
        void CullingAndBuildRenderQueue(const Math::FFrustum& Frustum, const Math::FVector& CameraPos);

        bool Raycast(const Math::FRay& Ray, float MaxDistance, uint32_t& OutHitIndex, float& OutHitDistance);
    };
}
