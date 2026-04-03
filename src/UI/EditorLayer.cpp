#include <UI/EditorLayer.h>
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <cwchar>

/**
 * [임시] imgui.h가 프로젝트에 포함되지 않은 상태이므로
 * 모든 ImGui 관련 호출을 주석 처리하거나 매크로로 보호합니다.
 * 팀원분이 ImGui 소스를 추가하시면 이 부분의 주석을 제거하십시오.
 */
#ifdef WITH_IMGUI
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace UI
{
    UEditorLayer::UEditorLayer() : bIsInitialized(false)
    {
        BuildPanelRegistry();
    }

    UEditorLayer::~UEditorLayer() 
    {
        Cleanup();
    }

    bool UEditorLayer::Initialize(const FEditorModuleDependencies& InDependencies)
    {
        Dependencies = InDependencies;

#ifdef WITH_IMGUI
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        if (!ImGui_ImplWin32_Init(Dependencies.WindowHandle)) return false;
        if (!ImGui_ImplDX11_Init(Dependencies.Device, Dependencies.DeviceContext)) return false;
        bIsInitialized = true;
#endif

        if (Dependencies.Renderer)
        {
            SharedState.DebugRenderSettings = Dependencies.Renderer->GetDebugRenderSettings();
        }

        wcscpy_s(SharedState.SceneFileSystemState.SaveFilePath.data(), SharedState.SceneFileSystemState.SaveFilePath.size(), L"Data\\DefaultScene\\Default.bin");
        wcscpy_s(SharedState.SceneFileSystemState.LoadFilePath.data(), SharedState.SceneFileSystemState.LoadFilePath.size(), L"Data\\DefaultScene\\Default.bin");

        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (Panel && !Panel->Initialize(Context)) return false;
        }

        return true;
    }

    void UEditorLayer::Update(float DeltaTime)
    {
        RefreshFrameSnapshot();
        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (Panel) Panel->Update(Context, DeltaTime);
        }
    }

    void UEditorLayer::Draw()
    {
#ifdef WITH_IMGUI
        if (!bIsInitialized) return;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
#endif

        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (Panel) Panel->Draw(Context);
        }

#ifdef WITH_IMGUI
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
    }

    void UEditorLayer::Cleanup()
    {
#ifdef WITH_IMGUI
        if (bIsInitialized)
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            bIsInitialized = false;
        }
#endif
    }

    void UEditorLayer::SetFramePerformanceMetrics(const Core::FFramePerformanceMetrics& InMetrics)
    {
        FrameData.PerformanceMetrics = InMetrics;
    }

    bool UEditorLayer::HandleWindowMessage(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
    {
#ifdef WITH_IMGUI
        if (bIsInitialized)
        {
            return ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam);
        }
#endif
        return false;
    }

    FEditorContext UEditorLayer::BuildContext()
    {
        FEditorContext Context;
        Context.Dependencies = &Dependencies;
        Context.SpawnSettings = &SharedState.SpawnSettings;
        Context.SceneFileSystemState = &SharedState.SceneFileSystemState;
        Context.DebugRenderSettings = &SharedState.DebugRenderSettings;
        Context.ConsoleState = &SharedState.ConsoleState;
        Context.FrameData = &FrameData;
        return Context;
    }

    void UEditorLayer::BuildPanelRegistry()
    {
        Panels[static_cast<size_t>(EEditorPanelType::Overlay)] = &PerformanceOverlay;
        Panels[static_cast<size_t>(EEditorPanelType::ControlPanel)] = &ControlPanel;
        Panels[static_cast<size_t>(EEditorPanelType::SceneFileSystem)] = &SceneFileSystemPanel;
        Panels[static_cast<size_t>(EEditorPanelType::SceneManager)] = &SceneManagerPanel;
        Panels[static_cast<size_t>(EEditorPanelType::PropertyWindow)] = &PropertyWindowPanel;
        Panels[static_cast<size_t>(EEditorPanelType::Console)] = &ConsolePanel;
    }

    void UEditorLayer::RefreshFrameSnapshot()
    {
        if (!Dependencies.SceneManager) return;
        const Scene::FSceneStatistics& Statistics = Dependencies.SceneManager->GetSceneStatistics();
        const Scene::FSceneSelectionData& SelectionData = Dependencies.SceneManager->GetSelectionData();
        FrameData.TotalObjectCount = Statistics.TotalObjectCount;
        FrameData.VisibleObjectCount = Statistics.VisibleObjectCount;
        FrameData.bHasSelection = SelectionData.bHasSelection;
        FrameData.SelectedObjectIndex = SelectionData.ObjectIndex;
        FrameData.SelectedMeshID = SelectionData.MeshID;
        FrameData.SelectedMaterialID = SelectionData.MaterialID;
    }
}