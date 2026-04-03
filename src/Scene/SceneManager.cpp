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

        Math::FBox MakeDefaultBounds()
        {
            return Math::FBox{ { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } };
        }

        /** [성능] SOA 배열에 AABB 데이터를 효율적으로 기록 */
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

    void USceneManager::Update(float DeltaTime)
    {
        // Culling 엔진이 RenderCount를 갱신하므로 여기선 루프를 돌지 않음
    }

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
        
        // 데이터 주입
        SceneData->WorldMatrices[Index].Store(InRequest.WorldMatrix);
        
        const DirectX::XMVECTOR Translation = InRequest.WorldMatrix.r[3];
        Math::FBox Bounds;
        float CX = DirectX::XMVectorGetX(Translation);
        float CY = DirectX::XMVectorGetY(Translation);
        float CZ = DirectX::XMVectorGetZ(Translation);
        Bounds.Min = { CX - DEFAULT_HALF_EXTENT, CY - DEFAULT_HALF_EXTENT, CZ - DEFAULT_HALF_EXTENT };
        Bounds.Max = { CX + DEFAULT_HALF_EXTENT, CY + DEFAULT_HALF_EXTENT, CZ + DEFAULT_HALF_EXTENT };
        
        WriteBounds(*SceneData, Index, Bounds);
        SceneData->MeshIDs[Index] = InRequest.MeshID;
        SceneData->MaterialIDs[Index] = InRequest.MaterialID;
        SceneData->IsVisible[Index] = true;

        SceneStatistics.TotalObjectCount++;

        if (Grid) Grid->InsertObject(Index);

        return true;
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

    void USceneManager::SpawnStaticMeshGrid(const FSceneGridSpawnRequest& InRequest)
    {
        for (uint32_t Z = 0; Z < InRequest.Depth; ++Z)
        {
            for (uint32_t Y = 0; Y < InRequest.Height; ++Y)
            {
                for (uint32_t X = 0; X < InRequest.Width; ++X)
                {
                    FSceneSpawnRequest Req;
                    Req.MeshID = InRequest.MeshID;
                    Req.MaterialID = InRequest.MaterialID;
                    Req.WorldMatrix = DirectX::XMMatrixTranslation(X * InRequest.Spacing, Y * InRequest.Spacing, Z * InRequest.Spacing);
                    if (!SpawnStaticMesh(Req)) return;
                }
            }
        }
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