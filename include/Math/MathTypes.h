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
}