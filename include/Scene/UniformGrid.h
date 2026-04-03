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
        std::vector<FGridCell> Cells;
        FSceneDataSOA* SceneData;

        /** [Zero-Alloc] 모든 셀의 인덱스를 통합 관리하는 거대 버퍼 */
        static constexpr uint32_t MAX_GRID_ENTRIES = FSceneDataSOA::MAX_OBJECTS * 4;
        std::array<uint32_t, MAX_GRID_ENTRIES> GlobalIndexBuffer;
        uint32_t TotalEntryCount = 0;

    public:
        UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData);

        /** [성능] 그리드 초기화 및 객체 재배치 준비 */
        void ClearGrid();

        /** [성능] 객체를 그리드에 삽입 (현재는 인프라 구축용으로 vector 잠시 활용 가능하나 최종은 2-pass 권장) */
        void InsertObject(uint32_t ObjectIndex);

        /** [Hot Path] 최적화된 컬링 루프 */
        void CullingAndBuildRenderQueue(const Math::FFrustum& Frustum);
    };
}
