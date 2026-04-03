#include <UI/EditorLayer.h>
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <cwchar>

namespace UI
{
    UEditorLayer::UEditorLayer() {}
    UEditorLayer::~UEditorLayer() {}

    bool UEditorLayer::Initialize(const FEditorModuleDependencies& InDependencies)
    {
        Dependencies = InDependencies;

        if (Dependencies.Renderer)
        {
            SharedState.DebugRenderSettings = Dependencies.Renderer->GetDebugRenderSettings();
        }

        wcscpy_s(
            SharedState.SceneFileSystemState.SaveFilePath.data(),
            SharedState.SceneFileSystemState.SaveFilePath.size(),
            L"Data\\DefaultScene\\Default.scene.bin");
        wcscpy_s(
            SharedState.SceneFileSystemState.LoadFilePath.data(),
            SharedState.SceneFileSystemState.LoadFilePath.size(),
            L"Data\\DefaultScene\\Default.scene.bin");

        BuildPanelRegistry();

        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (!Panel || !Panel->Initialize(Context))
            {
                return false;
            }
        }

        SharedState.ConsoleState.PushMessage("Editor interface initialized.", EConsoleMessageSeverity::Info);
        return true;
    }

    void UEditorLayer::Update(float DeltaTime)
    {
        RefreshFrameSnapshot();

        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (Panel)
            {
                Panel->Update(Context, DeltaTime);
            }
        }
    }

    void UEditorLayer::Draw()
    {
        FEditorContext Context = BuildContext();
        for (IEditorPanel* Panel : Panels)
        {
            if (Panel)
            {
                Panel->Draw(Context);
            }
        }
    }

    void UEditorLayer::Cleanup() {}

    void UEditorLayer::SetFramePerformanceMetrics(const Core::FFramePerformanceMetrics& InMetrics)
    {
        FrameData.PerformanceMetrics = InMetrics;
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
        if (!Dependencies.SceneManager)
        {
            FrameData.TotalObjectCount = 0;
            FrameData.VisibleObjectCount = 0;
            FrameData.bHasSelection = false;
            FrameData.SelectedObjectIndex = 0;
            FrameData.SelectedMeshID = 0;
            FrameData.SelectedMaterialID = 0;
            return;
        }

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
