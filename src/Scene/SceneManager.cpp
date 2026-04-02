#include <Scene/SceneManager.h>

namespace ExtremeScene
{
    SceneManager::SceneManager() {}
    SceneManager::~SceneManager() {}

    void SceneManager::Initialize()
    {
        m_pSceneData = std::make_unique<SceneDataSOA>();
        // 초기 5만 개 데이터는 여기서 생성하거나 OBJ 로더에서 호출할 예정
    }

    void SceneManager::Update(float deltaTime)
    {
        // 5만 개 객체의 애니메이션이나 상태 업데이트 로직 (필요 시)
    }
}