#include <Scene/UniformGrid.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace Scene
{
    namespace
    {
        constexpr float MIN_CELL_SIZE = 2.0f;
        constexpr float MAX_CELL_SIZE = 8.0f;
        constexpr float CELL_SIZE_MULTIPLIER = 2.0f;
    }

    UUniformGrid::UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData)
        : Width(InW), Height(InH), Depth(InD), CellSize(InCellSize), SceneData(InSceneData)
        , OriginX(-(static_cast<float>(InW) * InCellSize) * 0.5f)
        , OriginY(-(static_cast<float>(InH) * InCellSize) * 0.5f)
        , OriginZ(0.0f)
        , TotalEntryCount(0)
    {
        InvCellSize = 1.0f / CellSize;
        Cells.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * static_cast<size_t>(Depth));
        ClearGrid();
    }

    void UUniformGrid::ReconfigureToSceneBounds()
    {
        if (!SceneData || SceneData->TotalObjectCount == 0)
        {
            Width = Height = Depth = 1;
            CellSize = 4.0f;
            InvCellSize = 1.0f / CellSize;
            OriginX = OriginY = OriginZ = 0.0f;
            Cells.resize(1);
            return;
        }

        float SceneMinX = FLT_MAX;
        float SceneMinY = FLT_MAX;
        float SceneMinZ = FLT_MAX;
        float SceneMaxX = -FLT_MAX;
        float SceneMaxY = -FLT_MAX;
        float SceneMaxZ = -FLT_MAX;
        float LargestObjectExtent = 0.0f;

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ++ObjectIndex)
        {
            SceneMinX = (std::min)(SceneMinX, SceneData->MinX[ObjectIndex]);
            SceneMinY = (std::min)(SceneMinY, SceneData->MinY[ObjectIndex]);
            SceneMinZ = (std::min)(SceneMinZ, SceneData->MinZ[ObjectIndex]);
            SceneMaxX = (std::max)(SceneMaxX, SceneData->MaxX[ObjectIndex]);
            SceneMaxY = (std::max)(SceneMaxY, SceneData->MaxY[ObjectIndex]);
            SceneMaxZ = (std::max)(SceneMaxZ, SceneData->MaxZ[ObjectIndex]);

            const float ExtentX = SceneData->MaxX[ObjectIndex] - SceneData->MinX[ObjectIndex];
            const float ExtentY = SceneData->MaxY[ObjectIndex] - SceneData->MinY[ObjectIndex];
            const float ExtentZ = SceneData->MaxZ[ObjectIndex] - SceneData->MinZ[ObjectIndex];
            LargestObjectExtent = (std::max)(LargestObjectExtent, (std::max)(ExtentX, (std::max)(ExtentY, ExtentZ)));
        }

        CellSize = std::clamp(LargestObjectExtent * CELL_SIZE_MULTIPLIER, MIN_CELL_SIZE, MAX_CELL_SIZE);
        InvCellSize = 1.0f / CellSize;

        OriginX = SceneMinX - CellSize;
        OriginY = SceneMinY - CellSize;
        OriginZ = SceneMinZ - CellSize;

        Width = (std::max)(1, static_cast<int>(std::ceil((SceneMaxX - SceneMinX) * InvCellSize)) + 2);
        Height = (std::max)(1, static_cast<int>(std::ceil((SceneMaxY - SceneMinY) * InvCellSize)) + 2);
        Depth = (std::max)(1, static_cast<int>(std::ceil((SceneMaxZ - SceneMinZ) * InvCellSize)) + 2);

        Cells.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * static_cast<size_t>(Depth));
    }

    void UUniformGrid::FallbackToSingleCell()
    {
        if (!SceneData) return;

        float SceneMinX = FLT_MAX;
        float SceneMinY = FLT_MAX;
        float SceneMinZ = FLT_MAX;
        float SceneMaxX = -FLT_MAX;
        float SceneMaxY = -FLT_MAX;
        float SceneMaxZ = -FLT_MAX;

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ++ObjectIndex)
        {
            SceneMinX = (std::min)(SceneMinX, SceneData->MinX[ObjectIndex]);
            SceneMinY = (std::min)(SceneMinY, SceneData->MinY[ObjectIndex]);
            SceneMinZ = (std::min)(SceneMinZ, SceneData->MinZ[ObjectIndex]);
            SceneMaxX = (std::max)(SceneMaxX, SceneData->MaxX[ObjectIndex]);
            SceneMaxY = (std::max)(SceneMaxY, SceneData->MaxY[ObjectIndex]);
            SceneMaxZ = (std::max)(SceneMaxZ, SceneData->MaxZ[ObjectIndex]);
        }

        Width = Height = Depth = 1;
        CellSize = (std::max)({ SceneMaxX - SceneMinX, SceneMaxY - SceneMinY, SceneMaxZ - SceneMinZ, 1.0f }) + 1.0f;
        InvCellSize = 1.0f / CellSize;
        OriginX = SceneMinX - 0.5f;
        OriginY = SceneMinY - 0.5f;
        OriginZ = SceneMinZ - 0.5f;

        Cells.resize(1);
        ClearGrid();
        Cells[0].StartIndex = 0;
        Cells[0].Count = SceneData->TotalObjectCount;
        TotalEntryCount = SceneData->TotalObjectCount;

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ++ObjectIndex)
        {
            GlobalIndexBuffer[ObjectIndex] = ObjectIndex;
        }
    }

    void UUniformGrid::ClearGrid()
    {
        TotalEntryCount = 0;
        for (auto& Cell : Cells)
        {
            Cell.Count = 0;
            Cell.StartIndex = 0;
        }

        for (int z = 0; z < Depth; ++z)
        {
            for (int y = 0; y < Height; ++y)
            {
                for (int x = 0; x < Width; ++x)
                {
                    const int Index = x + (y * Width) + (z * Width * Height);
                    Cells[Index].CellBox.Min = { OriginX + (x * CellSize), OriginY + (y * CellSize), OriginZ + (z * CellSize) };
                    Cells[Index].CellBox.Max = { OriginX + ((x + 1) * CellSize), OriginY + ((y + 1) * CellSize), OriginZ + ((z + 1) * CellSize) };
                }
            }
        }
    }

    void UUniformGrid::BuildGrid()
    {
        if (!SceneData) return;

        ReconfigureToSceneBounds();
        ClearGrid();

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ++ObjectIndex)
        {
            const int MinGridX = std::clamp(static_cast<int>((SceneData->MinX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
            const int MinGridY = std::clamp(static_cast<int>((SceneData->MinY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
            const int MinGridZ = std::clamp(static_cast<int>((SceneData->MinZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);
            const int MaxGridX = std::clamp(static_cast<int>((SceneData->MaxX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
            const int MaxGridY = std::clamp(static_cast<int>((SceneData->MaxY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
            const int MaxGridZ = std::clamp(static_cast<int>((SceneData->MaxZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

            for (int z = MinGridZ; z <= MaxGridZ; ++z)
            {
                for (int y = MinGridY; y <= MaxGridY; ++y)
                {
                    for (int x = MinGridX; x <= MaxGridX; ++x)
                    {
                        const int CellIndex = x + (y * Width) + (z * Width * Height);
                        ++Cells[CellIndex].Count;
                    }
                }
            }
        }

        TotalEntryCount = 0;
        for (auto& Cell : Cells)
        {
            Cell.StartIndex = TotalEntryCount;
            TotalEntryCount += Cell.Count;
            Cell.Count = 0;
        }

        if (TotalEntryCount > MAX_GRID_ENTRIES)
        {
            FallbackToSingleCell();
            return;
        }

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ++ObjectIndex)
        {
            const int MinGridX = std::clamp(static_cast<int>((SceneData->MinX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
            const int MinGridY = std::clamp(static_cast<int>((SceneData->MinY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
            const int MinGridZ = std::clamp(static_cast<int>((SceneData->MinZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);
            const int MaxGridX = std::clamp(static_cast<int>((SceneData->MaxX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
            const int MaxGridY = std::clamp(static_cast<int>((SceneData->MaxY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
            const int MaxGridZ = std::clamp(static_cast<int>((SceneData->MaxZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

            for (int z = MinGridZ; z <= MaxGridZ; ++z)
            {
                for (int y = MinGridY; y <= MaxGridY; ++y)
                {
                    for (int x = MinGridX; x <= MaxGridX; ++x)
                    {
                        const int CellIndex = x + (y * Width) + (z * Width * Height);
                        FGridCell& Cell = Cells[CellIndex];
                        GlobalIndexBuffer[Cell.StartIndex + Cell.Count] = ObjectIndex;
                        ++Cell.Count;
                    }
                }
            }
        }
    }

    void UUniformGrid::QueryFrustum(const Math::FFrustum& Frustum, uint32_t* OutIndices, uint32_t& OutCount, uint32_t MaxCapacity)
    {
        OutCount = 0;
        if (!SceneData || !OutIndices || MaxCapacity == 0 || Cells.empty()) return;

        ++CurrentVisitToken;
        if (CurrentVisitToken == 0)
        {
            VisitTokens.fill(0);
            CurrentVisitToken = 1;
        }

        const int MinGridX = std::clamp(static_cast<int>((Frustum.AABBMin.x - OriginX) * InvCellSize), 0, Width - 1);
        const int MinGridY = std::clamp(static_cast<int>((Frustum.AABBMin.y - OriginY) * InvCellSize), 0, Height - 1);
        const int MinGridZ = std::clamp(static_cast<int>((Frustum.AABBMin.z - OriginZ) * InvCellSize), 0, Depth - 1);
        const int MaxGridX = std::clamp(static_cast<int>((Frustum.AABBMax.x - OriginX) * InvCellSize), 0, Width - 1);
        const int MaxGridY = std::clamp(static_cast<int>((Frustum.AABBMax.y - OriginY) * InvCellSize), 0, Height - 1);
        const int MaxGridZ = std::clamp(static_cast<int>((Frustum.AABBMax.z - OriginZ) * InvCellSize), 0, Depth - 1);

        for (int z = MinGridZ; z <= MaxGridZ; ++z)
        {
            for (int y = MinGridY; y <= MaxGridY; ++y)
            {
                for (int x = MinGridX; x <= MaxGridX; ++x)
                {
                    const FGridCell& Cell = Cells[x + (y * Width) + (z * Width * Height)];
                    if (Cell.Count == 0) continue;

                    const Math::ECullingResult CellResult = Frustum.TestBox(Cell.CellBox);
                    if (CellResult == Math::ECullingResult::Outside) continue;

                    const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

                    if (CellResult == Math::ECullingResult::FullyInside)
                    {
                        for (uint32_t i = 0; i < Cell.Count; ++i)
                        {
                            const uint32_t Index = Indices[i];
                            if (VisitTokens[Index] == CurrentVisitToken) continue;

                            VisitTokens[Index] = CurrentVisitToken;
                            if (OutCount < MaxCapacity)
                            {
                                OutIndices[OutCount++] = Index;
                            }
                        }
                    }
                    else
                    {
                        for (uint32_t i = 0; i < Cell.Count; ++i)
                        {
                            const uint32_t Index = Indices[i];
                            if (VisitTokens[Index] == CurrentVisitToken) continue;

                            if (Frustum.TestSphere(
                                SceneData->CenterX[Index],
                                SceneData->CenterY[Index],
                                SceneData->CenterZ[Index],
                                SceneData->Radius[Index]) == Math::ECullingResult::Outside)
                            {
                                continue;
                            }
                            /*if (Frustum.TestBox(
                                SceneData->MinX[Index],
                                SceneData->MinY[Index],
                                SceneData->MinZ[Index],
                                SceneData->MaxX[Index],
                                SceneData->MaxY[Index],
                                SceneData->MaxZ[Index]) == Math::ECullingResult::Outside)
                            {
                                continue;
                            }*/

                            VisitTokens[Index] = CurrentVisitToken;
                            if (OutCount < MaxCapacity)
                            {
                                OutIndices[OutCount++] = Index;
                            }
                        }
                    }
                }
            }
        }
    }
}
