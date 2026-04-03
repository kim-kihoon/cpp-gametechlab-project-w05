#pragma once
#include <Core/AppTypes.h>
#include <windows.h>
#include <memory>
#include <string>

namespace Graphics { class URenderer; }
namespace Scene { class USceneManager; }
namespace UI { class UEditorLayer; }

/**
 * Verstappen Engine 메인 애플리케이션 클래스로 각 서브시스템을 조립한다.
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
    void UpdateFramePerformanceMetrics(float InDeltaTime);

private:
    HWND WindowHandle;
    HINSTANCE InstanceHandle;
    std::wstring AppName = L"Verstappen Engine";
    int ScreenWidth;
    int ScreenHeight;

    std::unique_ptr<Graphics::URenderer> Renderer;
    std::unique_ptr<Scene::USceneManager> SceneManager;
    std::unique_ptr<UI::UEditorLayer> EditorLayer;

    float DeltaTime = 0.0f;
    Core::FFramePerformanceMetrics FramePerformanceMetrics;
};
