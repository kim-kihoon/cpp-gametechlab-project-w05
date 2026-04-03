#include <Scene/AssetLoader.h>
#include <Core/PathManager.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace
{
    struct FParsedObjData
    {
        Scene::FObjMeshSummary Summary;
        bool bHasVertices = false;
    };

    bool ParseObjSummary(const std::filesystem::path& InObjPath, FParsedObjData& OutParsedData)
    {
        std::ifstream InputStream(InObjPath);
        if (!InputStream.is_open())
        {
            return false;
        }

        Math::FBox Bounds = {};
        Bounds.Min = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()
        };
        Bounds.Max = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        };

        std::string Line;
        while (std::getline(InputStream, Line))
        {
            if (Line.size() > 2 && Line[0] == 'v' && Line[1] == ' ')
            {
                std::istringstream VertexStream(Line.substr(2));
                float X = 0.0f;
                float Y = 0.0f;
                float Z = 0.0f;

                if (VertexStream >> X >> Y >> Z)
                {
                    Bounds.Min.x = (std::min)(Bounds.Min.x, X);
                    Bounds.Min.y = (std::min)(Bounds.Min.y, Y);
                    Bounds.Min.z = (std::min)(Bounds.Min.z, Z);
                    Bounds.Max.x = (std::max)(Bounds.Max.x, X);
                    Bounds.Max.y = (std::max)(Bounds.Max.y, Y);
                    Bounds.Max.z = (std::max)(Bounds.Max.z, Z);
                    ++OutParsedData.Summary.VertexCount;
                    OutParsedData.bHasVertices = true;
                }
            }
            else if (Line.size() > 2 && Line[0] == 'f' && Line[1] == ' ')
            {
                std::istringstream FaceStream(Line.substr(2));
                std::string FaceToken;
                uint32_t FaceVertexCount = 0;

                while (FaceStream >> FaceToken)
                {
                    ++FaceVertexCount;
                }

                if (FaceVertexCount >= 3)
                {
                    OutParsedData.Summary.TriangleCount += FaceVertexCount - 2;
                }
            }
        }

        if (!OutParsedData.bHasVertices)
        {
            return false;
        }

        OutParsedData.Summary.LocalBounds = Bounds;
        return true;
    }

    float ResolveSpacing(const Scene::FAssetLoadOptions& InOptions, const Scene::FObjMeshSummary& InSummary)
    {
        if (InOptions.Spacing > 0.0f)
        {
            return InOptions.Spacing;
        }

        const float SizeX = InSummary.LocalBounds.Max.x - InSummary.LocalBounds.Min.x;
        const float SizeY = InSummary.LocalBounds.Max.y - InSummary.LocalBounds.Min.y;
        const float SizeZ = InSummary.LocalBounds.Max.z - InSummary.LocalBounds.Min.z;
        const float LargestAxis = (std::max)((std::max)(SizeX, SizeY), (std::max)(SizeZ, 1.0f));
        return LargestAxis * 1.5f;
    }

    Math::FBox TranslateBounds(const Math::FBox& InBounds, float InOffsetX, float InOffsetY, float InOffsetZ)
    {
        Math::FBox WorldBounds = InBounds;
        WorldBounds.Min.x += InOffsetX;
        WorldBounds.Min.y += InOffsetY;
        WorldBounds.Min.z += InOffsetZ;
        WorldBounds.Max.x += InOffsetX;
        WorldBounds.Max.y += InOffsetY;
        WorldBounds.Max.z += InOffsetZ;
        return WorldBounds;
    }
}

namespace Scene
{
    bool FAssetLoader::LoadAppleMid(USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary)
    {
        const std::filesystem::path ObjPath = std::filesystem::path(ExtremeCore::FPathManager::GetMeshPath()) / L"apple_mid.obj";
        return LoadObjIntoScene(ObjPath.wstring(), InSceneManager, InOptions, OutSummary);
    }

    bool FAssetLoader::LoadObjIntoScene(const std::wstring& InObjPath, USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary)
    {
        if (InOptions.GridX == 0 || InOptions.GridY == 0 || InOptions.GridZ == 0)
        {
            return false;
        }

        FParsedObjData ParsedData = {};
        if (!ParseObjSummary(std::filesystem::path(InObjPath), ParsedData))
        {
            return false;
        }

        if (OutSummary)
        {
            *OutSummary = ParsedData.Summary;
        }

        const uint64_t GridCapacity = static_cast<uint64_t>(InOptions.GridX) * InOptions.GridY * InOptions.GridZ;
        const uint64_t RequestedCount = static_cast<uint64_t>(InOptions.ObjectCount);
        const uint64_t MaxCount = static_cast<uint64_t>(FSceneDataSOA::MAX_OBJECTS);
        const uint64_t ClampedCount = (std::min)(RequestedCount, (std::min)(GridCapacity, MaxCount));
        const uint32_t SpawnCount = static_cast<uint32_t>(ClampedCount);
        if (SpawnCount == 0)
        {
            return false;
        }

        if (InOptions.bClearSceneFirst)
        {
            InSceneManager.ResetScene();
        }

        const float Spacing = ResolveSpacing(InOptions, ParsedData.Summary);
        const float HalfWidth = (static_cast<float>(InOptions.GridX) - 1.0f) * Spacing * 0.5f;
        const float HalfHeight = (static_cast<float>(InOptions.GridY) - 1.0f) * Spacing * 0.5f;
        const float HalfDepth = (static_cast<float>(InOptions.GridZ) - 1.0f) * Spacing * 0.5f;

        for (uint32_t Index = 0; Index < SpawnCount; ++Index)
        {
            const uint32_t GridX = Index % InOptions.GridX;
            const uint32_t GridY = (Index / InOptions.GridX) % InOptions.GridY;
            const uint32_t GridZ = Index / (InOptions.GridX * InOptions.GridY);

            const float OffsetX = static_cast<float>(GridX) * Spacing - HalfWidth;
            const float OffsetY = static_cast<float>(GridY) * Spacing - HalfHeight;
            const float OffsetZ = static_cast<float>(GridZ) * Spacing - HalfDepth;

            const DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixTranslation(OffsetX, OffsetY, OffsetZ);
            const Math::FBox WorldBounds = TranslateBounds(ParsedData.Summary.LocalBounds, OffsetX, OffsetY, OffsetZ);

            if (!InSceneManager.AddObject(WorldBounds, WorldMatrix, InOptions.MeshID, InOptions.MaterialID))
            {
                return false;
            }
        }

        return true;
    }
}
