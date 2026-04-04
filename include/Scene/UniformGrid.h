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

        template <typename NarrowPhaseFunc>
        bool Raycast(const Math::FRay& Ray, float MaxDistance, uint32_t& OutHitIndex, float& OutHitDistance, NarrowPhaseFunc NarrowPhaseTest);
    };

    template <typename NarrowPhaseFunc>
    bool UUniformGrid::Raycast(const Math::FRay& Ray, float MaxDistance, uint32_t& OutHitIndex, float& OutHitDistance, NarrowPhaseFunc NarrowPhaseTest)
    {
        if (!SceneData || Cells.empty()) return false;

        int GridX = std::clamp(static_cast<int>((Ray.Origin.x - OriginX) * InvCellSize), 0, Width - 1);
        int GridY = std::clamp(static_cast<int>((Ray.Origin.y - OriginY) * InvCellSize), 0, Height - 1);
        int GridZ = std::clamp(static_cast<int>((Ray.Origin.z - OriginZ) * InvCellSize), 0, Depth - 1);

        const int StepX = (Ray.Direction.x > 0.0f) ? 1 : -1;
        const int StepY = (Ray.Direction.y > 0.0f) ? 1 : -1;
        const int StepZ = (Ray.Direction.z > 0.0f) ? 1 : -1;

        const float tDeltaX = std::abs(CellSize * Ray.InvDirection.x);
        const float tDeltaY = std::abs(CellSize * Ray.InvDirection.y);
        const float tDeltaZ = std::abs(CellSize * Ray.InvDirection.z);

        float tMaxX = ((OriginX + (GridX + (StepX > 0 ? 1 : 0)) * CellSize) - Ray.Origin.x) * Ray.InvDirection.x;
        float tMaxY = ((OriginY + (GridY + (StepY > 0 ? 1 : 0)) * CellSize) - Ray.Origin.y) * Ray.InvDirection.y;
        float tMaxZ = ((OriginZ + (GridZ + (StepZ > 0 ? 1 : 0)) * CellSize) - Ray.Origin.z) * Ray.InvDirection.z;

        bool bHitFound = false;
        float ClosestHitT = MaxDistance;

        while (GridX >= 0 && GridX < Width && GridY >= 0 && GridY < Height && GridZ >= 0 && GridZ < Depth)
        {
            const FGridCell& Cell = Cells[GridX + (GridY * Width) + (GridZ * Width * Height)];

            if (Cell.Count > 0)
            {
                bool bHitInCell = false;
                float ClosestHitInCell = ClosestHitT;
                uint32_t BestIndexInCell = 0;

                const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];
                for (uint32_t i = 0; i < Cell.Count; ++i)
                {
                    const uint32_t Idx = Indices[i];

                    // --- 1. AABB 광역 검사 (Broad Phase) ---
                    float t1 = (SceneData->MinX[Idx] - Ray.Origin.x) * Ray.InvDirection.x;
                    float t2 = (SceneData->MaxX[Idx] - Ray.Origin.x) * Ray.InvDirection.x;
                    const float tMinX = (std::min)(t1, t2); const float tMaxX_Box = (std::max)(t1, t2);

                    t1 = (SceneData->MinY[Idx] - Ray.Origin.y) * Ray.InvDirection.y;
                    t2 = (SceneData->MaxY[Idx] - Ray.Origin.y) * Ray.InvDirection.y;
                    const float tMinY = (std::min)(t1, t2); const float tMaxY_Box = (std::max)(t1, t2);

                    t1 = (SceneData->MinZ[Idx] - Ray.Origin.z) * Ray.InvDirection.z;
                    t2 = (SceneData->MaxZ[Idx] - Ray.Origin.z) * Ray.InvDirection.z;
                    const float tMinZ = (std::min)(t1, t2); const float tMaxZ_Box = (std::max)(t1, t2);

                    const float tNear = (std::max)((std::max)(tMinX, tMinY), tMinZ);
                    const float tFar = (std::min)((std::min)(tMaxX_Box, tMaxY_Box), tMaxZ_Box);

                    // 박스에 맞았다면!
                    if (tNear <= tFar && tFar > 0.0f && tNear < ClosestHitInCell)
                    {
                        // --- 2. 정밀 검사 (Narrow Phase) ---
                        // 템플릿으로 전달되었기 때문에 여기서 컴파일러가 App.cpp의 람다 코드를 통째로 복사해서 붙여넣습니다. (간접 호출 X)
                        float PreciseDistance = ClosestHitInCell;
                        if (NarrowPhaseTest(Idx, PreciseDistance) && PreciseDistance < ClosestHitInCell)
                        {
                            ClosestHitInCell = PreciseDistance;
                            BestIndexInCell = Idx;
                            bHitInCell = true;
                        }
                    }
                }

                if (bHitInCell)
                {
                    OutHitIndex = BestIndexInCell;
                    OutHitDistance = ClosestHitInCell;
                    return true;
                }
            }

            if (tMaxX < tMaxY)
            {
                if (tMaxX < tMaxZ) { GridX += StepX; tMaxX += tDeltaX; }
                else { GridZ += StepZ; tMaxZ += tDeltaZ; }
            }
            else
            {
                if (tMaxY < tMaxZ) { GridY += StepY; tMaxY += tDeltaY; }
                else { GridZ += StepZ; tMaxZ += tDeltaZ; }
            }
        }
        return false;
    }
}
