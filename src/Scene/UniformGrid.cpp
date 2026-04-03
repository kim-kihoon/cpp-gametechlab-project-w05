#include <Scene/UniformGrid.h>

namespace Scene
{
    UUniformGrid::UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData)
        : Width(InW)
        , Height(InH)
        , Depth(InD)
        , CellSize(InCellSize)
        , SceneData(InSceneData)
    {
        Cells.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * static_cast<size_t>(Depth));
    }

    void UUniformGrid::InsertObject(uint32_t ObjectIndex)
    {
        if (Cells.empty())
        {
            return;
        }

        FGridCell& RootCell = Cells.front();
        if (RootCell.ObjectIndices.size() < RootCell.ObjectIndices.capacity())
        {
            RootCell.ObjectIndices.push_back(ObjectIndex);
        }
    }

    void UUniformGrid::CullingAndBuildRenderQueue(const Math::FFrustum& Frustum)
    {
        (void)Frustum;

        if (!SceneData)
        {
            return;
        }

        SceneData->ResetRenderQueue();
        for (uint32_t ObjectIndex = 0; ObjectIndex < FSceneDataSOA::MAX_OBJECTS; ++ObjectIndex)
        {
            if (SceneData->IsVisible[ObjectIndex])
            {
                SceneData->AddToRenderQueue(ObjectIndex);
            }
        }
    }
}
