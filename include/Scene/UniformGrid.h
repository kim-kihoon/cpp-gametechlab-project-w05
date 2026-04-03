#pragma once
#include <vector>
#include <Math/Frustum.h>
#include <Scene/SceneData.h>

namespace Scene
{
    /**
     * 공간 분할 그리드의 개별 셀.
     */
    struct FGridCell
    {
        Math::FBox CellBox;
        std::vector<uint32_t> ObjectIndices;
        
        FGridCell() 
        {
            ObjectIndices.reserve(200);
        }
    };

    /**
     * 균등 그리드 기반 공간 분할 클래스.
     */
    class UUniformGrid
    {
    private:
        int Width, Height, Depth;
        float CellSize;
        std::vector<FGridCell> Cells;
        FSceneDataSOA* SceneData;

    public:
        UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData);

        /** 객체를 적절한 그리드 셀에 삽입 */
        void InsertObject(uint32_t ObjectIndex);

        /** Frustum 기반 Culling을 수행하고 가시 객체 목록 구축 */
        void CullingAndBuildRenderQueue(const Math::FFrustum& Frustum);
    };
}