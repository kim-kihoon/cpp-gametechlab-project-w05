#pragma once
#include <Math/MathTypes.h>
#include <cstdint>
#include <DirectXMath.h>

namespace Scene
{
    using namespace Math;

    enum class ELODLevel : uint8_t
    {
        LOD0 = 0,
        LOD1,
        LOD2,
        Billboard
    };

    static constexpr uint32_t BASE_MESH_TYPE_COUNT = 2;
    static constexpr uint32_t MESH_LOD_RESOURCE_COUNT = 3;
    static constexpr uint32_t LOD1_MESH_ID_OFFSET = BASE_MESH_TYPE_COUNT;
    static constexpr uint32_t LOD2_MESH_ID_OFFSET = BASE_MESH_TYPE_COUNT * 2;
    static constexpr uint32_t TOTAL_MESH_RESOURCE_COUNT = BASE_MESH_TYPE_COUNT * MESH_LOD_RESOURCE_COUNT;
    static constexpr uint32_t BILLBOARD_MESH_ID_OFFSET = 10;

    constexpr uint32_t EncodeRenderMeshID(uint32_t BaseMeshID, ELODLevel LODLevel)
    {
        switch (LODLevel)
        {
        case ELODLevel::LOD0: return BaseMeshID;
        case ELODLevel::LOD1: return LOD1_MESH_ID_OFFSET + BaseMeshID;
        case ELODLevel::LOD2: return LOD2_MESH_ID_OFFSET + BaseMeshID;
        case ELODLevel::Billboard: return BILLBOARD_MESH_ID_OFFSET + BaseMeshID;
        }

        return BaseMeshID;
    }

    struct FLODSelectionContext
    {
        DirectX::XMFLOAT3 CameraPosition = { 0.0f, 0.0f, 0.0f };
        float ViewportHeight = 1080.0f;
        float TanHalfFovY = 0.57735026f;
        float LOD0ProjectedSizePx = 40.0f;
        float LOD1ProjectedSizePx = 24.0f;
        float BillboardProjectedSizePx = 18.0f;
        float BillboardMinDistance = 45.0f;
        float HysteresisRatio = 0.15f;
        float TransitionJitterRatio = 0.30f;
    };

    /**
     * 단일 Static Mesh 생성 요청 데이터.
     */
    struct FSceneSpawnRequest
    {
        FMatrix WorldMatrix = DirectX::XMMatrixIdentity();
        uint32_t MeshID = 0;
        uint32_t MaterialID = 0;
    };

    /**
     * 규칙적인 3차원 배치를 생성하기 위한 요청 데이터.
     */
    struct FSceneGridSpawnRequest
    {
        uint32_t Width = 50;
        uint32_t Height = 50;
        uint32_t Depth = 20;
        float Spacing = 100.0f;
        uint32_t MeshID = 0;
        uint32_t MaterialID = 0;
    };

    /**
     * 현재 선택된 오브젝트 정보를 저장하는 구조체.
     */
    struct FSceneSelectionData
    {
        bool bHasSelection = false;
        uint32_t ObjectIndex = 0;
        uint32_t MeshID = 0;
        uint32_t MaterialID = 0;
    };

    /**
     * 씬 상태 요약 정보를 저장하는 구조체.
	 */
	struct FSceneStatistics
	{
		uint32_t TotalObjectCount = 0;
		uint32_t VisibleObjectCount = 0;
	};
}
