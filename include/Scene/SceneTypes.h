#pragma once
#include <Math/MathTypes.h>
#include <cstdint>
#include <DirectXMath.h>

namespace Scene
{
    using namespace Math;

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
