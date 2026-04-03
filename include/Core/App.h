#pragma once
#include <windows.h>
#include <memory>
#include <string>

namespace Graphics { class URenderer; }
namespace Scene { class USceneManager; }
namespace UI { class UEditorLayer; }

/**
 * VerstappenEngine의 메인 애플리케이션 클래스.
 */
class UApp
{
public:
    UApp();
    ~UApp();

    /** 엔진 초기화 */
    bool Initialize(HINSTANCE InHInstance, int InCmdShow);
    
    /** 메인 루프 실행 */
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Update(float DeltaTime);
    void Render();

private:
    HWND WindowHandle;
    HINSTANCE InstanceHandle;
    std::wstring AppName = L"cpp-gametechlab-project-w05";
    int ScreenWidth = 1920;
    int ScreenHeight = 1080;

    std::unique_ptr<Graphics::URenderer> Renderer;
    std::unique_ptr<Scene::USceneManager> SceneManager;
    std::unique_ptr<UI::UEditorLayer> EditorLayer;

    float DeltaTime = 0.0f;
};
