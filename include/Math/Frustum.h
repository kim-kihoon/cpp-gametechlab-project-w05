#pragma once
#include <Math/MathTypes.h>

namespace Math
{
    enum class ECullingResult
    {
        Outside = 0,
        Intersecting = 1,
        FullyInside = 2
    };

    struct FFrustum
    {
        alignas(16) FVector4 Planes[6];

        void BuildFromMatrix(const FMatrix& ViewProj)
        {
            // 행렬에서 평면 추출 (구현 생략)
        }

        /** [성능 극대화] 구조체 생성 없이 레지스터 데이터를 직접 받아 연산 */
        inline ECullingResult TestBox(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) const
        {
            bool bFullyInside = true;
            
            // SIMD 레지스터에 직접 로드
            XMVECTOR BoxMin = XMVectorSet(MinX, MinY, MinZ, 0.0f);
            XMVECTOR BoxMax = XMVectorSet(MaxX, MaxY, MaxZ, 0.0f);

            for (int i = 0; i < 6; ++i)
            {
                FVector Plane = Planes[i];
                XMVECTOR Mask = XMVectorGreater(Plane, XMVectorZero());
                XMVECTOR P = XMVectorSelect(BoxMin, BoxMax, Mask);
                XMVECTOR N = XMVectorSelect(BoxMax, BoxMin, Mask);

                if (XMVectorGetX(XMPlaneDotCoord(Plane, N)) > 0.0f)
                {
                    return ECullingResult::Outside;
                }

                if (XMVectorGetX(XMPlaneDotCoord(Plane, P)) > 0.0f)
                {
                    bFullyInside = false;
                }
            }

            return bFullyInside ? ECullingResult::FullyInside : ECullingResult::Intersecting;
        }

        /** 기존 인터페이스 유지 (하위 호환성) */
        inline ECullingResult TestBox(const FBox& Box) const
        {
            return TestBox(Box.Min.x, Box.Min.y, Box.Min.z, Box.Max.x, Box.Max.y, Box.Max.z);
        }
    };
}