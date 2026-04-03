#pragma once
#include <Scene/SceneData.h>
#include <Scene/UniformGrid.h>
#include <memory>

namespace Scene
{
    /**
     * 전체 씬의 오브젝트와 그리드를 관리하는 매니저 클래스.
     */
    class USceneManager
    {
    public:
        USceneManager();
        ~USceneManager();

        void Initialize();
        void Update(float DeltaTime);
        
        FSceneDataSOA* GetSceneData() { return SceneData.get(); }
        UUniformGrid* GetGrid() { return Grid.get(); }

    private:
        std::unique_ptr<FSceneDataSOA> SceneData;
        std::unique_ptr<UUniformGrid> Grid;
    };
}