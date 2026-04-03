#include <UI/Panels/SSceneFileSystemPanel.h>
#include <Scene/SceneManager.h>
#include <string>

namespace UI
{
    bool SSceneFileSystemPanel::Initialize(const FEditorContext& InContext)
    {
        (void)InContext;
        return true;
    }

    void SSceneFileSystemPanel::Update(const FEditorContext& InContext, float InDeltaTime)
    {
        (void)InDeltaTime;
        ProcessFileRequests(InContext);
    }

    void SSceneFileSystemPanel::Draw(const FEditorContext& InContext)
    {
        (void)InContext;
    }

    EEditorPanelType SSceneFileSystemPanel::GetPanelType() const
    {
        return EEditorPanelType::SceneFileSystem;
    }

    const char* SSceneFileSystemPanel::GetPanelName() const
    {
        return "Scene File System";
    }

    void SSceneFileSystemPanel::ProcessFileRequests(const FEditorContext& InContext) const
    {
        if (!InContext.Dependencies || !InContext.Dependencies->SceneManager || !InContext.SceneFileSystemState)
        {
            return;
        }

        Scene::USceneManager* SceneManager = InContext.Dependencies->SceneManager;
        FSceneFileSystemState& FileSystemState = *InContext.SceneFileSystemState;

        if (FileSystemState.bPendingSave)
        {
            const std::wstring SavePath = FileSystemState.SaveFilePath.data();
            const bool bSaved = SceneManager->SaveSceneBinary(SavePath);
            FileSystemState.bPendingSave = false;

            if (InContext.ConsoleState)
            {
                InContext.ConsoleState->PushMessage(
                    bSaved ? "Scene save request succeeded." : "Scene save request failed.",
                    bSaved ? EConsoleMessageSeverity::Info : EConsoleMessageSeverity::Error);
            }
        }

        if (FileSystemState.bPendingLoad)
        {
            const std::wstring LoadPath = FileSystemState.LoadFilePath.data();
            const bool bLoaded = SceneManager->LoadSceneBinary(LoadPath);
            FileSystemState.bPendingLoad = false;

            if (InContext.ConsoleState)
            {
                InContext.ConsoleState->PushMessage(
                    bLoaded ? "Scene load request succeeded." : "Scene load request failed.",
                    bLoaded ? EConsoleMessageSeverity::Info : EConsoleMessageSeverity::Error);
            }
        }
    }
}
