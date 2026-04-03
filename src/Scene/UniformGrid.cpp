#include <Scene/UniformGrid.h>
#include <algorithm>

namespace Scene
{
    UUniformGrid::UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData)
        : Width(InW), Height(InH), Depth(InD), CellSize(InCellSize), SceneData(InSceneData)
        , TotalEntryCount(0)
    {
        Cells.resize(Width * Height * Depth);
        ClearGrid();
    }

    void UUniformGrid::ClearGrid()
    {
        TotalEntryCount = 0;
        for (auto& Cell : Cells)
        {
            Cell.Count = 0;
            Cell.StartIndex = 0;
        }
        
        // 각 Cell의 AABB 초기화 (좌표계 기준)
        for (int z = 0; z < Depth; ++z)
        {
            for (int y = 0; y < Height; ++y)
            {
                for (int x = 0; x < Width; ++x)
                {
                    int Index = x + (y * Width) + (z * Width * Height);
                    Cells[Index].CellBox.Min = { x * CellSize, y * CellSize, z * CellSize };
                    Cells[Index].CellBox.Max = { (x + 1) * CellSize, (y + 1) * CellSize, (z + 1) * CellSize };
                }
            }
        }
    }

    void UUniformGrid::InsertObject(uint32_t ObjectIndex)
    {
        if (!SceneData || TotalEntryCount >= MAX_GRID_ENTRIES) return;

        // [성능] 힙 메모리 접근 최소화 (로컬 캐싱)
        const float OXMin = SceneData->MinX[ObjectIndex];
        const float OYMin = SceneData->MinY[ObjectIndex];
        const float OZMin = SceneData->MinZ[ObjectIndex];
        const float OXMax = SceneData->MaxX[ObjectIndex];
        const float OYMax = SceneData->MaxY[ObjectIndex];
        const float OZMax = SceneData->MaxZ[ObjectIndex];

        float CX = (OXMin + OXMax) * 0.5f;
        float CY = (OYMin + OYMax) * 0.5f;
        float CZ = (OZMin + OZMax) * 0.5f;

        int GX = std::clamp(static_cast<int>(CX / CellSize), 0, Width - 1);
        int GY = std::clamp(static_cast<int>(CY / CellSize), 0, Height - 1);
        int GZ = std::clamp(static_cast<int>(CZ / CellSize), 0, Depth - 1);

        int CellIndex = GX + (GY * Width) + (GZ * Width * Height);
        FGridCell& Cell = Cells[CellIndex];

        if (Cell.Count == 0)
        {
            Cell.StartIndex = TotalEntryCount;
        }
        
        GlobalIndexBuffer[TotalEntryCount++] = ObjectIndex;
        Cell.Count++;
    }

    void UUniformGrid::CullingAndBuildRenderQueue(const Math::FFrustum& Frustum)
    {
        if (!SceneData) return;
        
        SceneData->ResetRenderQueue();
        SceneData->IsVisible.fill(false);

        for (const auto& Cell : Cells)
        {
            if (Cell.Count == 0) continue;

            Math::ECullingResult CellRes = Frustum.TestBox(Cell.CellBox);

            if (CellRes == Math::ECullingResult::Outside) continue;

            const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

            if (CellRes == Math::ECullingResult::FullyInside)
            {
                // [성능] Fully Inside Skip: 개별 검사 없이 즉시 추가
                for (uint32_t i = 0; i < Cell.Count; ++i)
                {
                    uint32_t Idx = Indices[i];
                    if (!SceneData->IsVisible[Idx])
                    {
                        SceneData->AddToRenderQueue(Idx);
                        SceneData->IsVisible[Idx] = true;
                    }
                }
            }
            else // Intersecting
            {
                // [성능] SOA 데이터를 레지스터에서 직접 읽어 검사 (Stack Traffic 0)
                for (uint32_t i = 0; i < Cell.Count; ++i)
                {
                    uint32_t Idx = Indices[i];
                    if (SceneData->IsVisible[Idx]) continue;

                    if (Frustum.TestBox(
                        SceneData->MinX[Idx], SceneData->MinY[Idx], SceneData->MinZ[Idx],
                        SceneData->MaxX[Idx], SceneData->MaxY[Idx], SceneData->MaxZ[Idx]) 
                        != Math::ECullingResult::Outside)
                    {
                        SceneData->AddToRenderQueue(Idx);
                        SceneData->IsVisible[Idx] = true;
                    }
                }
            }
        }
    }
}