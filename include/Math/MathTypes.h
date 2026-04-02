#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

namespace ExtremeMath
{
    using namespace DirectX;

    // SIMD 레지스터(XMM)에 바로 올릴 수 있는 16-Byte Aligned Matrix.
    // 연산용으로만 사용하고, GPU로 전송할 때는 Packed3x4Matrix로 변환합니다.
    __declspec(align(16)) struct AlignedMatrix
    {
        XMMATRIX m;
    };

    // [초격차] GPU 대역폭 25% 절감: 64바이트 -> 48바이트 압축 전송용   
    // 16바이트 정렬을 유지하면서 3x4를 보관합니다.
    struct Packed3x4Matrix
    {
        XMVECTOR Row0;
        XMVECTOR Row1;
        XMVECTOR Row2;

        // [초격차] XMM 레지스터에서 직접 저장 (대입 오버헤드 0)
        inline void Store(CXMMATRIX mat)
        {
            // 행렬을 전치(Transpose)하여 3x4 형태로 추출 (셰이더는 열 우선 기대 가능성 높음)
            XMMATRIX t = XMMatrixTranspose(mat);
            Row0 = t.r[0];
            Row1 = t.r[1];
            Row2 = t.r[2];
        }
    };

    // 가장 빠른 충돌 검사를 위한 정렬된 AABB (Center + Extents 방식이 연산이 가장 빠름)
    __declspec(align(16)) struct AlignedAABB
    {
        XMVECTOR Center;  // w 컴포넌트는 사용하지 않거나 패딩으로 활용
        XMVECTOR Extents; // 각 축의 반경(Half-size)
    };
}