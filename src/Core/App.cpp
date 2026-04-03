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

    WNDCLASSEXW WindowClass = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        UApp::WindowProc,
        0L,
        0L,
        InstanceHandle,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"VerstappenEngineClass",
        nullptr
    };
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

    WindowHandle = ::CreateWindowW(
        WindowClass.lpszClassName,
        AppName.c_str(),
        WindowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        ScreenWidth,
        ScreenHeight,
        nullptr,
        nullptr,
        InstanceHandle,
        this);

    if (!WindowHandle)
    {
        return false;
    }

    ::ShowWindow(WindowHandle, InCmdShow);
    ::UpdateWindow(WindowHandle);

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight))
    {
        return false;
    }

    SceneManager->Initialize();

    UI::FEditorModuleDependencies Dependencies;
    Dependencies.WindowHandle = WindowHandle;
    Dependencies.Renderer = Renderer.get();
    Dependencies.SceneManager = SceneManager.get();
    Dependencies.Device = Renderer->GetDevice();
    Dependencies.DeviceContext = Renderer->GetContext();

    if (!EditorLayer->Initialize(Dependencies))
    {
        return false;
    }

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

    return static_cast<int>(Message.wParam);
}

void UApp::Update(float InDeltaTime)
{
    UpdateFramePerformanceMetrics(InDeltaTime);
    SceneManager->Update(InDeltaTime);
    EditorLayer->SetFramePerformanceMetrics(FramePerformanceMetrics);
    EditorLayer->Update(InDeltaTime);
}

void UApp::Render()
{
    Renderer->BeginFrame();
    EditorLayer->Draw();
    Renderer->EndFrame();
}

void UApp::UpdateFramePerformanceMetrics(float InDeltaTime)
{
    FramePerformanceMetrics.DeltaTimeSeconds = InDeltaTime;
    FramePerformanceMetrics.FramesPerSecond = InDeltaTime > 0.0f ? (1.0f / InDeltaTime) : 0.0f;
    FramePerformanceMetrics.ElapsedTimeMilliseconds += InDeltaTime * 1000.0f;
    ++FramePerformanceMetrics.FrameIndex;
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
