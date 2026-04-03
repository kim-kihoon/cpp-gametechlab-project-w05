#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace Math
{
    using namespace DirectX;

    // 3D Vector (SIMD XMVECTOR 래핑)
    using FVector = XMVECTOR;
    
    // 4D Vector
    using FVector4 = XMVECTOR;

    // 4x4 Matrix (SIMD XMMATRIX 래핑)
    using FMatrix = XMMATRIX;

    /**
     * GPU 대역폭 절감을 위한 3x4 압축 행렬 구조체.
     */
    struct FPacked3x4Matrix
    {
        FVector Row0;
        FVector Row1;
        FVector Row2;

        /**
         * FMatrix로부터 데이터를 추출하여 압축 저장.
         */
        inline void Store(const FMatrix& InMatrix)
        {
            FMatrix Transposed = XMMatrixTranspose(InMatrix);
            Row0 = Transposed.r[0];
            Row1 = Transposed.r[1];
            Row2 = Transposed.r[2];
        }
    };

    /**
     * 캐시 라인 정렬이 적용된 Bounding Box (AABB).
     */
    __declspec(align(16)) struct FBox
    {
        FVector Center;
        FVector Extents;
    };
}