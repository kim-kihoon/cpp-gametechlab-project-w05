#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Scene
{
    namespace
    {
        // 사과의 기본 반경 (필요 시 메시 데이터로부터 자동 계산하도록 확장 가능)
        constexpr float DEFAULT_HALF_EXTENT = 0.5f; 
    }

    USceneManager::USceneManager() {}
    USceneManager::~USceneManager() {}

    void USceneManager::Initialize()
    {
        SceneData = std::make_unique<FSceneDataSOA>();

        // 실제 씬 bounds를 읽은 뒤 BuildGrid()가 차원과 셀 크기를 다시 결정한다.
        Grid = std::make_unique<UUniformGrid>(1, 1, 1, 4.0f, SceneData.get());
        
        ResetScene();
    }

    void USceneManager::Update(float DeltaTime)
    {
        // [Hot Path] 5만 개 가시성 카운트는 Culling 엔진이 직접 SceneStatistics.VisibleObjectCount를 업데이트하게 함.
        // 여기서는 매 프레임 루프를 도는 행위를 금지함.
    }

    void USceneManager::BuildSceneBVH()
    {
        if (SceneData) SceneBVH.Build(*SceneData);
    }

    void USceneManager::ResetScene()
    {
		if (!SceneData) return;

		SceneData->ResetRenderQueue();
		SceneData->IsVisible.fill(false);
		ResetSelectionState();
        if (Grid) Grid->BuildGrid();
        // BuildSceneBVH();
    }

    bool USceneManager::SpawnStaticMesh(const FSceneSpawnRequest& InRequest, bool bRebuildGrid)
    {
        if (!SceneData || SceneData->TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) return false;

        const uint32_t ObjectIndex = SceneData->TotalObjectCount;
        
        // 1. 행렬 압축 저장 (3x4)
        SceneData->WorldMatrices[ObjectIndex].Store(InRequest.WorldMatrix);

        // 2. AABB 계산 및 SOA 분할 저장
        const DirectX::XMVECTOR Translation = InRequest.WorldMatrix.r[3];
        const float CX = DirectX::XMVectorGetX(Translation);
        const float CY = DirectX::XMVectorGetY(Translation);
        const float CZ = DirectX::XMVectorGetZ(Translation);

        SceneData->MinX[ObjectIndex] = CX - DEFAULT_HALF_EXTENT;
        SceneData->MinY[ObjectIndex] = CY - DEFAULT_HALF_EXTENT;
        SceneData->MinZ[ObjectIndex] = CZ - DEFAULT_HALF_EXTENT;
        SceneData->MaxX[ObjectIndex] = CX + DEFAULT_HALF_EXTENT;
        SceneData->MaxY[ObjectIndex] = CY + DEFAULT_HALF_EXTENT;
        SceneData->MaxZ[ObjectIndex] = CZ + DEFAULT_HALF_EXTENT;

        // 3. 기타 메타데이터
        SceneData->MeshIDs[ObjectIndex] = InRequest.MeshID;
        SceneData->MaterialIDs[ObjectIndex] = InRequest.MaterialID;
        SceneData->IsVisible[ObjectIndex] = true;

        SceneData->TotalObjectCount++;

        // 4. 그리드 삽입
		if (bRebuildGrid) 
        { 
            if (Grid) Grid->BuildGrid(); 
            // BuildSceneBVH();       
        }

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
                    
                    float PosX = static_cast<float>(X) * InRequest.Spacing;
                    float PosY = static_cast<float>(Y) * InRequest.Spacing;
                    float PosZ = static_cast<float>(Z) * InRequest.Spacing;
                    Req.WorldMatrix = DirectX::XMMatrixTranslation(PosX, PosY, PosZ);

                    if (!SpawnStaticMesh(Req, false)) return;
                }
            }
        }
        if (Grid) Grid->BuildGrid(); 
        // BuildSceneBVH();    
    }

    bool USceneManager::EnsureObjectCount(uint32_t InObjectCount)
    {
		if (!SceneData || InObjectCount > FSceneDataSOA::MAX_OBJECTS)
		{
			return false;
		}

		// 현재 Scene의 총 객체 수를 갱신합니다.
		// (미리 MAX_OBJECTS 만큼 할당된 SOA 구조를 사용하므로 카운트만 갱신)
		SceneData->TotalObjectCount = InObjectCount;

		return true;
    }

    bool USceneManager::SaveSceneBinary(const std::wstring& InFilePath) const
    {
        if (!SceneData || InFilePath.empty()) return false;

        std::ofstream File(InFilePath, std::ios::binary);
        if (!File) return false;

        const uint32_t Count = SceneData->TotalObjectCount;
        File.write(reinterpret_cast<const char*>(&Count), sizeof(Count));

        // [성능] 개별 write 대신 큰 블록 단위로 기록하여 I/O 지연 단축
        File.write(reinterpret_cast<const char*>(SceneData->MinX.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MinY.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MinZ.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxX.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxY.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->MaxZ.data()), sizeof(float) * Count);
        File.write(reinterpret_cast<const char*>(SceneData->WorldMatrices.data()), sizeof(FPacked3x4Matrix) * Count);
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

        // [성능] 메모리 블록 단위 고속 로드
        File.read(reinterpret_cast<char*>(SceneData->MinX.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MinY.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MinZ.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxX.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxY.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaxZ.data()), sizeof(float) * Count);
        File.read(reinterpret_cast<char*>(SceneData->WorldMatrices.data()), sizeof(FPacked3x4Matrix) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MeshIDs.data()), sizeof(uint32_t) * Count);
        File.read(reinterpret_cast<char*>(SceneData->MaterialIDs.data()), sizeof(uint32_t) * Count);

        SceneData->TotalObjectCount = Count;
        if (Grid) Grid->BuildGrid();
        // BuildSceneBVH();
        return File.good();
    }

    bool USceneManager::AddObject(const Math::FBox& InBounds, const Math::FMatrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        if (!SceneData || SceneData->TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) return false;

        const uint32_t ObjectIndex = SceneData->TotalObjectCount;
        SceneData->MinX[ObjectIndex] = InBounds.Min.x;
        SceneData->MinY[ObjectIndex] = InBounds.Min.y;
        SceneData->MinZ[ObjectIndex] = InBounds.Min.z;
        SceneData->MaxX[ObjectIndex] = InBounds.Max.x;
        SceneData->MaxY[ObjectIndex] = InBounds.Max.y;
        SceneData->MaxZ[ObjectIndex] = InBounds.Max.z;

        SceneData->WorldMatrices[ObjectIndex].Store(InWorldMatrix);
        SceneData->MeshIDs[ObjectIndex] = InMeshID;
        SceneData->MaterialIDs[ObjectIndex] = InMaterialID;
        SceneData->IsVisible[ObjectIndex] = true;

        SceneData->TotalObjectCount++;
        return true;
    }

    bool USceneManager::AddObjectPacked(const Math::FBox& InBounds, const Math::FPacked3x4Matrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        if (!SceneData || SceneData->TotalObjectCount >= FSceneDataSOA::MAX_OBJECTS) return false;

        const uint32_t ObjectIndex = SceneData->TotalObjectCount;
        SceneData->MinX[ObjectIndex] = InBounds.Min.x;
        SceneData->MinY[ObjectIndex] = InBounds.Min.y;
        SceneData->MinZ[ObjectIndex] = InBounds.Min.z;
        SceneData->MaxX[ObjectIndex] = InBounds.Max.x;
        SceneData->MaxY[ObjectIndex] = InBounds.Max.y;
        SceneData->MaxZ[ObjectIndex] = InBounds.Max.z;

        SceneData->WorldMatrices[ObjectIndex] = InWorldMatrix;
        SceneData->MeshIDs[ObjectIndex] = InMeshID;
        SceneData->MaterialIDs[ObjectIndex] = InMaterialID;
        SceneData->IsVisible[ObjectIndex] = true;

        SceneData->TotalObjectCount++;
        return true;
    }

    bool USceneManager::SelectObject(uint32_t InObjectIndex)
    {
        if (!SceneData || InObjectIndex >= SceneData->TotalObjectCount) return false;

        SelectionData.bHasSelection = true;
        SelectionData.ObjectIndex = InObjectIndex;
        SelectionData.MeshID = SceneData->MeshIDs[InObjectIndex];
        SelectionData.MaterialID = SceneData->MaterialIDs[InObjectIndex];
        return true;
    }

    void USceneManager::ClearSelection() { ResetSelectionState(); }

    FSceneStatistics USceneManager::GetSceneStatistics() const
    {
        FSceneStatistics Stats;
        if (SceneData)
        {
            Stats.TotalObjectCount = SceneData->TotalObjectCount;
            Stats.VisibleObjectCount = SceneData->RenderCount;
        }
        return Stats;
    }

    void USceneManager::ResetSelectionState() { SelectionData = {}; }
}
