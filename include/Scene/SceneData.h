#pragma once
#include <Math/MathTypes.h>
#include <array>
#include <malloc.h>

namespace Scene
{
	using namespace Math;

	/**
	 * 5만 개의 개별 메시 데이터 (SOA 방식).
	 */
	struct alignas(64) FSceneDataSOA
	{
		static constexpr uint32_t MAX_OBJECTS = 50000;

		/** 월드 트랜스폼 행렬 배열 */
		alignas(64) std::array<FMatrix, MAX_OBJECTS> WorldMatrices;

		/** 바운딩 박스 배열 (Culling 용) */
		alignas(64) std::array<FBox, MAX_OBJECTS> BoundingBoxes;

		/** Static Mesh 식별자 */
		alignas(64) std::array<uint32_t, MAX_OBJECTS> MeshIDs;

		/** 재질 식별자 */
		alignas(64) std::array<uint32_t, MAX_OBJECTS> MaterialIDs;

		/** 가시성 결과 */
		alignas(64) std::array<bool, MAX_OBJECTS> IsVisible;

		/** 이번 프레임에 그려질 인덱스 목록 */
		alignas(64) std::array<uint32_t, MAX_OBJECTS> RenderQueue;
		uint32_t RenderCount = 0;

		void* operator new(size_t size) { return _aligned_malloc(size, 64); }
		void operator delete(void* p) { _aligned_free(p); }

		FSceneDataSOA() : RenderCount(0) 
		{
			IsVisible.fill(false);
		}

		inline void ResetRenderQueue() { RenderCount = 0; }
		inline void AddToRenderQueue(uint32_t Index) 
		{
			RenderQueue[RenderCount++] = Index;
		}
	};
}