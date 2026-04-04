#pragma once
#include <Math/MathTypes.h>
#include <DirectXCollision.h>
#include <algorithm> // std::max, std::min 사용을 위함

namespace Math
{
    enum class ECullingResult
    {
        Outside = 0,
        Intersecting = 1,
        FullyInside = 2
    };

    struct alignas(16) FFrustum
    {
        DirectX::BoundingFrustum NativeFrustum;

        FVector4 Planes[6];
        FVector Corners[8];
        XMFLOAT3 AABBMin;
        XMFLOAT3 AABBMax;

        inline void Update(const FMatrix& View, const FMatrix& Projection)
        {
            BuildFromViewProjection(View, Projection);
            BuildFromMatrix(XMMatrixMultiply(View, Projection));
        }

        inline void BuildFromViewProjection(const FMatrix& View, const FMatrix& Projection)
        {
            DirectX::BoundingFrustum LocalFrustum;
            DirectX::BoundingFrustum::CreateFromMatrix(LocalFrustum, Projection);

            const FMatrix InverseView = XMMatrixInverse(nullptr, View);
            LocalFrustum.Transform(NativeFrustum, InverseView);
        }

        inline void BuildFromMatrix(const FMatrix& ViewProj)
        {
            FMatrix T = XMMatrixTranspose(ViewProj);

            FVector C0 = T.r[0];
            FVector C1 = T.r[1];
            FVector C2 = T.r[2];
            FVector C3 = T.r[3];

            Planes[0] = XMPlaneNormalize(XMVectorAdd(C3, C0));
            Planes[1] = XMPlaneNormalize(XMVectorSubtract(C3, C0));
            Planes[2] = XMPlaneNormalize(XMVectorAdd(C3, C1));
            Planes[3] = XMPlaneNormalize(XMVectorSubtract(C3, C1));
            Planes[4] = XMPlaneNormalize(C2);
            Planes[5] = XMPlaneNormalize(XMVectorSubtract(C3, C2));

            FMatrix InvVP = XMMatrixInverse(nullptr, ViewProj);

            FVector NDCCorners[8] = {
                XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f), XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f),
                XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f), XMVectorSet(1.0f,  1.0f, 0.0f, 1.0f),
                XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f), XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f),
                XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f), XMVectorSet(1.0f,  1.0f, 1.0f, 1.0f)
            };

            FVector vMin = XMVectorReplicate(FLT_MAX);
            FVector vMax = XMVectorReplicate(-FLT_MAX);

            for (int i = 0; i < 8; i++)
            {
                Corners[i] = XMVector3TransformCoord(NDCCorners[i], InvVP);

                vMin = XMVectorMin(vMin, Corners[i]);
                vMax = XMVectorMax(vMax, Corners[i]);
            }

            XMStoreFloat3(&AABBMin, vMin);
            XMStoreFloat3(&AABBMax, vMax);
        }

        // SoA Min/Max 데이터를 DirectX API에 맞게 Center/Extents로 변환 -> SIMD계산에 좋지는 않지만 나쁘지 않다고 함. 
        // 원래 방식도 해볼것을 고려해봐야 할듯. 추후에 CPU를 극한으로 끌어올리겠다면.
        inline ECullingResult TestBox(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) const
        {
            // SoA Min/Max 데이터를 DirectX API에 맞게 Center/Extents로 변환
            const DirectX::XMFLOAT3 Center = {
                (MinX + MaxX) * 0.5f,
                (MinY + MaxY) * 0.5f,
                (MinZ + MaxZ) * 0.5f
            };
            const DirectX::XMFLOAT3 Extents = {
                (MaxX - MinX) * 0.5f,
                (MaxY - MinY) * 0.5f,
                (MaxZ - MinZ) * 0.5f
            };

            const DirectX::BoundingBox Box(Center, Extents);
            const DirectX::ContainmentType Result = NativeFrustum.Contains(Box);

            if (Result == DirectX::DISJOINT) return ECullingResult::Outside;
            if (Result == DirectX::CONTAINS) return ECullingResult::FullyInside;
            return ECullingResult::Intersecting;
        }

        inline ECullingResult TestBox(const FBox& Box) const
        {
            return TestBox(Box.Min.x, Box.Min.y, Box.Min.z, Box.Max.x, Box.Max.y, Box.Max.z);
        }
    };
}
