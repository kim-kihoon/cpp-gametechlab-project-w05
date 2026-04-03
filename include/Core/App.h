#pragma once
#include <windows.h>
#include <memory>
#include <string>

namespace Graphics { class URenderer; }
namespace Scene { class USceneManager; }
namespace UI { class UEditorLayer; }

/**
 * Verstappen Engine의 메인 애플리케이션 클래스.
 * 실행 시점의 모니터 해상도를 자동으로 감지하여 최적화됨.
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
    
    // 실행 시 감지된 해상도를 저장 (Picking 무결성용)
    int ScreenWidth;
    int ScreenHeight;

    std::unique_ptr<Graphics::URenderer> Renderer;
    std::unique_ptr<Scene::USceneManager> SceneManager;
    std::unique_ptr<UI::UEditorLayer> EditorLayer;

    float DeltaTime = 0.0f;
};
