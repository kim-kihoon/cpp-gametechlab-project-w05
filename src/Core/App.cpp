#include <Core/App.h>
#include <chrono>
#include <Graphics/Renderer.h>
#include <Scene/SceneManager.h>
#include <Scene/SceneData.h>
#include <UI/EditorLayer.h>
#include <Math/Frustum.h>

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
#ifdef _DEBUG
    ScreenWidth = 1280; ScreenHeight = 720;
    WindowStyle = WS_OVERLAPPEDWINDOW;
#else
    ScreenWidth = GetSystemMetrics(SM_CXSCREEN); ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    WindowStyle = WS_POPUP;
#endif

    WindowHandle = ::CreateWindowW(WindowClass.lpszClassName, AppName.c_str(), WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, ScreenWidth, ScreenHeight, nullptr, nullptr, InstanceHandle, this);
    if (!WindowHandle) return false;

    ::SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ::ShowWindow(WindowHandle, InCmdShow);
    ::UpdateWindow(WindowHandle);

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight)) return false;
    
    SceneManager->Initialize();
    
    // 사과 5만 개 생성 (간격 5.0)
    Scene::FSceneGridSpawnRequest GridReq;
    GridReq.Width = 50; GridReq.Height = 50; GridReq.Depth = 20; GridReq.Spacing = 10.0f;
    SceneManager->SpawnStaticMeshGrid(GridReq);
    
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
    // [성능] 매 프레임 Culling 수행
    if (SceneManager && SceneManager->GetGrid())
    {
        Math::FFrustum DummyFrustum;
        // 일단 모든 사과가 보이게끔 아주 큰 구 형태의 Frustum 평면을 임시로 설정 (테스트용)
        // 나중에 실제 카메라 행렬로부터 BuildFromMatrix 호출 필요
        for(int i=0; i<6; ++i) DummyFrustum.Planes[i] = DirectX::XMVectorSet(0, 1, 0, 10000.0f); 

        // 임시로 모든 객체를 보이게 강제 설정 (Culling 로직 테스트 전 렌더링 확인용)
        Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
        if (SceneData) {
            SceneData->ResetRenderQueue();
            uint32_t TargetCount = (std::min)(SceneData->MAX_OBJECTS, SceneManager->GetSceneStatistics().TotalObjectCount);
            for(uint32_t i=0; i < TargetCount; ++i) {
                SceneData->AddToRenderQueue(i);
            }
        }
    }

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

LRESULT CALLBACK UApp::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UApp* AppInstance = reinterpret_cast<UApp*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (AppInstance != nullptr && AppInstance->EditorLayer != nullptr)
    {
        if (AppInstance->EditorLayer->HandleWindowMessage(hWnd, message, wParam, lParam)) return 1;
    }
    switch (message)
    {
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, message, wParam, lParam);
}
