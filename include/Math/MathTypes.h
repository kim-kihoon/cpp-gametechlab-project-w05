#pragma once
#include <DirectXMath.h>
#include <cstdint>

namespace Math
{
    using namespace DirectX;

    using FVector = XMVECTOR;
    using FVector4 = XMVECTOR;
    using FMatrix = XMMATRIX;

    /**
     * [전략 반영] GPU 대역폭 25% 절감을 위한 3x4 압축 행렬.
     */
    struct alignas(16) FPacked3x4Matrix
    {
        FVector Row0;
        FVector Row1;
        FVector Row2;

        inline void Store(const FMatrix& InMatrix)
        {
            FMatrix Transposed = XMMatrixTranspose(InMatrix);
            Row0 = Transposed.r[0];
            Row1 = Transposed.r[1];
            Row2 = Transposed.r[2];
        }
    };

    /**
     * [전략 반영] SIMD Culling에 최적화된 Min/Max 방식의 AABB.
     * SoA 구조로 풀어서 관리하기 전, 데이터 전달용으로 사용.
     */
    struct FBox
    {
        XMFLOAT3 Min;
        XMFLOAT3 Max;
    };

	struct FRay
	{
		XMFLOAT3 Origin;
		XMFLOAT3 Direction;
		XMFLOAT3 InvDirection; // [최적화] 나눗셈을 곱셈으로 바꾸기 위한 역수

		FRay(const XMFLOAT3& InOrigin, const XMFLOAT3& InDirection)
		{
			Origin = InOrigin;

			// 방향 벡터 정규화(길이 1) 및 역수 캐싱
			XMVECTOR Dir = XMVector3Normalize(XMLoadFloat3(&InDirection));
			XMStoreFloat3(&Direction, Dir);

			// 0으로 나누기 방지를 위해 아주 작은 값(epsilon) 추가
			InvDirection.x = 1.0f / (Direction.x != 0.0f ? Direction.x : 0.000001f);
			InvDirection.y = 1.0f / (Direction.y != 0.0f ? Direction.y : 0.000001f);
			InvDirection.z = 1.0f / (Direction.z != 0.0f ? Direction.z : 0.000001f);
		}
	};
}
