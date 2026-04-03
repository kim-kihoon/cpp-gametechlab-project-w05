#include <Core/App.h>
#include <Graphics/Renderer.h>
#include <Math/Frustum.h>
#include <Scene/AssetLoader.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <Scene/SceneSerializer.h>
#include <UI/EditorLayer.h>
#include <algorithm>
#include <chrono>
#include <sstream>

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

UApp::~UApp()
{
    if (EditorLayer) EditorLayer->Cleanup();
}

bool UApp::Initialize(HINSTANCE InHInstance, int InCmdShow)
{
    InstanceHandle = InHInstance;

    WNDCLASSEXW WindowClass = { sizeof(WNDCLASSEXW), CS_CLASSDC, UApp::WindowProc, 0L, 0L, InstanceHandle, nullptr, nullptr, nullptr, nullptr, L"VerstappenEngineClass", nullptr };
    ::RegisterClassExW(&WindowClass);

    DWORD WindowStyle;
//#ifdef _DEBUG
    ScreenWidth = 1920;
    ScreenHeight = 1080;
    WindowStyle = WS_OVERLAPPEDWINDOW;
//#else
//    ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
//    ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
//    WindowStyle = WS_POPUP;
//#endif

    WindowHandle = ::CreateWindowW(WindowClass.lpszClassName, AppName.c_str(), WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, ScreenWidth, ScreenHeight, nullptr, nullptr, InstanceHandle, this);
    if (!WindowHandle) return false;

    ::SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ::ShowWindow(WindowHandle, InCmdShow);
    ::UpdateWindow(WindowHandle);

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight)) return false;

    SceneManager->Initialize();
    if (!Scene::FAssetLoader::LoadDefaultScene(*SceneManager) &&
        !Scene::FSceneSerializer::LoadWorldMatrices(*SceneManager))
    {
        Scene::FSceneGridSpawnRequest GridReq;
        GridReq.Width = 50;
        GridReq.Height = 50;
        GridReq.Depth = 20;
        GridReq.Spacing = 1.0f;
        SceneManager->SpawnStaticMeshGrid(GridReq);
    }

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

        const auto CurrentTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float> FrameDelta = CurrentTime - LastTime;
        DeltaTime = FrameDelta.count();
        LastTime = CurrentTime;

        Update(DeltaTime);
        Render();
    }

    return static_cast<int>(Message.wParam);
}

void UApp::Update(float InDeltaTime)
{
    if (SceneManager && SceneManager->GetGrid())
    {
        Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
        if (SceneData)
        {
            SceneData->ResetRenderQueue();
            const uint32_t TargetCount = (std::min)(SceneData->MAX_OBJECTS, SceneManager->GetSceneStatistics().TotalObjectCount);
            for (uint32_t Index = 0; Index < TargetCount; ++Index)
            {
                SceneData->AddToRenderQueue(Index);
            }
        }
    }

    UpdateFramePerformanceMetrics(InDeltaTime);
    SceneManager->Update(InDeltaTime);
    EditorLayer->Update(InDeltaTime);
}

void UApp::Render()
{
    Renderer->BeginFrame();
    Renderer->RenderScene(*SceneManager);
    EditorLayer->Draw();
    Renderer->EndFrame();
}

void UApp::UpdateFramePerformanceMetrics(float InDeltaTime)
{
    FramePerformanceMetrics.DeltaTimeSeconds = InDeltaTime;
    FramePerformanceMetrics.ElapsedTimeMilliseconds = InDeltaTime * 1000.0f;
    FramePerformanceMetrics.FramesPerSecond = (InDeltaTime > 0.0f) ? (1.0f / InDeltaTime) : 0.0f;
    ++FramePerformanceMetrics.FrameIndex;

    static float TitleUpdateAccumulator = 0.0f;
    static uint64_t TitleUpdateFrames = 0;

    TitleUpdateAccumulator += InDeltaTime;
    ++TitleUpdateFrames;

    if (TitleUpdateAccumulator < 0.5f || !WindowHandle || !SceneManager)
    {
        return;
    }

    const float AverageFPS = (TitleUpdateAccumulator > 0.0f) ? (static_cast<float>(TitleUpdateFrames) / TitleUpdateAccumulator) : 0.0f;
    const float AverageFrameMilliseconds = (TitleUpdateFrames > 0) ? ((TitleUpdateAccumulator * 1000.0f) / static_cast<float>(TitleUpdateFrames)) : 0.0f;
    const uint32_t TotalObjects = SceneManager->GetSceneStatistics().TotalObjectCount;

    std::wostringstream TitleStream;
    TitleStream.precision(2);
    TitleStream << std::fixed
                << L"Verstappen Engine | Scene Preview | FPS(avg): " << AverageFPS
                << L" | Frame(ms): " << AverageFrameMilliseconds
                << L" | Objects: " << TotalObjects;
    ::SetWindowTextW(WindowHandle, TitleStream.str().c_str());

    TitleUpdateAccumulator = 0.0f;
    TitleUpdateFrames = 0;
}

LRESULT CALLBACK UApp::WindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    UApp* AppInstance = reinterpret_cast<UApp*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (AppInstance != nullptr && AppInstance->EditorLayer != nullptr)
    {
        if (AppInstance->EditorLayer->HandleWindowMessage(hWnd, Message, wParam, lParam)) return 1;
    }

    switch (Message)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, Message, wParam, lParam);
}
