#include <Scene/UniformGrid.h>
#include <algorithm>

namespace Scene
{
    UUniformGrid::UUniformGrid(int InW, int InH, int InD, float InCellSize, FSceneDataSOA* InSceneData)
        : Width(InW), Height(InH), Depth(InD), CellSize(InCellSize), SceneData(InSceneData)
        , OriginX(-(static_cast<float>(InW) * InCellSize) * 0.5f)
        , OriginY(-(static_cast<float>(InH) * InCellSize) * 0.5f)
        , OriginZ(0.0f)
        , TotalEntryCount(0)
    {
        InvCellSize = 1.0f / CellSize;  // 최적화 나눗셈 캐싱
        Cells.resize(Width * Height * Depth);
        ClearGrid();
    }

    void UUniformGrid::ClearGrid()
    {
        TotalEntryCount = 0;
        for (auto& Cell : Cells)
        {
            Cell.Count = 0;
			Cell.StartIndex = 0;
		}
        
        // 각 Cell의 AABB 초기화 (좌표계 기준)
        for (int z = 0; z < Depth; ++z)
        {
            for (int y = 0; y < Height; ++y)
            {
                for (int x = 0; x < Width; ++x)
                {
                    int Index = x + (y * Width) + (z * Width * Height);
                    Cells[Index].CellBox.Min = { OriginX + (x * CellSize), OriginY + (y * CellSize), OriginZ + (z * CellSize) };
                    Cells[Index].CellBox.Max = { OriginX + ((x + 1) * CellSize), OriginY + ((y + 1) * CellSize), OriginZ + ((z + 1) * CellSize) };
                }
            }
        }
    }

    // 물체가 이동될때마다 이 함수를 호출해줘야 함.
    void UUniformGrid::BuildGrid()
    {
        if (!SceneData) return;

        // 셀 초기화
        for (auto& Cell : Cells) { Cell.Count = 0; Cell.StartIndex = 0; }

        // 각 셀에 들어갈 객체 수 계산
        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ObjectIndex++)
        {
			int MinGridX = std::clamp(static_cast<int>((SceneData->MinX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
			int MinGridY = std::clamp(static_cast<int>((SceneData->MinY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
			int MinGridZ = std::clamp(static_cast<int>((SceneData->MinZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

			int MaxGridX = std::clamp(static_cast<int>((SceneData->MaxX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
			int MaxGridY = std::clamp(static_cast<int>((SceneData->MaxY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
			int MaxGridZ = std::clamp(static_cast<int>((SceneData->MaxZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

            for (int z = MinGridZ; z <= MaxGridZ; z++)
            {
                for (int y = MinGridY; y <= MaxGridY; y++)
                {
                    for (int x = MinGridX; x <= MaxGridX; x++)
                    {
                        int CellIndex = x + (y * Width) + (z * Width * Height);
                        Cells[CellIndex].Count++;
                    }
                }
            }
        }

        TotalEntryCount = 0;
        for (auto& Cell : Cells)
        {
            Cell.StartIndex = TotalEntryCount;
            TotalEntryCount += Cell.Count;

            Cell.Count = 0; // 실제 삽입 시 인덱스 오프셋으로 쓰기 위함.
        }

        // 버퍼 오버플로우 방지 -> 추후 변경 해야함. 추가적인 객체가 안생긴다는 보장이 없음.
        if (TotalEntryCount >= MAX_GRID_ENTRIES) return;

        for (uint32_t ObjectIndex = 0; ObjectIndex < SceneData->TotalObjectCount; ObjectIndex++)
        {
			int MinGridX = std::clamp(static_cast<int>((SceneData->MinX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
			int MinGridY = std::clamp(static_cast<int>((SceneData->MinY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
			int MinGridZ = std::clamp(static_cast<int>((SceneData->MinZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

			int MaxGridX = std::clamp(static_cast<int>((SceneData->MaxX[ObjectIndex] - OriginX) * InvCellSize), 0, Width - 1);
			int MaxGridY = std::clamp(static_cast<int>((SceneData->MaxY[ObjectIndex] - OriginY) * InvCellSize), 0, Height - 1);
			int MaxGridZ = std::clamp(static_cast<int>((SceneData->MaxZ[ObjectIndex] - OriginZ) * InvCellSize), 0, Depth - 1);

			for (int z = MinGridZ; z <= MaxGridZ; z++)
			{
				for (int y = MinGridY; y <= MaxGridY; y++)
				{
					for (int x = MinGridX; x <= MaxGridX; x++)
					{
                        int CellIndex = x + (y * Width) + (z * Width * Height);
                        FGridCell& Cell = Cells[CellIndex];

                        GlobalIndexBuffer[Cell.StartIndex + Cell.Count] = ObjectIndex;
                        Cell.Count++;
					}
				}
			}
        }
    }

	/*void UUniformGrid::CullingAndBuildRenderQueue_ExactSort(const Math::FFrustum& Frustum,
		const Math::FVector& CameraPosVec)
	{
		if (!SceneData) return;

		SceneData->ResetRenderQueue();
		SceneData->IsVisible.fill(false);

		// DirectXMath 벡터를 float3로 변환
		DirectX::XMFLOAT3 CameraPos;
		DirectX::XMStoreFloat3(&CameraPos, CameraPosVec);

		// [오프셋 적용] 월드 좌표 -> 그리드 인덱스
		const int MinGridX = std::clamp(static_cast<int>((Frustum.AABBMin.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MinGridY = std::clamp(static_cast<int>((Frustum.AABBMin.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MinGridZ = std::clamp(static_cast<int>((Frustum.AABBMin.z - OriginZ) * InvCellSize), 0, Depth - 1);

		const int MaxGridX = std::clamp(static_cast<int>((Frustum.AABBMax.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MaxGridY = std::clamp(static_cast<int>((Frustum.AABBMax.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MaxGridZ = std::clamp(static_cast<int>((Frustum.AABBMax.z - OriginZ) * InvCellSize), 0, Depth - 1);

		for (int z = MinGridZ; z <= MaxGridZ; z++)
		{
			for (int y = MinGridY; y <= MaxGridY; y++)
			{
				for (int x = MinGridX; x <= MaxGridX; x++)
				{
					const FGridCell& Cell = Cells[x + (y * Width) + (z * Width * Height)];
					if (Cell.Count == 0) continue;

					Math::ECullingResult CellResult = Frustum.TestBox(Cell.CellBox);
					if (CellResult == Math::ECullingResult::Outside) continue;

					const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

					if (CellResult == Math::ECullingResult::FullyInside)
					{
						for (uint32_t i = 0; i < Cell.Count; i++)
						{
							const uint32_t Index = Indices[i];
							if (!SceneData->IsVisible[Index])
							{
								// [캐싱] 중심점 계산 및 카메라와의 거리(제곱) 저장
								float CX = (SceneData->MinX[Index] + SceneData->MaxX[Index]) * 0.5f;
								float CY = (SceneData->MinY[Index] + SceneData->MaxY[Index]) * 0.5f;
								float CZ = (SceneData->MinZ[Index] + SceneData->MaxZ[Index]) * 0.5f;

								SceneData->DistanceToCameraSq[Index] =
									(CX - CameraPos.x) * (CX - CameraPos.x) +
									(CY - CameraPos.y) * (CY - CameraPos.y) +
									(CZ - CameraPos.z) * (CZ - CameraPos.z);

								SceneData->AddToRenderQueue(Index);
								SceneData->IsVisible[Index] = true;
							}
						}
					}
					else
					{
						for (uint32_t i = 0; i < Cell.Count; i++)
						{
							const uint32_t Index = Indices[i];
							if (SceneData->IsVisible[Index]) continue;

							if (Frustum.TestBox(SceneData->MinX[Index], SceneData->MinY[Index], SceneData->MinZ[Index],
								SceneData->MaxX[Index], SceneData->MaxY[Index], SceneData->MaxZ[Index])
								!= Math::ECullingResult::Outside)
							{
								float CX = (SceneData->MinX[Index] + SceneData->MaxX[Index]) * 0.5f;
								float CY = (SceneData->MinY[Index] + SceneData->MaxY[Index]) * 0.5f;
								float CZ = (SceneData->MinZ[Index] + SceneData->MaxZ[Index]) * 0.5f;

								SceneData->DistanceToCameraSq[Index] =
									(CX - CameraPos.x) * (CX - CameraPos.x) +
									(CY - CameraPos.y) * (CY - CameraPos.y) +
									(CZ - CameraPos.z) * (CZ - CameraPos.z);

								SceneData->AddToRenderQueue(Index);
								SceneData->IsVisible[Index] = true;
							}
						}
					}
				}
			}
		}

		// [정렬] 순회 완료 후 캐싱된 거리를 기준으로 큐를 한 번만 정렬
		if (SceneData->RenderCount > 0)
		{
			std::sort(SceneData->RenderQueue.data(),
				SceneData->RenderQueue.data() + SceneData->RenderCount,
				[&](uint32_t A, uint32_t B) {
					return SceneData->DistanceToCameraSq[A] < SceneData->DistanceToCameraSq[B];
				});
		}
	}*/

	void UUniformGrid::CullingAndBuildRenderQueue_GridSort(const Math::FFrustum& Frustum,
		const Math::FVector& CameraPosVec)
	{
		if (!SceneData) return;

		SceneData->ResetRenderQueue();
		SceneData->IsVisible.fill(false);

		DirectX::XMFLOAT3 CameraPos;
		DirectX::XMStoreFloat3(&CameraPos, CameraPosVec);

		const int MinGridX = std::clamp(static_cast<int>((Frustum.AABBMin.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MinGridY = std::clamp(static_cast<int>((Frustum.AABBMin.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MinGridZ = std::clamp(static_cast<int>((Frustum.AABBMin.z - OriginZ) * InvCellSize), 0, Depth - 1);

		const int MaxGridX = std::clamp(static_cast<int>((Frustum.AABBMax.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MaxGridY = std::clamp(static_cast<int>((Frustum.AABBMax.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MaxGridZ = std::clamp(static_cast<int>((Frustum.AABBMax.z - OriginZ) * InvCellSize), 0, Depth - 1);

		// [마법의 렌더 방향 계산] 
		// Frustum 바운딩 박스의 중심을 구한 뒤, 카메라가 그 중심보다 어느 쪽에 있는지 판단합니다.
        float CenterX = (Frustum.AABBMin.x + Frustum.AABBMax.x) * 0.5f;
        float CenterY = (Frustum.AABBMin.y + Frustum.AABBMax.y) * 0.5f;
        float CenterZ = (Frustum.AABBMin.z + Frustum.AABBMax.z) * 0.5f;

        int StepX = (CameraPos.x > CenterX) ? -1 : 1;
        int StartX = (StepX == 1) ? MinGridX : MaxGridX;
        int EndX = (StepX == 1) ? MaxGridX : MinGridX;

        int StepY = (CameraPos.y > CenterY) ? -1 : 1;
        int StartY = (StepY == 1) ? MinGridY : MaxGridY;
        int EndY = (StepY == 1) ? MaxGridY : MinGridY;

        int StepZ = (CameraPos.z > CenterZ) ? -1 : 1;
        int StartZ = (StepZ == 1) ? MinGridZ : MaxGridZ;
        int EndZ = (StepZ == 1) ? MaxGridZ : MinGridZ;

		// 루프가 Start에서 시작하여 End까지 도달하도록 (z != EndZ + StepZ) 형태로 돕니다.
		for (int z = StartZ; z != EndZ + StepZ; z += StepZ)
		{
			for (int y = StartY; y != EndY + StepY; y += StepY)
			{
				for (int x = StartX; x != EndX + StepX; x += StepX)
				{
					const FGridCell& Cell = Cells[x + (y * Width) + (z * Width * Height)];
					if (Cell.Count == 0) continue;

					Math::ECullingResult CellResult = Frustum.TestBox(Cell.CellBox);
					if (CellResult == Math::ECullingResult::Outside) continue;

					const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

					if (CellResult == Math::ECullingResult::FullyInside)
					{
						for (uint32_t i = 0; i < Cell.Count; i++)
						{
							const uint32_t Index = Indices[i];
							if (!SceneData->IsVisible[Index])
							{
								SceneData->AddToRenderQueue(Index);
								SceneData->IsVisible[Index] = true;
							}
						}
					}
					else
					{
						for (uint32_t i = 0; i < Cell.Count; i++)
						{
							const uint32_t Index = Indices[i];
							if (SceneData->IsVisible[Index]) continue;

							if (Frustum.TestBox(SceneData->MinX[Index], SceneData->MinY[Index], SceneData->MinZ[Index],
								SceneData->MaxX[Index], SceneData->MaxY[Index], SceneData->MaxZ[Index])
								!= Math::ECullingResult::Outside)
							{
								SceneData->AddToRenderQueue(Index);
								SceneData->IsVisible[Index] = true;
							}
						}
					}
				}
			}
		}
	}

    /*void UUniformGrid::InsertObject(uint32_t ObjectIndex)
    {
        if (!SceneData || TotalEntryCount >= MAX_GRID_ENTRIES) return;

        // [성능] 힙 메모리 접근 최소화 (로컬 캐싱)
        const float OriginalXMin = SceneData->MinX[ObjectIndex];
        const float OriginalYMin = SceneData->MinY[ObjectIndex];
        const float OriginalZMin = SceneData->MinZ[ObjectIndex];
        const float OriginalXMax = SceneData->MaxX[ObjectIndex];
        const float OriginalYMax = SceneData->MaxY[ObjectIndex];
        const float OriginalZMax = SceneData->MaxZ[ObjectIndex];

        float CenterX = (OriginalXMin + OriginalXMax) * 0.5f;
        float CenterY = (OriginalYMin + OriginalYMax) * 0.5f;
        float CenterZ = (OriginalZMin + OriginalZMax) * 0.5f;

        int GX = std::clamp(static_cast<int>(CX / CellSize), 0, Width - 1);
        int GY = std::clamp(static_cast<int>(CY / CellSize), 0, Height - 1);
        int GZ = std::clamp(static_cast<int>(CZ / CellSize), 0, Depth - 1);

        int CellIndex = GX + (GY * Width) + (GZ * Width * Height);
        FGridCell& Cell = Cells[CellIndex];

        if (Cell.Count == 0)
        {
            Cell.StartIndex = TotalEntryCount;
        }
        
        GlobalIndexBuffer[TotalEntryCount++] = ObjectIndex;
        Cell.Count++;
    }*/

    void UUniformGrid::CullingAndBuildRenderQueue(const Math::FFrustum& Frustum)
    {
        if (!SceneData) return;
        
        SceneData->ResetRenderQueue();
        SceneData->IsVisible.fill(false);

		const int MinGridX = std::clamp(static_cast<int>((Frustum.AABBMin.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MinGridY = std::clamp(static_cast<int>((Frustum.AABBMin.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MinGridZ = std::clamp(static_cast<int>((Frustum.AABBMin.z - OriginZ) * InvCellSize), 0, Depth - 1);

		const int MaxGridX = std::clamp(static_cast<int>((Frustum.AABBMax.x - OriginX) * InvCellSize), 0, Width - 1);
		const int MaxGridY = std::clamp(static_cast<int>((Frustum.AABBMax.y - OriginY) * InvCellSize), 0, Height - 1);
		const int MaxGridZ = std::clamp(static_cast<int>((Frustum.AABBMax.z - OriginZ) * InvCellSize), 0, Depth - 1);

        for (int z = MinGridZ; z <= MaxGridZ; z++)
        {
	        for (int y = MinGridY; y <= MaxGridY; y++)
	        {
		        for (int x = MinGridX; x <= MaxGridX; x++)
		        {
                    const FGridCell& Cell = Cells[x + (y * Width) + (z * Width * Height)];

                    if (Cell.Count == 0) continue;

                    Math::ECullingResult CellResult = Frustum.TestBox(Cell.CellBox);

                    if (CellResult == Math::ECullingResult::Outside) continue;

                    const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

                    if (CellResult == Math::ECullingResult::FullyInside)
                    {
	                    for (uint32_t i = 0; i < Cell.Count; i++)
	                    {
                            const uint32_t Index = Indices[i];
                            if (!SceneData->IsVisible[Index])
                            {
                                SceneData->AddToRenderQueue(Index);
                                SceneData->IsVisible[Index] = true;
                            }
	                    }
                    }
                    else
                    {
	                    for (uint32_t i = 0; i < Cell.Count; i++)
	                    {
                            const uint32_t Index = Indices[i];
                            if (SceneData->IsVisible[Index]) continue;

                            if (Frustum.TestBox(SceneData->MinX[Index],SceneData->MinY[Index], SceneData->MinZ[Index],
                                SceneData->MaxX[Index], SceneData->MaxY[Index], SceneData->MaxZ[Index])
                                != Math::ECullingResult::Outside)
                            {
                                SceneData->AddToRenderQueue(Index);
                                SceneData->IsVisible[Index] = true;
                            }
	                    }
                    }
		        }
	        }
        }
    }

    bool UUniformGrid::Raycast(const Math::FRay& Ray, float MaxDistance, uint32_t& OutHitIndex, float& OutHitDistance)
    {
        if (!SceneData) return false;
        
		int GridX = std::clamp(static_cast<int>((Ray.Origin.x - OriginX) * InvCellSize), 0, Width - 1);
		int GridY = std::clamp(static_cast<int>((Ray.Origin.y - OriginY) * InvCellSize), 0, Height - 1);
		int GridZ = std::clamp(static_cast<int>((Ray.Origin.z - OriginZ) * InvCellSize), 0, Depth - 1);

		int StepX = (Ray.Direction.x > 0.0f) ? 1 : -1;
		int StepY = (Ray.Direction.y > 0.0f) ? 1 : -1;
		int StepZ = (Ray.Direction.z > 0.0f) ? 1 : -1;

		float tDeltaX = std::abs(CellSize * Ray.InvDirection.x);
		float tDeltaY = std::abs(CellSize * Ray.InvDirection.y);
		float tDeltaZ = std::abs(CellSize * Ray.InvDirection.z);

		float tMaxX = ((OriginX + (GridX + (StepX > 0 ? 1 : 0)) * CellSize) - Ray.Origin.x) * Ray.InvDirection.x;
		float tMaxY = ((OriginY + (GridY + (StepY > 0 ? 1 : 0)) * CellSize) - Ray.Origin.y) * Ray.InvDirection.y;
		float tMaxZ = ((OriginZ + (GridZ + (StepZ > 0 ? 1 : 0)) * CellSize) - Ray.Origin.z) * Ray.InvDirection.z;

        bool bHitFound = false;
        float ClosestHitT = MaxDistance;

		while (GridX >= 0 && GridX < Width && GridY >= 0 && GridY < Height && GridZ >= 0 && GridZ < Depth)
		{
			const FGridCell& Cell = Cells[GridX + (GridY * Width) + (GridZ * Width * Height)];

			// 해당 칸에 객체가 있다면 정밀 검사 (Narrow Phase)
			if (Cell.Count > 0)
			{
				const uint32_t* Indices = &GlobalIndexBuffer[Cell.StartIndex];

				for (uint32_t i = 0; i < Cell.Count; ++i)
				{
					const uint32_t Idx = Indices[i];

					// [최적화] 분기 없는 스칼라 슬랩 메서드 (Branchless Slab Method)
					float t1 = (SceneData->MinX[Idx] - Ray.Origin.x) * Ray.InvDirection.x;
					float t2 = (SceneData->MaxX[Idx] - Ray.Origin.x) * Ray.InvDirection.x;
					float tMinX = std::min(t1, t2);
					float tMaxX_Box = std::max(t1, t2);

					t1 = (SceneData->MinY[Idx] - Ray.Origin.y) * Ray.InvDirection.y;
					t2 = (SceneData->MaxY[Idx] - Ray.Origin.y) * Ray.InvDirection.y;
					float tMinY = std::min(t1, t2);
					float tMaxY_Box = std::max(t1, t2);

					t1 = (SceneData->MinZ[Idx] - Ray.Origin.z) * Ray.InvDirection.z;
					t2 = (SceneData->MaxZ[Idx] - Ray.Origin.z) * Ray.InvDirection.z;
					float tMinZ = std::min(t1, t2);
					float tMaxZ_Box = std::max(t1, t2);

					// 레이가 박스를 관통하는 가장 늦게 진입한 시간(tNear)과 가장 일찍 빠져나온 시간(tFar)
					float tNear = std::max(std::max(tMinX, tMinY), tMinZ);
					float tFar = std::min(std::min(tMaxX_Box, tMaxY_Box), tMaxZ_Box);

					// 관통 성공 (tNear <= tFar) 했고, 총알이 박스 뒤에 있지 않으며(tFar > 0), 사거리(MaxDistance) 이내인가?
					if (tNear <= tFar && tFar > 0.0f && tNear < ClosestHitT)
					{
						// 현재까지 검사한 객체 중 가장 가깝다면 갱신
						ClosestHitT = tNear > 0.0f ? tNear : 0.0f; // 레이 시작점이 박스 안이면 거리는 0
						OutHitIndex = Idx;
						OutHitDistance = ClosestHitT;
						bHitFound = true;
					}
				}

				// DDA 알고리즘의 꽃:
				// "현재 칸(Cell)"에서 히트가 발생했다면? 
				// 3D DDA는 항상 가까운 칸부터 순차적으로 방문하므로, 현재 칸에서 찾은 놈이 우주에서 가장 가까운 놈입니다.
				// 뒤에 있는 수만 개의 셀은 검사할 필요도 없이 즉시 루프 탈출!
				if (bHitFound)
				{
					return true;
				}
			}

			// 4. 다음 칸(Cell)으로 이동 (레이가 가장 먼저 닿는 경계선을 향해 한 칸 전진)
			if (tMaxX < tMaxY)
			{
				if (tMaxX < tMaxZ) { GridX += StepX; tMaxX += tDeltaX; }
				else { GridZ += StepZ; tMaxZ += tDeltaZ; }
			}
			else
			{
				if (tMaxY < tMaxZ) { GridY += StepY; tMaxY += tDeltaY; }
				else { GridZ += StepZ; tMaxZ += tDeltaZ; }
			}
		}

		return false;
    }
}
