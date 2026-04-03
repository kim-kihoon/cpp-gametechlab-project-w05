#include <UI/Panels/SPropertyWindowPanel.h>

namespace UI
{
    bool SPropertyWindowPanel::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SPropertyWindowPanel::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InContext;
        (void)InDeltaTime;
    }

    void SPropertyWindowPanel::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SPropertyWindowPanel::GetPanelType() const
    {
        return EEditorPanelType::PropertyWindow;
    }

    const char* SPropertyWindowPanel::GetPanelName() const
    {
        return "Property Window";
    }
}
