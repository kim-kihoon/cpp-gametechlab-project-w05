#include <UI/Panels/SControlPanel.h>
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <Scene/SceneTypes.h>
#include <DirectXMath.h>

namespace UI
{
    bool SControlPanel::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SControlPanel::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InDeltaTime;
        ProcessSpawnRequests(InContext);
        SyncDebugRenderSettings(InContext);
    }

    void SControlPanel::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SControlPanel::GetPanelType() const
    {
        return EEditorPanelType::ControlPanel;
    }

    const char* SControlPanel::GetPanelName() const
    {
        return "Control Panel";
    }

    void SControlPanel::ProcessSpawnRequests(const FEditorContext& InContext) const
    {
        if (!InContext.Dependencies || !InContext.Dependencies->SceneManager || !InContext.SpawnSettings)
        {
            return;
        }

        Scene::USceneManager* SceneManager = InContext.Dependencies->SceneManager;
        FEditorSpawnSettings& SpawnSettings = *InContext.SpawnSettings;

        if (SpawnSettings.bPendingSingleSpawn)
        {
            for (uint32_t SpawnIndex = 0; SpawnIndex < SpawnSettings.SingleSpawnCount; ++SpawnIndex)
            {
                Scene::FSceneSpawnRequest SpawnRequest;
                SpawnRequest.MeshID = SpawnSettings.DefaultMeshID;
                SpawnRequest.MaterialID = SpawnSettings.DefaultMaterialID;
                SpawnRequest.WorldMatrix = DirectX::XMMatrixIdentity();
                SceneManager->SpawnStaticMesh(SpawnRequest);
            }

            SpawnSettings.bPendingSingleSpawn = false;
            if (InContext.ConsoleState)
            {
                InContext.ConsoleState->PushMessage("Single spawn request processed.", EConsoleMessageSeverity::Info);
            }
        }

        if (SpawnSettings.bPendingGridSpawn)
        {
            Scene::FSceneGridSpawnRequest GridRequest;
            GridRequest.Width = SpawnSettings.GridWidth;
            GridRequest.Height = SpawnSettings.GridHeight;
            GridRequest.Depth = SpawnSettings.GridDepth;
            GridRequest.Spacing = SpawnSettings.GridSpacing;
            GridRequest.MeshID = SpawnSettings.DefaultMeshID;
            GridRequest.MaterialID = SpawnSettings.DefaultMaterialID;
            SceneManager->SpawnStaticMeshGrid(GridRequest);

            SpawnSettings.bPendingGridSpawn = false;
            if (InContext.ConsoleState)
            {
                InContext.ConsoleState->PushMessage("Grid spawn request processed.", EConsoleMessageSeverity::Info);
            }
        }
    }

    void SControlPanel::SyncDebugRenderSettings(const FEditorContext& InContext) const
    {
        if (!InContext.Dependencies || !InContext.Dependencies->Renderer || !InContext.DebugRenderSettings)
        {
            return;
        }

        InContext.Dependencies->Renderer->SetDebugRenderSettings(*InContext.DebugRenderSettings);
    }
}
