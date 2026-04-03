#pragma once
#include <Scene/SceneData.h>
#include <Scene/SceneTypes.h>
#include <Scene/UniformGrid.h>
#include <memory>
#include <string>

namespace Scene
{
    /**
     * 씬 데이터, 선택 상태, 파일 I/O 인터페이스를 관리하는 매니저 클래스.
     */
    class USceneManager
    {
    public:
        USceneManager();
        ~USceneManager();

        void Initialize();
        void Update(float DeltaTime);

        void ResetScene();
        bool SpawnStaticMesh(const FSceneSpawnRequest& InRequest);
        void SpawnStaticMeshGrid(const FSceneGridSpawnRequest& InRequest);
        bool SaveSceneBinary(const std::wstring& InFilePath) const;
        bool LoadSceneBinary(const std::wstring& InFilePath);

        bool SelectObject(uint32_t InObjectIndex);
        void ClearSelection();

        FSceneDataSOA* GetSceneData() { return SceneData.get(); }
        const FSceneDataSOA* GetSceneData() const { return SceneData.get(); }
        UUniformGrid* GetGrid() { return Grid.get(); }
        const UUniformGrid* GetGrid() const { return Grid.get(); }
        const FSceneStatistics& GetSceneStatistics() const { return SceneStatistics; }
        const FSceneSelectionData& GetSelectionData() const { return SelectionData; }

    private:
        void UpdateVisibleObjectCount();
        void ResetSelectionState();

        std::unique_ptr<FSceneDataSOA> SceneData;
        std::unique_ptr<UUniformGrid> Grid;
        FSceneStatistics SceneStatistics;
        FSceneSelectionData SelectionData;
    };
}
