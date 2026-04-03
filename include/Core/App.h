#pragma once
#include <windows.h>
#include <memory>
#include <string>

namespace Graphics { class URenderer; }
namespace Scene { class USceneManager; }
namespace UI { class UEditorLayer; }

/**
 * VerstappenEngine의 메인 애플리케이션 클래스.
 * Alienware M15 R5 (1920x1080, 165Hz) 환경에 최적화됨.
 */
class UApp
{
public:
    UApp();
    ~UApp();

    bool Initialize(HINSTANCE InHInstance, int InCmdShow);
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Update(float DeltaTime);
    void Render();

private:
    HWND WindowHandle;
    HINSTANCE InstanceHandle;
    std::wstring AppName = L"Verstappen Engine";
    
    // 대회 사양 고정 (Alienware M15 FHD)
    static constexpr int ScreenWidth = 1920;
    static constexpr int ScreenHeight = 1080;

    std::unique_ptr<Graphics::URenderer> Renderer;
    std::unique_ptr<Scene::USceneManager> SceneManager;
    std::unique_ptr<UI::UEditorLayer> EditorLayer;

    float DeltaTime = 0.0f;
};
