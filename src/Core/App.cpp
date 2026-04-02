#include <Core/App.h>
#include <chrono>

// 실체 헤더들 포함 (Incomplete Type 에러 해결)
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <UI/EditorLayer.h>

App::App() : m_hWnd(nullptr), m_hInstance(nullptr) 
{
    // 스마트 포인터 초기화
    m_pRenderer = std::make_unique<ExtremeGraphics::Renderer>();
    m_pSceneManager = std::make_unique<ExtremeScene::SceneManager>();
    m_pEditorLayer = std::make_unique<ExtremeUI::EditorLayer>();
}
App::~App() {}

bool App::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    m_hInstance = hInstance;

    // 1. 유니코드 전용 윈도우 클래스 등록
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, App::WindowProc, 0L, 0L, m_hInstance, nullptr, nullptr, nullptr, nullptr, L"VerstappenEngineClass", nullptr };
    ::RegisterClassExW(&wc);

    // 2. 유니코드 전용 윈도우 생성
    m_hWnd = ::CreateWindowW(wc.lpszClassName, m_AppName.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, m_ScreenWidth, m_ScreenHeight, nullptr, nullptr, m_hInstance, this);

    if (!m_hWnd) return false;

    // 3. 윈도우 표시
    ::ShowWindow(m_hWnd, nCmdShow);
    ::UpdateWindow(m_hWnd);

    // 4. 엔진 각 모듈 초기화 (순서 중요: Graphics -> Scene -> UI)
    if (!m_pRenderer->Initialize(m_hWnd, m_ScreenWidth, m_ScreenHeight)) return false;
    m_pSceneManager->Initialize();
    
    // ImGui는 Renderer의 Device가 필요함
    if (!m_pEditorLayer->Initialize(m_hWnd, m_pRenderer->GetDevice(), m_pRenderer->GetContext())) return false;

    return true;
}

int App::Run()
{
    MSG msg = {};
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT)
    {
        if (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            continue;
        }

        // 시간 측정 (DeltaTime)
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - lastTime;
        m_DeltaTime = deltaTime.count();
        lastTime = currentTime;

        Update(m_DeltaTime);
        Render();
    }

    return (int)msg.wParam;
}

void App::Update(float deltaTime)
{
    // 로직 업데이트
    m_pSceneManager->Update(deltaTime);
    m_pEditorLayer->Update(deltaTime);
}

void App::Render()
{
    // 렌더링 시작
    m_pRenderer->BeginFrame();

    // [TODO] 메인 렌더링 (Draw Objects)
    
    // UI 렌더링
    m_pEditorLayer->Draw();

    // 화면 출력
    m_pRenderer->EndFrame();
}

// 윈도우 메시지 핸들러
LRESULT CALLBACK App::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}
