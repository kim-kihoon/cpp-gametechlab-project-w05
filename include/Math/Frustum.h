#pragma once
#include <Math/MathTypes.h>

namespace Math
{
    /**
     * 컬링 결과 열거형.
     */
    enum class ECullingResult
    {
        Outside = 0,
        Intersecting = 1,
        FullyInside = 2
    };

    /**
     * Frustum Culling을 위한 평면 집합 구조체.
     */
    struct FFrustum
    {
        alignas(16) FVector4 Planes[6];

        /** 카메라 행렬로부터 프러스텀 평면 구축 */
        void BuildFromMatrix(const FMatrix& ViewProj);

        /** 박스와 프러스텀 간의 교차 여부 테스트 */
        inline ECullingResult TestBox(const FBox& Box) const
        {
            bool bFullyInside = true;

            for (int i = 0; i < 6; ++i)
            {
                FVector Plane = Planes[i];
                FVector PlaneNormal = XMVectorSetW(Plane, 0.0f);
                FVector PlaneNormalAbs = XMVectorAbs(PlaneNormal);
                
                FVector R = XMVector3Dot(Box.Extents, PlaneNormalAbs);
                FVector S = XMVector3Dot(Box.Center, Plane);
                
                if (XMVector4Less(XMVectorAdd(S, R), XMVectorZero()))
                {
                    return ECullingResult::Outside;
                }
                if (XMVector4Less(XMVectorSubtract(S, R), XMVectorZero()))
                {
                    bFullyInside = false;
                }
            }

            return bFullyInside ? ECullingResult::FullyInside : ECullingResult::Intersecting;
        }
    };
}