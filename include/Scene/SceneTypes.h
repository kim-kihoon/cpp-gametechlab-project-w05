#pragma once
#include <Math/MathTypes.h>
#include <cstdint>
#include <DirectXMath.h>

#include <vector>
#include <functional>

namespace Math { struct FFrustum; }

namespace Scene
{
    using namespace Math;

    typedef std::function<bool(uint32_t, float&)> NarrowPhaseFunc;

    struct FSceneBVHNode
    {
        Math::FBox Bounds;
        uint32_t LeftChild = 0;
        uint32_t RightChild = 0;
        uint32_t ObjectIndex = 0;
        uint32_t ObjectCount = 0;

        bool IsLeaf() const { return ObjectCount > 0; }
    };

    struct FSceneBVH
    {
        std::vector<FSceneBVHNode> Nodes;
        std::vector<uint32_t> ObjectIndices;

        void Build(const struct FSceneDataSOA& SceneData);
        void QueryFrustum(const Math::FFrustum& Frustum, uint32_t* OutObjectIndices, uint32_t& OutCount, uint32_t MaxCount) const;
        bool Raycast(const Math::FRay& Ray, float MaxDistance, uint32_t& OutHitIndex, float& OutHitDistance, NarrowPhaseFunc NarrowPhaseTest) const;
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
