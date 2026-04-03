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
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <windows.h>

namespace UI
{
    /**
     * Verstappen Engine의 에디터 레이어 총괄 클래스.
     * IEditorPanel 기반의 플러그형 패널 아키텍처를 사용하여 확장성과 유지보수성을 극대화함.
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

        /** 성능 지표 업데이트 */
        void SetFramePerformanceMetrics(const Core::FFramePerformanceMetrics& InMetrics);
        
        /** 윈도우 메시지 처리 (ImGui용) */
        bool HandleWindowMessage(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

    private:
        static constexpr size_t PANEL_COUNT = static_cast<size_t>(EEditorPanelType::Count);

        /** 패널 간 공유되는 가변 상태 데이터 */
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
        
        // 패널 인스턴스 관리
        std::array<IEditorPanel*, PANEL_COUNT> Panels = {};
        SPerformanceOverlay PerformanceOverlay;
        SControlPanel ControlPanel;
        SSceneFileSystemPanel SceneFileSystemPanel;
        SSceneManagerPanel SceneManagerPanel;
        SPropertyWindowPanel PropertyWindowPanel;
        SConsolePanel ConsolePanel;

        bool bIsInitialized = false;
    };
}