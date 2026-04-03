#include <UI/Panels/SConsolePanel.h>

namespace UI
{
    bool SConsolePanel::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SConsolePanel::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InContext;
        (void)InDeltaTime;
    }

    void SConsolePanel::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SConsolePanel::GetPanelType() const
    {
        return EEditorPanelType::Console;
    }

    const char* SConsolePanel::GetPanelName() const
    {
        return "Console";
    }
}
