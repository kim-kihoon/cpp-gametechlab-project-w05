#pragma once
#include <Math/MathTypes.h>

namespace ExtremeMath
{
    enum class CullingResult
    {
        Outside = 0,
        Intersecting = 1,
        FullyInside = 2  // [초격차] 이 결과가 나오면 하위 객체 검사 무조건 통과 (Skip)
    };

    struct OptimizedFrustum
    {
        // 평면 방정식 (nx, ny, nz, d) -> 평면 6개를 16-byte 정렬된 벡터 배열로 저장
        __declspec(align(16)) XMVECTOR Planes[6];

        // 카메라의 ViewProj 행렬로부터 6개의 평면을 추출
        void BuildFromMatrix(CXMMATRIX viewProj)
        {
            // (Left, Right, Bottom, Top, Near, Far)
            // 평면 추출 후 XMPlaneNormalize 로 정규화하는 코드 필요 (자세한 Math 생략)
            XMFLOAT4X4 m;
            XMStoreFloat4x4(&m, viewProj);
            // ... (Extract planes using Gribb/Hartner method or D3D11 standard)
            // 이곳에 SIMD 기반의 평면 추출 및 정규화 로직 구현
        }

        // [초격차] 1개의 AABB에 대해 SIMD 기반 고속 충돌 검사
        // 리턴: Outside, Intersecting, FullyInside
        inline CullingResult TestAABB(const AlignedAABB& aabb) const
        {
            // 모든 평면에 대해 완전히 안쪽(FullyInside)인지 추적
            bool fullyInside = true;

            for (int i = 0; i < 6; ++i)
            {
                XMVECTOR plane = Planes[i];
                
                // SIMD로 평면의 법선과 Extent의 Dot Product 연산 (P-N Vertex 방식)
                XMVECTOR planeNormal = XMVectorSetW(plane, 0.0f);
                XMVECTOR planeNormalAbs = XMVectorAbs(planeNormal);
                XMVECTOR r = XMVector3Dot(aabb.Extents, planeNormalAbs);
                XMVECTOR s = XMVector3Dot(aabb.Center, plane);
                
                // s.x + r.x < 0 이면 Outside (평면 뒤에 있음)
                // s.x - r.x < 0 이면 Intersecting (평면에 걸침)
                float d = XMVectorGetX(s);
                float radius = XMVectorGetX(r);

                if (d + radius < 0.0f)
                {
                    return CullingResult::Outside; // 하나라도 평면 바깥이면 완전히 바깥
                }
                if (d - radius < 0.0f)
                {
                    fullyInside = false; // 평면에 걸침
                }
            }

            return fullyInside ? CullingResult::FullyInside : CullingResult::Intersecting;
        }
    };
}