#pragma once
#include <Scene/SceneData.h>
#include <Scene/UniformGrid.h>
#include <memory>

namespace ExtremeScene
{
    class SceneManager
    {
    public:
        SceneManager();
        ~SceneManager();

        void Initialize();
        void Update(float deltaTime);
        
        // 5만 개의 SOA 데이터에 접근하기 위한 메서드
        SceneDataSOA* GetSceneData() { return m_pSceneData.get(); }
        UniformGrid* GetGrid() { return m_pGrid.get(); }

    private:
        std::unique_ptr<SceneDataSOA> m_pSceneData;
        std::unique_ptr<UniformGrid> m_pGrid;
    };
}