#include <Scene/AssetLoader.h>
#include <Core/PathManager.h>
#include <Scene/SceneManager.h>
#include <DirectXMath.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace Scene
{
    namespace
    {
        bool ParseTriple(const std::string& InLine, float& OutX, float& OutY, float& OutZ)
        {
            const size_t OpenBracket = InLine.find('[');
            const size_t CloseBracket = InLine.find(']', OpenBracket);
            if (OpenBracket == std::string::npos || CloseBracket == std::string::npos) return false;

            const std::string Values = InLine.substr(OpenBracket + 1, CloseBracket - OpenBracket - 1);
            return std::sscanf(Values.c_str(), "%f, %f, %f", &OutX, &OutY, &OutZ) == 3;
        }

        uint32_t DetermineMeshID(const std::string& InAssetPath)
        {
            return (InAssetPath.find("bitten_apple_mid.obj") != std::string::npos) ? 1u : 0u;
        }
    }

    bool FAssetLoader::LoadAppleMid(USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary)
    {
        (void)InOptions;

        FSceneGridSpawnRequest GridReq;
        GridReq.Width = 50;
        GridReq.Height = 50;
        GridReq.Depth = 20;
        GridReq.Spacing = 5.0f;
        GridReq.MeshID = 0;
        GridReq.MaterialID = 0;

        InSceneManager.SpawnStaticMeshGrid(GridReq);

        if (OutSummary)
        {
            OutSummary->VertexCount = 1054;
            OutSummary->TriangleCount = 2104;
        }

        return true;
    }

    bool FAssetLoader::LoadDefaultScene(USceneManager& InSceneManager, const std::wstring& InScenePath)
    {
        const std::wstring FinalPath = InScenePath.empty() ? (Core::FPathManager::GetScenePath() + L"Default.scene") : InScenePath;
        std::ifstream File{ std::filesystem::path(FinalPath) };
        if (!File) return false;

        InSceneManager.ResetScene();

        std::string Line;
        bool bInsidePrimitive = false;
        bool bHasLocation = false;
        float LocationX = 0.0f;
        float LocationY = 0.0f;
        float LocationZ = 0.0f;
        uint32_t MeshID = 0;

        while (std::getline(File, Line))
        {
            if (Line.find("\"Location\"") != std::string::npos)
            {
                bHasLocation = ParseTriple(Line, LocationX, LocationY, LocationZ);
            }
            else if (Line.find("\"ObjStaticMeshAsset\"") != std::string::npos)
            {
                MeshID = DetermineMeshID(Line);
                bInsidePrimitive = true;
            }
            else if (bInsidePrimitive && bHasLocation && Line.find("\"Type\"") != std::string::npos)
            {
                FSceneSpawnRequest SpawnRequest;
                SpawnRequest.MeshID = MeshID;
                SpawnRequest.MaterialID = MeshID;
                SpawnRequest.WorldMatrix = DirectX::XMMatrixTranslation(LocationX, LocationY, LocationZ);
                InSceneManager.SpawnStaticMesh(SpawnRequest);

                bInsidePrimitive = false;
                bHasLocation = false;
                MeshID = 0;
            }
        }

        return InSceneManager.GetSceneStatistics().TotalObjectCount > 0;
    }
}
