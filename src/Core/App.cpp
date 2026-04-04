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
#include <cmath>
#include <limits>
#include <sstream>
#include <windowsx.h>

namespace
{
    constexpr float MAX_PITCH_RADIANS = 1.55334306f;

    DirectX::XMVECTOR BuildCameraForward(const Graphics::FCameraState& InCameraState)
    {
        const float CosPitch = std::cos(InCameraState.PitchRadians);
        const float SinPitch = std::sin(InCameraState.PitchRadians);
        const float CosYaw = std::cos(InCameraState.YawRadians);
        const float SinYaw = std::sin(InCameraState.YawRadians);

        return DirectX::XMVector3Normalize(DirectX::XMVectorSet(
            CosPitch * CosYaw,
            CosPitch * SinYaw,
            SinPitch,
            0.0f));
    }

    Math::FMatrix BuildCameraViewProjection(const Graphics::FCameraState& InCameraState, int InViewportWidth, int InViewportHeight)
    {
        const float AspectRatio = (InViewportHeight == 0) ? 1.0f : static_cast<float>(InViewportWidth) / static_cast<float>(InViewportHeight);
        const DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&InCameraState.Position);
        const DirectX::XMVECTOR Forward = BuildCameraForward(InCameraState);
        const DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const DirectX::XMVECTOR CameraTarget = DirectX::XMVectorAdd(CameraPosition, Forward);

        const DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(CameraPosition, CameraTarget, WorldUp);
        const DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(InCameraState.FOVDegrees),
            AspectRatio,
            InCameraState.NearClip,
            InCameraState.FarClip);
        return View * Projection;
    }

    Math::FMatrix BuildCameraViewMatrix(const Graphics::FCameraState& InCameraState)
    {
        const DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&InCameraState.Position);
        const DirectX::XMVECTOR Forward = BuildCameraForward(InCameraState);
        const DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const DirectX::XMVECTOR CameraTarget = DirectX::XMVectorAdd(CameraPosition, Forward);
        return DirectX::XMMatrixLookAtLH(CameraPosition, CameraTarget, WorldUp);
    }

    Math::FMatrix BuildCameraProjectionMatrix(const Graphics::FCameraState& InCameraState, int InViewportWidth, int InViewportHeight)
    {
        const float AspectRatio = (InViewportHeight == 0) ? 1.0f : static_cast<float>(InViewportWidth) / static_cast<float>(InViewportHeight);
        return DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(InCameraState.FOVDegrees),
            AspectRatio,
            InCameraState.NearClip,
            InCameraState.FarClip);
    }

    bool ComputeSceneBounds(const Scene::USceneManager& InSceneManager, Math::FBox& OutBounds)
    {
        const Scene::FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        const uint32_t Count = InSceneManager.GetObjectCount();
        if (!SceneData || Count == 0) return false;

        float MinX = (std::numeric_limits<float>::max)();
        float MinY = (std::numeric_limits<float>::max)();
        float MinZ = (std::numeric_limits<float>::max)();
        float MaxX = std::numeric_limits<float>::lowest();
        float MaxY = std::numeric_limits<float>::lowest();
        float MaxZ = std::numeric_limits<float>::lowest();

        for (uint32_t Index = 0; Index < Count; ++Index)
        {
            MinX = (std::min)(MinX, SceneData->MinX[Index]);
            MinY = (std::min)(MinY, SceneData->MinY[Index]);
            MinZ = (std::min)(MinZ, SceneData->MinZ[Index]);
            MaxX = (std::max)(MaxX, SceneData->MaxX[Index]);
            MaxY = (std::max)(MaxY, SceneData->MaxY[Index]);
            MaxZ = (std::max)(MaxZ, SceneData->MaxZ[Index]);
        }

        OutBounds.Min = { MinX, MinY, MinZ };
        OutBounds.Max = { MaxX, MaxY, MaxZ };
        return true;
    }

    void FitCameraToScene(const Scene::USceneManager& InSceneManager, Graphics::FCameraState& InOutCameraState, int InViewportWidth, int InViewportHeight)
    {
        Math::FBox SceneBounds = {};
        if (!ComputeSceneBounds(InSceneManager, SceneBounds)) return;

        const DirectX::XMFLOAT3 Center = {
            (SceneBounds.Min.x + SceneBounds.Max.x) * 0.5f,
            (SceneBounds.Min.y + SceneBounds.Max.y) * 0.5f,
            (SceneBounds.Min.z + SceneBounds.Max.z) * 0.5f
        };

        const DirectX::XMFLOAT3 Extents = {
            (SceneBounds.Max.x - SceneBounds.Min.x) * 0.5f,
            (SceneBounds.Max.y - SceneBounds.Min.y) * 0.5f,
            (SceneBounds.Max.z - SceneBounds.Min.z) * 0.5f
        };

        const float BoundingRadius = std::sqrt(
            (Extents.x * Extents.x) +
            (Extents.y * Extents.y) +
            (Extents.z * Extents.z));

        const float AspectRatio = (InViewportHeight == 0) ? 1.0f : static_cast<float>(InViewportWidth) / static_cast<float>(InViewportHeight);
        const float VerticalHalfFov = DirectX::XMConvertToRadians(InOutCameraState.FOVDegrees) * 0.5f;
        const float HorizontalHalfFov = std::atan(std::tan(VerticalHalfFov) * AspectRatio);
        const float LimitingHalfFov = (std::min)(VerticalHalfFov, HorizontalHalfFov);
        const float CameraDistance = (BoundingRadius / std::sin(LimitingHalfFov)) + 12.0f;

        const DirectX::XMVECTOR Forward = DirectX::XMVector3Normalize(DirectX::XMVectorSet(1.0f, 1.0f, -0.5f, 0.0f));
        const DirectX::XMVECTOR CenterVector = DirectX::XMLoadFloat3(&Center);
        const DirectX::XMVECTOR CameraPosition = DirectX::XMVectorSubtract(CenterVector, DirectX::XMVectorScale(Forward, CameraDistance));

        DirectX::XMStoreFloat3(&InOutCameraState.Position, CameraPosition);
        InOutCameraState.YawRadians = std::atan2(DirectX::XMVectorGetY(Forward), DirectX::XMVectorGetX(Forward));
        InOutCameraState.PitchRadians = std::atan2(
            DirectX::XMVectorGetZ(Forward),
            std::sqrt(
                (DirectX::XMVectorGetX(Forward) * DirectX::XMVectorGetX(Forward)) +
                (DirectX::XMVectorGetY(Forward) * DirectX::XMVectorGetY(Forward))));
        InOutCameraState.FarClip = (std::max)(InOutCameraState.FarClip, CameraDistance + BoundingRadius + 16.0f);
        InOutCameraState.NearClip = 0.1f;
    }
}

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
    if (bIsCOMInitialized)
    {
        ::CoUninitialize();
    }
}

bool UApp::Initialize(HINSTANCE InHInstance, int InCmdShow)
{
    InstanceHandle = InHInstance;

    const HRESULT COMResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(COMResult) || COMResult == S_FALSE)
    {
        bIsCOMInitialized = true;
    }
    else if (COMResult != RPC_E_CHANGED_MODE)
    {
        return false;
    }

    WNDCLASSEXW WindowClass = { sizeof(WNDCLASSEXW), CS_CLASSDC, UApp::WindowProc, 0L, 0L, InstanceHandle, nullptr, nullptr, nullptr, nullptr, L"VerstappenEngineClass", nullptr };
    ::RegisterClassExW(&WindowClass);

    DWORD WindowStyle;
    ScreenWidth = 1920;
    ScreenHeight = 1080;
    WindowStyle = WS_OVERLAPPEDWINDOW;

    WindowHandle = ::CreateWindowW(WindowClass.lpszClassName, AppName.c_str(), WindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, ScreenWidth, ScreenHeight, nullptr, nullptr, InstanceHandle, this);
    if (!WindowHandle) return false;

    ::SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    ::ShowWindow(WindowHandle, InCmdShow);
    ::UpdateWindow(WindowHandle);

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight)) return false;

    SceneManager->Initialize();
    if (!Scene::FAssetLoader::LoadDefaultScene(*SceneManager, &CameraState) &&
        !Scene::FSceneSerializer::LoadWorldMatrices(*SceneManager))
    {
        Scene::FSceneGridSpawnRequest GridReq;
        GridReq.Width = 50;
        GridReq.Height = 50;
        GridReq.Depth = 20;
        GridReq.Spacing = 1.0f;
        SceneManager->SpawnStaticMeshGrid(GridReq);
    }

    FitCameraToScene(*SceneManager, CameraState, ScreenWidth, ScreenHeight);
    Renderer->SetCameraState(CameraState);

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
	UpdateCamera(InDeltaTime);

    UniformCullingAndRenderCollect();

	UpdateFramePerformanceMetrics(InDeltaTime);
	SceneManager->Update(InDeltaTime);
	EditorLayer->Update(InDeltaTime);
}

void UApp::UniformCullingAndRenderCollect()
{
    if (SceneManager && SceneManager->GetGrid() != nullptr)
    {
        Math::FMatrix View = BuildCameraViewMatrix(CameraState);
        Math::FMatrix Proj = BuildCameraProjectionMatrix(CameraState, ScreenWidth, ScreenHeight);

        Math::FFrustum CameraFrustum;
        CameraFrustum.Update(View, Proj);

        Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
        SceneData->ResetRenderQueue();

        SceneManager->GetGrid()->QueryFrustum(
            CameraFrustum,
            SceneData->RenderQueue.data(),
            SceneData->RenderCount,
            Scene::FSceneDataSOA::MAX_OBJECTS
        );
    }
}

/*void UApp::Update(float InDeltaTime)
{
    UpdateCamera(InDeltaTime);

    if (SceneManager && SceneManager->GetSceneData() != nullptr)
    {
        Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
        SceneData->ResetRenderQueue();

        const uint32_t TotalObjectCount = SceneManager->GetSceneStatistics().TotalObjectCount;
        for (uint32_t ObjectIndex = 0; ObjectIndex < TotalObjectCount; ++ObjectIndex)
        {
            SceneData->AddToRenderQueue(ObjectIndex);
        }

        SceneManager->SetVisibleObjectCount(TotalObjectCount);
    }

    UpdateFramePerformanceMetrics(InDeltaTime);
    SceneManager->Update(InDeltaTime);
    EditorLayer->Update(InDeltaTime);
}*/

void UApp::Render()
{
    Renderer->SetCameraState(CameraState);
    Renderer->BeginFrame();
    Renderer->RenderScene(*SceneManager);
    EditorLayer->Draw();
    Renderer->EndFrame();
}

void UApp::UpdateCamera(float InDeltaTime)
{
    const DirectX::XMVECTOR Forward = BuildCameraForward(CameraState);
    const DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(WorldUp, Forward));
    DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&CameraState.Position);

    const float MoveDistance = CameraState.MoveSpeed * InDeltaTime;
    if ((::GetAsyncKeyState('W') & 0x8000) != 0) Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(Forward, MoveDistance));
    if ((::GetAsyncKeyState('S') & 0x8000) != 0) Position = DirectX::XMVectorSubtract(Position, DirectX::XMVectorScale(Forward, MoveDistance));
    if ((::GetAsyncKeyState('A') & 0x8000) != 0) Position = DirectX::XMVectorSubtract(Position, DirectX::XMVectorScale(Right, MoveDistance));
    if ((::GetAsyncKeyState('D') & 0x8000) != 0) Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(Right, MoveDistance));

    if (PendingWheelDelta != 0.0f)
    {
        Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(Forward, PendingWheelDelta * CameraState.WheelSpeed));
        PendingWheelDelta = 0.0f;
    }

    DirectX::XMStoreFloat3(&CameraState.Position, Position);
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

	const uint32_t TotalObjects = SceneManager->GetObjectCount();
	const uint32_t VisibleObjects = SceneManager->GetVisibleObjectCount();

    std::wostringstream TitleStream;
    TitleStream.precision(2);
    TitleStream << std::fixed
                << L"Verstappen Engine | Scene Preview | FPS(avg): " << AverageFPS
                << L" | Frame(ms): " << AverageFrameMilliseconds
                << L" | Visible: " << VisibleObjects
                << L"/" << TotalObjects;
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
    case WM_RBUTTONDOWN:
        if (AppInstance != nullptr)
        {
            AppInstance->bIsRightMouseLooking = true;
            AppInstance->LastMousePosition.x = GET_X_LPARAM(lParam);
            AppInstance->LastMousePosition.y = GET_Y_LPARAM(lParam);
            ::SetCapture(hWnd);
        }
        return 0;
    case WM_RBUTTONUP:
        if (AppInstance != nullptr)
        {
            AppInstance->bIsRightMouseLooking = false;
            ::ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (AppInstance != nullptr && AppInstance->bIsRightMouseLooking)
        {
            const POINT CurrentMousePosition = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int DeltaX = CurrentMousePosition.x - AppInstance->LastMousePosition.x;
            const int DeltaY = CurrentMousePosition.y - AppInstance->LastMousePosition.y;

            AppInstance->CameraState.YawRadians += static_cast<float>(DeltaX) * AppInstance->CameraState.LookSensitivity;
            AppInstance->CameraState.PitchRadians = (std::clamp)(
                AppInstance->CameraState.PitchRadians - static_cast<float>(DeltaY) * AppInstance->CameraState.LookSensitivity,
                -MAX_PITCH_RADIANS,
                MAX_PITCH_RADIANS);
            AppInstance->LastMousePosition = CurrentMousePosition;
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        if (AppInstance != nullptr)
        {
            AppInstance->PendingWheelDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
        }
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, Message, wParam, lParam);
}
