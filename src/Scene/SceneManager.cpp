#include <Scene/SceneManager.h>

namespace Scene
{
    USceneManager::USceneManager() {}
    USceneManager::~USceneManager() {}

    void USceneManager::Initialize()
    {
        SceneData = std::make_unique<FSceneDataSOA>();
    }

    void USceneManager::Update(float DeltaTime)
    {
    }
}