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
    // ID3D11DeviceContext::Map 시 Constant Buffer에 밀어넣을 실제 데이터 구조체.
    // HLSL에서는 float3x4 로 받아 4x4로 조립하여 사용합니다.
    struct Packed3x4Matrix
    {
        float m[3][4];

        // XMMATRIX로부터 변환 (열 우선/행 우선 여부는 셰이더 설정에 따라 조정)
        inline void Store(CXMMATRIX mat)
        {
            XMFLOAT4X4 temp;
            XMStoreFloat4x4(&temp, mat);
            // 4번째 행(0, 0, 0, 1)을 제외하고 복사 (Translation은 보통 4행에 위치하지만, HLSL 로직에 따라 트랜스포즈 필요)
            m[0][0] = temp._11; m[0][1] = temp._12; m[0][2] = temp._13; m[0][3] = temp._14;
            m[1][0] = temp._21; m[1][1] = temp._22; m[1][2] = temp._23; m[1][3] = temp._24;
            m[2][0] = temp._31; m[2][1] = temp._32; m[2][2] = temp._33; m[2][3] = temp._34;
        }
    };

    // 가장 빠른 충돌 검사를 위한 정렬된 AABB (Center + Extents 방식이 연산이 가장 빠름)
    __declspec(align(16)) struct AlignedAABB
    {
        XMVECTOR Center;  // w 컴포넌트는 사용하지 않거나 패딩으로 활용
        XMVECTOR Extents; // 각 축의 반경(Half-size)
    };
}