#include <UI/Panels/SPerformanceOverlay.h>

namespace UI
{
    bool SPerformanceOverlay::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SPerformanceOverlay::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InContext;
        (void)InDeltaTime;
    }

    void SPerformanceOverlay::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SPerformanceOverlay::GetPanelType() const
    {
        return EEditorPanelType::Overlay;
    }

    const char* SPerformanceOverlay::GetPanelName() const
    {
        return "Performance Overlay";
    }
}
