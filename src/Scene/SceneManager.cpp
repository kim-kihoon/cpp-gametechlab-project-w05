#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Scene
{
    namespace
    {
        constexpr float DEFAULT_HALF_EXTENT = 0.5f;

        void WriteBounds(FSceneDataSOA& InSceneData, uint32_t InIndex, const Math::FBox& InBounds)
        {
            InSceneData.MinX[InIndex] = InBounds.Min.x;
            InSceneData.MinY[InIndex] = InBounds.Min.y;
            InSceneData.MinZ[InIndex] = InBounds.Min.z;
            InSceneData.MaxX[InIndex] = InBounds.Max.x;
            InSceneData.MaxY[InIndex] = InBounds.Max.y;
            InSceneData.MaxZ[InIndex] = InBounds.Max.z;
        }
    }

    USceneManager::USceneManager() {}
    USceneManager::~USceneManager() {}

    void USceneManager::Initialize()
    {
        SceneData = std::make_unique<FSceneDataSOA>();
        Grid = std::make_unique<UUniformGrid>(50, 50, 20, 10.0f, SceneData.get());
        ResetScene();
    }

    void USceneManager::Update(float DeltaTime) {}

    void USceneManager::ResetScene()
    {
        if (!SceneData) return;
        SceneStatistics = {};
        SceneData->ResetRenderQueue();
        SceneData->IsVisible.fill(false);
        ResetSelectionState();
        if (Grid) Grid->ClearGrid();
    }

    bool USceneManager::SpawnStaticMesh(const FSceneSpawnRequest& InRequest)
    {
        if (!SceneData || SceneStatistics.TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) return false;

        const uint32_t Index = SceneStatistics.TotalObjectCount;
        SceneData->WorldMatrices[Index].Store(InRequest.WorldMatrix);
        
        const DirectX::XMVECTOR Translation = InRequest.WorldMatrix.r[3];
        float CX = DirectX::XMVectorGetX(Translation);
        float CY = DirectX::XMVectorGetY(Translation);
        float CZ = DirectX::XMVectorGetZ(Translation);
        
        SceneData->MinX[Index] = CX - DEFAULT_HALF_EXTENT;
        SceneData->MinY[Index] = CY - DEFAULT_HALF_EXTENT;
        SceneData->MinZ[Index] = CZ - DEFAULT_HALF_EXTENT;
        SceneData->MaxX[Index] = CX + DEFAULT_HALF_EXTENT;
        SceneData->MaxY[Index] = CY + DEFAULT_HALF_EXTENT;
        SceneData->MaxZ[Index] = CZ + DEFAULT_HALF_EXTENT;

        SceneData->MeshIDs[Index] = InRequest.MeshID;
        SceneData->MaterialIDs[Index] = InRequest.MaterialID;
        SceneData->IsVisible[Index] = true;

        SceneStatistics.TotalObjectCount++;
        if (Grid) Grid->InsertObject(Index);

        return true;
    }

    void USceneManager::SpawnStaticMeshGrid(const FSceneGridSpawnRequest& InRequest)
    {
        if (Grid) Grid->ClearGrid();

        for (uint32_t Z = 0; Z < InRequest.Depth; ++Z)
        {
            for (uint32_t Y = 0; Y < InRequest.Height; ++Y)
            {
                for (uint32_t X = 0; X < InRequest.Width; ++X)
                {
                    if (SceneStatistics.TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) break;

                    const uint32_t Index = SceneStatistics.TotalObjectCount;
                    float PosX = X * InRequest.Spacing;
                    float PosY = Y * InRequest.Spacing;
                    float PosZ = Z * InRequest.Spacing;
                    
                    SceneData->WorldMatrices[Index].Store(DirectX::XMMatrixTranslation(PosX, PosY, PosZ));
                    SceneData->MinX[Index] = PosX - DEFAULT_HALF_EXTENT;
                    SceneData->MinY[Index] = PosY - DEFAULT_HALF_EXTENT;
                    SceneData->MinZ[Index] = PosZ - DEFAULT_HALF_EXTENT;
                    SceneData->MaxX[Index] = PosX + DEFAULT_HALF_EXTENT;
                    SceneData->MaxY[Index] = PosY + DEFAULT_HALF_EXTENT;
                    SceneData->MaxZ[Index] = PosZ + DEFAULT_HALF_EXTENT;
                    
                    SceneData->MeshIDs[Index] = InRequest.MeshID;
                    SceneData->MaterialIDs[Index] = InRequest.MaterialID;
                    SceneData->IsVisible[Index] = true;

                    SceneStatistics.TotalObjectCount++;
                }
            }
        }

        if (Grid)
        {
            for (uint32_t i = 0; i < SceneStatistics.TotalObjectCount; ++i)
            {
                Grid->InsertObject(i);
            }
        }
    }

    bool USceneManager::EnsureObjectCount(uint32_t InObjectCount)
    {
        if (InObjectCount > FSceneDataSOA::MAX_OBJECTS) return false;
        if (!SceneData) Initialize();
        SceneStatistics.TotalObjectCount = InObjectCount;
        return true;
    }

    bool USceneManager::AddObject(const Math::FBox& InBounds, const Math::FMatrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        Math::FPacked3x4Matrix Packed;
        Packed.Store(InWorldMatrix);
        return AddObjectPacked(InBounds, Packed, InMeshID, InMaterialID);
    }

    bool USceneManager::AddObjectPacked(const Math::FBox& InBounds, const Math::FPacked3x4Matrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        if (!SceneData || SceneStatistics.TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) return false;

        const uint32_t Index = SceneStatistics.TotalObjectCount;
        WriteBounds(*SceneData, Index, InBounds);
        SceneData->WorldMatrices[Index] = InWorldMatrix;
        SceneData->MeshIDs[Index] = InMeshID;
        SceneData->MaterialIDs[Index] = InMaterialID;
        SceneData->IsVisible[Index] = true;

        SceneStatistics.TotalObjectCount++;
        if (Grid) Grid->InsertObject(Index);

        return true;
    }

    bool USceneManager::SaveSceneBinary(const std::wstring& InFilePath) const
    {
        if (!SceneData || InFilePath.empty()) return false;
        std::ofstream File(InFilePath, std::ios::binary);
        if (!File) return false;

        const uint32_t Count = SceneStatistics.TotalObjectCount;
        File.write(reinterpret_cast<const char*>(&Count), sizeof(Count));
        File.write(reinterpret_cast<const char*>(SceneData->MinX.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MinY.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MinZ.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxX.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxY.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxZ.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->WorldMatrices.data()), sizeof(Math::FPacked3x4Matrix) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MeshIDs.data()), sizeof(uint32_t) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaterialIDs.data()), sizeof(uint32_t) * Count);
        return File.good();
    }

    bool USceneManager::LoadSceneBinary(const std::wstring& InFilePath)
    {
        if (!SceneData || InFilePath.empty()) return false;
        std::ifstream File(InFilePath, std::ios::binary);
        if (!File) return false;

        uint32_t Count = 0;
        File.read(reinterpret_cast<char*>(&Count), sizeof(Count));
        if (Count > FSceneDataSOA::MAX_OBJECTS) return false;

        ResetScene();
        File.read(reinterpret_cast<char*>(SceneData->MinX.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MinY.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MinZ.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxX.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxY.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxZ.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->WorldMatrices.data()), sizeof(Math::FPacked3x4Matrix) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MeshIDs.data()), sizeof(uint32_t) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaterialIDs.data()), sizeof(uint32_t) * Count);

        SceneStatistics.TotalObjectCount = Count;
        return File.good();
    }

    bool USceneManager::SelectObject(uint32_t InObjectIndex)
    {
        if (!IsValidIndex(InObjectIndex)) return false;
        SelectionData.bHasSelection = true;
        SelectionData.ObjectIndex = InObjectIndex;
        SelectionData.MeshID = SceneData->MeshIDs[InObjectIndex];
        SelectionData.MaterialID = SceneData->MaterialIDs[InObjectIndex];
        return true;
    }

    void USceneManager::ClearSelection() { ResetSelectionState(); }
    void USceneManager::ResetSelectionState() { SelectionData = {}; }
}