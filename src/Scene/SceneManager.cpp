#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Scene
{
    namespace
    {
        constexpr float DEFAULT_BOUNDING_BOX_EXTENT = 50.0f;
    }

    USceneManager::USceneManager() {}
    USceneManager::~USceneManager() {}

    void USceneManager::Initialize()
    {
        SceneData = std::make_unique<FSceneDataSOA>();
        Grid = std::make_unique<UUniformGrid>(50, 50, 20, 100.0f, SceneData.get());
        ResetScene();
    }

    void USceneManager::Update(float DeltaTime)
    {
        (void)DeltaTime;
        UpdateVisibleObjectCount();
    }

    void USceneManager::ResetScene()
    {
        if (!SceneData)
        {
            return;
        }

        SceneStatistics = {};
        SceneData->ResetRenderQueue();
        SceneData->IsVisible.fill(false);
        ResetSelectionState();
    }

    bool USceneManager::SpawnStaticMesh(const FSceneSpawnRequest& InRequest)
    {
        if (!SceneData || SceneStatistics.TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS)
        {
            return false;
        }

        const uint32_t ObjectIndex = SceneStatistics.TotalObjectCount;
        SceneData->WorldMatrices[ObjectIndex].Store(InRequest.WorldMatrix);

        const DirectX::XMVECTOR Translation = InRequest.WorldMatrix.r[3];
        const float CenterX = DirectX::XMVectorGetX(Translation);
        const float CenterY = DirectX::XMVectorGetY(Translation);
        const float CenterZ = DirectX::XMVectorGetZ(Translation);

        SceneData->MinX[ObjectIndex] = CenterX - DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MinY[ObjectIndex] = CenterY - DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MinZ[ObjectIndex] = CenterZ - DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MaxX[ObjectIndex] = CenterX + DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MaxY[ObjectIndex] = CenterY + DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MaxZ[ObjectIndex] = CenterZ + DEFAULT_BOUNDING_BOX_EXTENT;
        SceneData->MeshIDs[ObjectIndex] = InRequest.MeshID;
        SceneData->MaterialIDs[ObjectIndex] = InRequest.MaterialID;
        SceneData->IsVisible[ObjectIndex] = true;

        ++SceneStatistics.TotalObjectCount;
        UpdateVisibleObjectCount();

        if (Grid)
        {
            Grid->InsertObject(ObjectIndex);
        }

        return true;
    }

    void USceneManager::SpawnStaticMeshGrid(const FSceneGridSpawnRequest& InRequest)
    {
        for (uint32_t DepthIndex = 0; DepthIndex < InRequest.Depth; ++DepthIndex)
        {
            for (uint32_t HeightIndex = 0; HeightIndex < InRequest.Height; ++HeightIndex)
            {
                for (uint32_t WidthIndex = 0; WidthIndex < InRequest.Width; ++WidthIndex)
                {
                    FSceneSpawnRequest SpawnRequest;
                    SpawnRequest.MeshID = InRequest.MeshID;
                    SpawnRequest.MaterialID = InRequest.MaterialID;
                    SpawnRequest.WorldMatrix = DirectX::XMMatrixTranslation(
                        static_cast<float>(WidthIndex) * InRequest.Spacing,
                        static_cast<float>(HeightIndex) * InRequest.Spacing,
                        static_cast<float>(DepthIndex) * InRequest.Spacing);

                    if (!SpawnStaticMesh(SpawnRequest))
                    {
                        return;
                    }
                }
            }
        }
    }

    bool USceneManager::SaveSceneBinary(const std::wstring& InFilePath) const
    {
        if (!SceneData || InFilePath.empty())
        {
            return false;
        }

        std::ofstream OutputStream(std::filesystem::path(InFilePath), std::ios::binary | std::ios::trunc);
        if (!OutputStream.is_open())
        {
            return false;
        }

        const uint32_t ObjectCount = SceneStatistics.TotalObjectCount;
        OutputStream.write(reinterpret_cast<const char*>(&ObjectCount), sizeof(ObjectCount));

        if (ObjectCount > 0)
        {
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MinX.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MinY.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MinZ.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MaxX.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MaxY.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MaxZ.data()), sizeof(float) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->WorldMatrices.data()), sizeof(Math::FPacked3x4Matrix) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MeshIDs.data()), sizeof(uint32_t) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->MaterialIDs.data()), sizeof(uint32_t) * ObjectCount);
            OutputStream.write(reinterpret_cast<const char*>(SceneData->IsVisible.data()), sizeof(bool) * ObjectCount);
        }

        return OutputStream.good();
    }

    bool USceneManager::LoadSceneBinary(const std::wstring& InFilePath)
    {
        if (!SceneData || InFilePath.empty())
        {
            return false;
        }

        std::ifstream InputStream(std::filesystem::path(InFilePath), std::ios::binary);
        if (!InputStream.is_open())
        {
            return false;
        }

        uint32_t ObjectCount = 0;
        InputStream.read(reinterpret_cast<char*>(&ObjectCount), sizeof(ObjectCount));
        if (!InputStream.good() || ObjectCount > FSceneDataSOA::MAX_OBJECTS)
        {
            return false;
        }

        ResetScene();

        if (ObjectCount > 0)
        {
            InputStream.read(reinterpret_cast<char*>(SceneData->MinX.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MinY.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MinZ.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MaxX.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MaxY.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MaxZ.data()), sizeof(float) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->WorldMatrices.data()), sizeof(Math::FPacked3x4Matrix) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MeshIDs.data()), sizeof(uint32_t) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->MaterialIDs.data()), sizeof(uint32_t) * ObjectCount);
            InputStream.read(reinterpret_cast<char*>(SceneData->IsVisible.data()), sizeof(bool) * ObjectCount);
        }

        if (!InputStream.good() && !InputStream.eof())
        {
            return false;
        }

        SceneStatistics.TotalObjectCount = ObjectCount;
        UpdateVisibleObjectCount();
        return true;
    }

    bool USceneManager::SelectObject(uint32_t InObjectIndex)
    {
        if (!SceneData || InObjectIndex >= SceneStatistics.TotalObjectCount)
        {
            return false;
        }

        SelectionData.bHasSelection = true;
        SelectionData.ObjectIndex = InObjectIndex;
        SelectionData.MeshID = SceneData->MeshIDs[InObjectIndex];
        SelectionData.MaterialID = SceneData->MaterialIDs[InObjectIndex];
        return true;
    }

    void USceneManager::ClearSelection()
    {
        ResetSelectionState();
    }

    void USceneManager::UpdateVisibleObjectCount()
    {
        if (!SceneData)
        {
            SceneStatistics.VisibleObjectCount = 0;
            return;
        }

        uint32_t VisibleCount = 0;
        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneStatistics.TotalObjectCount; ++ObjectIndex)
        {
            if (SceneData->IsVisible[ObjectIndex])
            {
                ++VisibleCount;
            }
        }

        SceneStatistics.VisibleObjectCount = VisibleCount;
    }

    void USceneManager::ResetSelectionState()
    {
        SelectionData = {};
    }
}
