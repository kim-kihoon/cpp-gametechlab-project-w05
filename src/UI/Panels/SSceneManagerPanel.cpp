#include <UI/Panels/SSceneManagerPanel.h>

namespace UI
{
    bool SSceneManagerPanel::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SSceneManagerPanel::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InContext;
        (void)InDeltaTime;
    }

    void SSceneManagerPanel::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SSceneManagerPanel::GetPanelType() const
    {
        return EEditorPanelType::SceneManager;
    }

    const char* SSceneManagerPanel::GetPanelName() const
    {
        return "Scene Manager";
    }
}
