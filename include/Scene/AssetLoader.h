#pragma once
#include <string>
#include <cstdint>

namespace Scene
{
    class USceneManager;

    /**
     * OBJ 로드 시 요약 정보를 반환하는 구조체.
     */
    struct FObjMeshSummary
    {
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;
    };

    /**
     * 에셋 로드 옵션.
     */
    struct FAssetLoadOptions
    {
        bool bBakeBinary = true;
    };

    /**
     * Verstappen Engine의 고속 에셋 로더 클래스.
     */
    class FAssetLoader
    {
    public:
        /** [성능] 사과 메시지를 USceneManager의 SOA 구조에 직접 주입 */
        static bool LoadAppleMid(USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary = nullptr);
    };
}