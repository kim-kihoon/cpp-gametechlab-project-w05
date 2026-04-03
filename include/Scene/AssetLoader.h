#pragma once
#include <Math/MathTypes.h>
#include <cstdint>
#include <string>

namespace Scene
{
    class USceneManager;

    struct FAssetLoadOptions
    {
        uint32_t ObjectCount = 50000;
        uint32_t GridX = 50;
        uint32_t GridY = 50;
        uint32_t GridZ = 20;
        float Spacing = 0.0f;
        uint32_t MeshID = 0;
        uint32_t MaterialID = 0;
        bool bClearSceneFirst = true;
    };

    struct FObjMeshSummary
    {
        uint32_t VertexCount = 0;
        uint32_t TriangleCount = 0;
        Math::FBox LocalBounds = {};
    };

    class FAssetLoader
    {
    public:
        static bool LoadAppleMid(USceneManager& InSceneManager, const FAssetLoadOptions& InOptions = FAssetLoadOptions(), FObjMeshSummary* OutSummary = nullptr);
        static bool LoadObjIntoScene(const std::wstring& InObjPath, USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary = nullptr);
    };
}
