#include <Core/App.h>
#include <chrono>
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <UI/EditorLayer.h>

UApp::UApp() 
    : WindowHandle(nullptr)
    , InstanceHandle(nullptr)
    , ScreenWidth(0)
    , ScreenHeight(0)
{
    Renderer = std::make_unique<Graphics::URenderer>();
    SceneManager = std::make_unique<Scene::USceneManager>();
    EditorLayer = std::make_unique<UI::UEditorLayer>();
}

UApp::~UApp() {}

bool UApp::Initialize(HINSTANCE InHInstance, int InCmdShow)
{
    InstanceHandle = InHInstance;

    WNDCLASSEXW WindowClass = { sizeof(WNDCLASSEXW), CS_CLASSDC, UApp::WindowProc, 0L, 0L, InstanceHandle, nullptr, nullptr, nullptr, nullptr, L"VerstappenEngineClass", nullptr };
    ::RegisterClassExW(&WindowClass);

    DWORD WindowStyle;

#ifdef _DEBUG
    ScreenWidth = 1920;
    ScreenHeight = 1080;
    WindowStyle = WS_OVERLAPPEDWINDOW;
#else
    ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    WindowStyle = WS_POPUP;
#endif

    WindowHandle = ::CreateWindowW(WindowClass.lpszClassName, AppName.c_str(), WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, ScreenWidth, ScreenHeight, nullptr, nullptr, InstanceHandle, this);

    if (!WindowHandle) return false;

    ::ShowWindow(WindowHandle, InCmdShow);
    ::UpdateWindow(WindowHandle);

    // 1. 그래픽 렌더러 초기화
    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight)) return false;
    
    // 2. 씬 매니저 초기화
    SceneManager->Initialize();
    
    // 3. 에디터 UI 초기화 (수정된 인터페이스 반영)
    UI::FEditorModuleDependencies EditorDeps;
    EditorDeps.WindowHandle = WindowHandle;
    EditorDeps.Renderer = Renderer.get();
    EditorDeps.SceneManager = SceneManager.get();
    EditorDeps.Device = Renderer->GetDevice();
    EditorDeps.DeviceContext = Renderer->GetContext();

    if (!EditorLayer->Initialize(EditorDeps)) return false;

    return true;
}

int UApp::Run()
{
    MSG Message = {};
    auto LastTime = std::chrono::high_resolution_clock::now();

    while (Message.message != WM_QUIT)
    {
        if (::PeekMessageW(&Message, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&Message);
            ::DispatchMessageW(&Message);
            continue;
        }

        auto CurrentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> FrameDelta = CurrentTime - LastTime;
        DeltaTime = FrameDelta.count();
        LastTime = CurrentTime;

        Update(DeltaTime);
        Render();
    }

    return (int)Message.wParam;
}

void UApp::Update(float InDeltaTime)
{
    SceneManager->Update(InDeltaTime);
    EditorLayer->Update(InDeltaTime);
}

void UApp::Render()
{
    Renderer->BeginFrame();

    // [TODO] 렌더 루프 구현 (UpdateInstanceBuffer 활용)

    EditorLayer->Draw();
    Renderer->EndFrame();
}

LRESULT CALLBACK UApp::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}
