#include <UI/EditorLayer.h>
#include <Scene/AssetLoader.h>
#include <Scene/SceneManager.h>
#include <Scene/SceneSerializer.h>
#include <cstdio>

#if __has_include(<imgui.h>)
#if __has_include(<backends/imgui_impl_dx11.h>) && __has_include(<backends/imgui_impl_win32.h>)
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>
#define VERSTAPPEN_WITH_IMGUI 1
#elif __has_include(<imgui_impl_dx11.h>) && __has_include(<imgui_impl_win32.h>)
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#define VERSTAPPEN_WITH_IMGUI 1
#endif
#endif

#ifndef VERSTAPPEN_WITH_IMGUI
#define VERSTAPPEN_WITH_IMGUI 0
#endif

namespace UI
{
    UEditorLayer::UEditorLayer()
    {
        BinaryFileName.fill(0);
        std::snprintf(BinaryFileName.data(), BinaryFileName.size(), "scene_matrices.bin");
    }

    UEditorLayer::~UEditorLayer() {}

    bool UEditorLayer::Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
    {
        WindowHandle = InWindowHandle;
        Device = InDevice;
        Context = InContext;
        StatusMessage = "Editor ready.";

#if VERSTAPPEN_WITH_IMGUI
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(InWindowHandle);
        ImGui_ImplDX11_Init(InDevice, InContext);
        bIsInitialized = true;
#endif

        return true;
    }

    void UEditorLayer::Update(float DeltaTime) {}

    void UEditorLayer::Draw()
    {
#if VERSTAPPEN_WITH_IMGUI
        if (!bIsInitialized)
        {
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawControlPanel();
        DrawSceneManager();
        DrawPropertyWindow();
        DrawConsole();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif
    }

    void UEditorLayer::Cleanup()
    {
#if VERSTAPPEN_WITH_IMGUI
        if (bIsInitialized)
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            bIsInitialized = false;
        }
#endif
    }

    void UEditorLayer::SetSceneManager(Scene::USceneManager* InSceneManager)
    {
        SceneManager = InSceneManager;
    }

    bool UEditorLayer::HandleWindowMessage(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
    {
#if VERSTAPPEN_WITH_IMGUI
        if (bIsInitialized)
        {
            return ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam);
        }
#endif
        return false;
    }

    void UEditorLayer::DrawControlPanel()
    {
#if VERSTAPPEN_WITH_IMGUI
        ImGui::Begin("Control Panel");

        if (SceneManager != nullptr)
        {
            ImGui::Text("Scene Objects: %u / %u", SceneManager->GetObjectCount(), SceneManager->GetMaxObjectCount());
        }
        else
        {
            ImGui::TextUnformatted("SceneManager is not connected.");
        }

        if (ImGui::Button("Load Apple OBJ"))
        {
            Scene::FObjMeshSummary MeshSummary{};
            const bool bLoaded = SceneManager != nullptr && Scene::FAssetLoader::LoadAppleMid(*SceneManager, Scene::FAssetLoadOptions{}, &MeshSummary);
            StatusMessage = bLoaded ? "OBJ asset loaded into SceneManager." : "Failed to load OBJ asset.";
        }

        ImGui::SameLine();
        if (ImGui::Button("Save Matrices"))
        {
            const bool bSaved = SceneManager != nullptr && Scene::FSceneSerializer::SaveWorldMatrices(*SceneManager);
            StatusMessage = bSaved ? "World matrices saved." : "Failed to save world matrices.";
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Matrices"))
        {
            const bool bLoaded = SceneManager != nullptr && Scene::FSceneSerializer::LoadWorldMatrices(*SceneManager);
            StatusMessage = bLoaded ? "World matrices loaded." : "Failed to load world matrices.";
        }

        ImGui::Separator();
        ImGui::TextWrapped("All file-system access is routed through FPathManager.");
        ImGui::End();
#endif
    }

    void UEditorLayer::DrawSceneManager()
    {
#if VERSTAPPEN_WITH_IMGUI
        ImGui::Begin("Scene Manager");

        if (SceneManager == nullptr || SceneManager->GetSceneData() == nullptr)
        {
            ImGui::TextUnformatted("Scene data is unavailable.");
            ImGui::End();
            return;
        }

        const Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
        const uint32_t ObjectCount = SceneManager->GetObjectCount();

        if (SelectedObjectIndex != UINT32_MAX)
        {
            ImGui::Text("Selected Index: %u", SelectedObjectIndex);
        }
        else
        {
            ImGui::TextUnformatted("Selected Index: None");
        }

        ImGui::Separator();

        ImGuiListClipper Clipper;
        Clipper.Begin(static_cast<int>(ObjectCount));
        while (Clipper.Step())
        {
            for (int RowIndex = Clipper.DisplayStart; RowIndex < Clipper.DisplayEnd; ++RowIndex)
            {
                const uint32_t ObjectIndex = static_cast<uint32_t>(RowIndex);
                char Label[96] = {};
                std::snprintf(
                    Label,
                    sizeof(Label),
                    "#%05u | Mesh %u | Material %u",
                    ObjectIndex,
                    SceneData->MeshIDs[ObjectIndex],
                    SceneData->MaterialIDs[ObjectIndex]);

                if (ImGui::Selectable(Label, SelectedObjectIndex == ObjectIndex))
                {
                    SelectedObjectIndex = ObjectIndex;
                }
            }
        }

        ImGui::End();
#endif
    }

    void UEditorLayer::DrawPropertyWindow()
    {
#if VERSTAPPEN_WITH_IMGUI
        ImGui::Begin("Property Window");

        if (SceneManager == nullptr || !SceneManager->IsValidIndex(SelectedObjectIndex))
        {
            ImGui::TextUnformatted("Select an object index from Scene Manager.");
            ImGui::End();
            return;
        }

        Math::FBox Bounds{};
        Math::FPacked3x4Matrix WorldMatrix{};
        SceneManager->GetBounds(SelectedObjectIndex, Bounds);
        SceneManager->GetWorldMatrixPacked(SelectedObjectIndex, WorldMatrix);

        ImGui::Text("Index: %u", SelectedObjectIndex);
        ImGui::Text("Visible: %s", SceneManager->GetSceneData()->IsVisible[SelectedObjectIndex] ? "true" : "false");
        ImGui::Text("Translation: %.2f %.2f %.2f", DirectX::XMVectorGetW(WorldMatrix.Row0), DirectX::XMVectorGetW(WorldMatrix.Row1), DirectX::XMVectorGetW(WorldMatrix.Row2));
        ImGui::Text("Bounds Min: %.2f %.2f %.2f", Bounds.Min.x, Bounds.Min.y, Bounds.Min.z);
        ImGui::Text("Bounds Max: %.2f %.2f %.2f", Bounds.Max.x, Bounds.Max.y, Bounds.Max.z);

        ImGui::End();
#endif
    }

    void UEditorLayer::DrawConsole()
    {
#if VERSTAPPEN_WITH_IMGUI
        ImGui::Begin("Console");
        ImGui::TextWrapped("%s", StatusMessage.c_str());
        ImGui::End();
#endif
    }
}
