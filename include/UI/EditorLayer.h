#pragma once
#include <windows.h>
#include <d3d11.h>

namespace ExtremeUI
{
    class EditorLayer
    {
    public:
        EditorLayer();
        ~EditorLayer();

        bool Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* context);
        void Update(float deltaTime);
        void Draw();
        void Cleanup();

    private:
        // ImGui 창들을 그리는 내부 메서드
        void DrawControlPanel();
        void DrawSceneManager();
        void DrawPropertyWindow();
        void DrawConsole();
    };
}