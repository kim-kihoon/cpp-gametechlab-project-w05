#pragma once
#include <UI/EditorTypes.h>
#include <UI/IEditorPanel.h>
#include <UI/Panels/SConsolePanel.h>
#include <UI/Panels/SControlPanel.h>
#include <UI/Panels/SPerformanceOverlay.h>
#include <UI/Panels/SPropertyWindowPanel.h>
#include <UI/Panels/SSceneFileSystemPanel.h>
#include <UI/Panels/SSceneManagerPanel.h>
#include <array>

namespace UI
{
    /**
     * ImGui 기반 에디터 패널 조립 및 상태 공유를 담당하는 클래스.
     */
    class UEditorLayer
    {
    public:
        UEditorLayer();
        ~UEditorLayer();

        bool Initialize(const FEditorModuleDependencies& InDependencies);
        void Update(float DeltaTime);
        void Draw();
        void Cleanup();
        void SetFramePerformanceMetrics(const Core::FFramePerformanceMetrics& InMetrics);

    private:
        static constexpr size_t PANEL_COUNT = static_cast<size_t>(EEditorPanelType::Count);

        struct FEditorSharedState
        {
            FEditorSpawnSettings SpawnSettings = {};
            FSceneFileSystemState SceneFileSystemState = {};
            Graphics::FDebugRenderSettings DebugRenderSettings = {};
            FConsoleState ConsoleState = {};
        };

        FEditorContext BuildContext();
        void BuildPanelRegistry();
        void RefreshFrameSnapshot();

    private:
        FEditorModuleDependencies Dependencies;
        FEditorSharedState SharedState;
        FEditorFrameData FrameData;
        std::array<IEditorPanel*, PANEL_COUNT> Panels = {};
        SPerformanceOverlay PerformanceOverlay;
        SControlPanel ControlPanel;
        SSceneFileSystemPanel SceneFileSystemPanel;
        SSceneManagerPanel SceneManagerPanel;
        SPropertyWindowPanel PropertyWindowPanel;
        SConsolePanel ConsolePanel;
    };
}
