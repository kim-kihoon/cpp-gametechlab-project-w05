#include <UI/EditorLayer.h>

namespace ExtremeUI
{
    EditorLayer::EditorLayer() {}
    EditorLayer::~EditorLayer() {}

    bool EditorLayer::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        // [TODO] ImGui Context 생성 및 DX11 ImGui Implementation 초기화
        return true;
    }

    void EditorLayer::Update(float deltaTime)
    {
        // ImGui 프레임 시작
    }

    void EditorLayer::Draw()
    {
        // 각종 패널 그리기 (DrawControlPanel, DrawSceneManager 등)
    }

    void EditorLayer::Cleanup()
    {
        // ImGui Shutdown
    }

    void EditorLayer::DrawControlPanel() {}
    void EditorLayer::DrawSceneManager() {}
    void EditorLayer::DrawPropertyWindow() {}
    void EditorLayer::DrawConsole() {}
}