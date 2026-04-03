#pragma once
#include <Math/MathTypes.h>
#include <DirectXCollision.h>

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
        DirectX::BoundingFrustum NativeFrustum;

        void BuildFromViewProjection(const FMatrix& View, const FMatrix& Projection)
        {
            DirectX::BoundingFrustum LocalFrustum;
            DirectX::BoundingFrustum::CreateFromMatrix(LocalFrustum, Projection);

            const FMatrix InverseView = XMMatrixInverse(nullptr, View);
            LocalFrustum.Transform(NativeFrustum, InverseView);
        }

        inline ECullingResult TestBox(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ) const
        {
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
