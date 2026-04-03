#pragma once
#include <windows.h>
#include <d3d11.h>

namespace UI
{
    /**
     * ImGui 기반 에디터 레이어를 담당하는 클래스.
     */
    class UEditorLayer
    {
    public:
        UEditorLayer();
        ~UEditorLayer();

        bool Initialize(HWND InWindowHandle, ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
        void Update(float DeltaTime);
        void Draw();
        void Cleanup();

    private:
        void DrawControlPanel();
        void DrawSceneManager();
        void DrawPropertyWindow();
        void DrawConsole();
    };
}