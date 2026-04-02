#pragma once
#include <vector>
#include <Math/Frustum.h>
#include <Scene/SceneData.h>

namespace ExtremeScene
{
    // Uniform Grid의 1개 Cell 공간. 
    // 객체의 실제 데이터는 SceneDataSOA에 있으므로, 여기서는 가벼운 인덱스만 보유합니다.
    struct GridCell
    {
        ExtremeMath::AlignedAABB CellAABB; // 이 Cell 자체의 AABB (Frustum 검사용)
        std::vector<uint32_t> ObjectIndices; // Cell에 포함된 객체들의 인덱스 목록
        
        GridCell() {
            ObjectIndices.reserve(200); // 할당 재발생을 막기 위해 넉넉히 예약
        }
    };

    class UniformGrid
    {
    private:
        int m_Width, m_Height, m_Depth;
        float m_CellSize;
        std::vector<GridCell> m_Cells;
        
        // 5만개의 실제 데이터를 보유한 절대적인 원본
        SceneDataSOA* m_pSceneData; 

    public:
        UniformGrid(int w, int h, int d, float cellSize, SceneDataSOA* sceneData)
            : m_Width(w), m_Height(h), m_Depth(d), m_CellSize(cellSize), m_pSceneData(sceneData)
        {
            m_Cells.resize(w * h * d);
            // 각 Cell의 AABB(CellAABB)를 초기화하는 로직 필요 (w, h, d 위치 기반)
        }

        // 특정 객체(Index)를 Grid에 삽입
        void InsertObject(uint32_t objectIndex)
        {
            // objectIndex를 이용해 m_pSceneData->BoundingBoxes[objectIndex]를 가져옴
            // 해당 AABB가 걸치는 모든 Cell에 인덱스 추가 (루프)
        }

        // [초격차] 극한의 Frustum Culling 엔진
        void CullingAndBuildRenderQueue(const ExtremeMath::OptimizedFrustum& frustum)
        {
            m_pSceneData->ResetRenderQueue();

            for (const auto& cell : m_Cells)
            {
                if (cell.ObjectIndices.empty()) continue;

                // 1. 먼저 Cell 자체를 Frustum과 검사
                ExtremeMath::CullingResult cellRes = frustum.TestAABB(cell.CellAABB);

                if (cellRes == ExtremeMath::CullingResult::Outside)
                {
                    // 이 Cell에 속한 모든 사과(500개든 1000개든)를 단 1번의 검사로 폐기(Skip)
                    continue;
                }
                else if (cellRes == ExtremeMath::CullingResult::FullyInside)
                {
                    // [초격차] 완전히 안에 들어옴! 내부 객체들의 AABB 검사를 전면 생략하고 무조건 RenderQueue로 직행
                    for (uint32_t idx : cell.ObjectIndices)
                    {
                        m_pSceneData->RenderQueue.push_back(idx);
                    }
                }
                else // Intersecting (Cell이 Frustum 경계에 걸침)
                {
                    // 어쩔 수 없이 내부에 있는 객체들만 개별 AABB 검사 진행 (여기가 유일하게 연산이 일어나는 곳)
                    for (uint32_t idx : cell.ObjectIndices)
                    {
                        const auto& objAABB = m_pSceneData->BoundingBoxes[idx];
                        if (frustum.TestAABB(objAABB) != ExtremeMath::CullingResult::Outside)
                        {
                            m_pSceneData->RenderQueue.push_back(idx);
                        }
                    }
                }
            }
        }
    };
}