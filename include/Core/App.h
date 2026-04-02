#pragma once
#include <windows.h>
#include <memory>
#include <string>

// 전방 선언 (컴파일 속도 최적화)
namespace ExtremeGraphics { class Renderer; class DebugRenderer; }
namespace ExtremeScene { class SceneManager; }
namespace ExtremeUI { class EditorLayer; }

class App
{
public:
    App();
    ~App();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int Run();

private:
    // 핵심 윈도우 메시지 처리
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void Update(float deltaTime);
    void Render();

private:
    // 윈도우 관련
    HWND m_hWnd;
    HINSTANCE m_hInstance;
    std::wstring m_AppName = L"VerstappenEngine";
    int m_ScreenWidth = 1920;
    int m_ScreenHeight = 1080;

    // 핵심 모듈 (Smart Pointers 사용)
    // 4인 팀이 각자 개발한 모듈들이 여기에 붙게 됩니다.
    std::unique_ptr<ExtremeGraphics::Renderer> m_pRenderer;
    std::unique_ptr<ExtremeScene::SceneManager> m_pSceneManager;
    std::unique_ptr<ExtremeUI::EditorLayer> m_pEditorLayer;

    // 타이머 관련 (FPS, Elapsed Time 측정용)
    float m_DeltaTime = 0.0f;
};
