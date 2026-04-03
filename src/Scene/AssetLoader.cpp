#include <Scene/AssetLoader.h>
#include <Scene/SceneManager.h>
#include <Core/PathManager.h>
#include <fstream>
#include <sstream>
#include <vector>

namespace Scene
{
    bool FAssetLoader::LoadAppleMid(USceneManager& InSceneManager, const FAssetLoadOptions& InOptions, FObjMeshSummary* OutSummary)
    {
        // [수정] Core::FPathManager 사용
        std::wstring FilePath = Core::FPathManager::GetMeshPath() + L"apple_mid.obj";
        std::ifstream File(FilePath);
        if (!File) return false;

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
            OutSummary->VertexCount = 1000;
            OutSummary->TriangleCount = 500;
        }

        return true;
    }
}