#pragma once
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <windows.h>

namespace Scene { class USceneManager; }

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
        void SetSceneManager(Scene::USceneManager* InSceneManager);
        bool HandleWindowMessage(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
        uint32_t GetSelectedObjectIndex() const { return SelectedObjectIndex; }

    private:
        void DrawControlPanel();
        void DrawSceneManager();
        void DrawPropertyWindow();
        void DrawConsole();

    private:
        HWND WindowHandle = nullptr;
        ID3D11Device* Device = nullptr;
        ID3D11DeviceContext* Context = nullptr;
        Scene::USceneManager* SceneManager = nullptr;
        uint32_t SelectedObjectIndex = UINT32_MAX;
        bool bIsInitialized = false;
        std::array<char, 128> BinaryFileName = {};
        std::string StatusMessage;
    };
}
