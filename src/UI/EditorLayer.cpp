#include <UI/EditorLayer.h>

namespace UI
{
    UEditorLayer::UEditorLayer() {}
    UEditorLayer::~UEditorLayer() {}

    bool UEditorLayer::Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
    {
        return true;
    }

    void UEditorLayer::Update(float DeltaTime) {}
    void UEditorLayer::Draw() {}
    void UEditorLayer::Cleanup() {}

    void UEditorLayer::DrawControlPanel() {}
    void UEditorLayer::DrawSceneManager() {}
    void UEditorLayer::DrawPropertyWindow() {}
    void UEditorLayer::DrawConsole() {}
}