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
        void BuildFromMatrix(const FMatrix& ViewProj)
        {
            // 행렬에서 6개 평면 추출 로직 (구현부 필요)
        }

        /** 
         * [전략 반영] Min/Max 기반 SIMD P-N Vertex 컬링.
         * Center/Extents 방식보다 연산량이 적어 지연 시간을 단축합니다.
         */
        inline ECullingResult TestBox(const FBox& Box) const
        {
            bool bFullyInside = true;
            XMVECTOR BoxMin = XMLoadFloat3(&Box.Min);
            XMVECTOR BoxMax = XMLoadFloat3(&Box.Max);

            for (int i = 0; i < 6; ++i)
            {
                FVector Plane = Planes[i];
                
                // 평면의 법선 방향에 따라 P-Vertex와 N-Vertex 결정
                // 법선이 양수면 Max가 P, Min이 N / 음수면 반대
                XMVECTOR Mask = XMVectorGreater(Plane, XMVectorZero());
                XMVECTOR P = XMVectorSelect(BoxMin, BoxMax, Mask);
                XMVECTOR N = XMVectorSelect(BoxMax, BoxMin, Mask);

                // N-Vertex가 평면 밖에 있으면 (평면 방정식 > 0) 완전히 바깥
                // (DirectX 평면 방정식 ax + by + cz + d = 0 기준, 
                //  내부인 경우 보통 결과가 0보다 작거나 평면 정의에 따라 다름. 
                //  여기서는 표준적인 Plane Dot Product 사용)
                
                if (XMVectorGetX(XMPlaneDotCoord(Plane, N)) > 0.0f)
                {
                    return ECullingResult::Outside;
                }

                // P-Vertex가 평면 밖에 있으면 걸쳐 있는 상태
                if (XMVectorGetX(XMPlaneDotCoord(Plane, P)) > 0.0f)
                {
                    bFullyInside = false;
                }
            }

            return bFullyInside ? ECullingResult::FullyInside : ECullingResult::Intersecting;
        }
    };
}