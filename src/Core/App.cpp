#include <Core/App.h>
#include <Graphics/Renderer.h>
#include <Math/Frustum.h>
#include <Scene/AssetLoader.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <Math/MathTypes.h>
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

    Math::FRay CalculatePickingRay(const Graphics::FCameraState& Camera, int MouseX, int MouseY, int ScreenWidth, int ScreenHeight)
    {
        float ptX = (2.0f * static_cast<float>(MouseX)) / static_cast<float>(ScreenWidth) - 1.0f;
        float ptY = 1.0f - (2.0f * static_cast<float>(MouseY)) / static_cast<float>(ScreenHeight);

        Math::FMatrix view = BuildCameraViewMatrix(Camera);
        Math::FMatrix proj = BuildCameraProjectionMatrix(Camera, ScreenWidth, ScreenHeight);
        DirectX::XMMATRIX invViewProj = DirectX::XMMatrixInverse(nullptr, view * proj);

        DirectX::XMVECTOR nearPtVec = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(ptX, ptY, 0.0f, 1.0f), invViewProj);
        DirectX::XMVECTOR farPtVec = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(ptX, ptY, 1.0f, 1.0f), invViewProj);
        DirectX::XMVECTOR dirVec = DirectX::XMVectorSubtract(farPtVec, nearPtVec);

        DirectX::XMFLOAT3 origin, direction;
        DirectX::XMStoreFloat3(&origin, nearPtVec);
        DirectX::XMStoreFloat3(&direction, dirVec);

        return Math::FRay(origin, direction);
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

    DWORD WindowStyle = WS_OVERLAPPEDWINDOW;
    ScreenWidth = 1920;
    ScreenHeight = 1080;
    RECT WindowRect = { 0, 0, ScreenWidth, ScreenHeight };

    ::AdjustWindowRect(&WindowRect, WindowStyle, FALSE);

    int ActualWidth = WindowRect.right - WindowRect.left;
    int ActualHeight = WindowRect.bottom - WindowRect.top;

    WindowHandle = ::CreateWindowW(WindowClass.lpszClassName, AppName.c_str(), WindowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, ActualWidth, ActualHeight, nullptr, nullptr, InstanceHandle, this);
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

    Picking();
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
        SceneData->IsVisible.fill(false);

        SceneManager->GetGrid()->QueryFrustum(
            CameraFrustum,
            SceneData->RenderQueue.data(),
            SceneData->RenderCount,
            Scene::FSceneDataSOA::MAX_OBJECTS
        );

        for (uint32_t QueueIndex = 0; QueueIndex < SceneData->RenderCount; ++QueueIndex)
        {
            SceneData->IsVisible[SceneData->RenderQueue[QueueIndex]] = true;
        }
    }
}

void UApp::Picking()
{
    if (bPendingPick && SceneManager && SceneManager->GetGrid())
    {
        // 1. 화면 클릭 좌표를 기반으로 World 공간의 Ray 생성
        Math::FRay WorldRay = CalculatePickingRay(CameraState, PickPosition.x, PickPosition.y, ScreenWidth, ScreenHeight);

        // 2. 정밀 검사(Narrow Phase)를 수행할 람다 함수 정의
        auto PreciseTriangleTest = [&](uint32_t ObjIndex, float& OutHitDistance) -> bool
            {
                const Scene::FSceneDataSOA* SceneData = SceneManager->GetSceneData();
                uint32_t MeshID = SceneData->MeshIDs[ObjIndex];

                // 렌더러에서 해당 객체의 Mesh(정점/인덱스) 데이터 가져오기
                const auto* MeshRes = Renderer->GetMeshResource(MeshID);
                if (!MeshRes || MeshRes->SourceIndices.empty()) return false;

                // --- Ray를 객체의 Local 공간으로 변환 (역행렬 사용) ---
                const auto& PMat = SceneData->WorldMatrices[ObjIndex];
                DirectX::XMMATRIX WorldMat = DirectX::XMMatrixSet(
                    DirectX::XMVectorGetX(PMat.Row0), DirectX::XMVectorGetY(PMat.Row0), DirectX::XMVectorGetZ(PMat.Row0), 0.0f,
                    DirectX::XMVectorGetX(PMat.Row1), DirectX::XMVectorGetY(PMat.Row1), DirectX::XMVectorGetZ(PMat.Row1), 0.0f,
                    DirectX::XMVectorGetX(PMat.Row2), DirectX::XMVectorGetY(PMat.Row2), DirectX::XMVectorGetZ(PMat.Row2), 0.0f,
                    DirectX::XMVectorGetW(PMat.Row0), DirectX::XMVectorGetW(PMat.Row1), DirectX::XMVectorGetW(PMat.Row2), 1.0f);

                DirectX::XMVECTOR Det;
                DirectX::XMMATRIX InvWorld = DirectX::XMMatrixInverse(&Det, WorldMat);

                DirectX::XMVECTOR LocalOrigin = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&WorldRay.Origin), InvWorld);
                DirectX::XMVECTOR LocalDir = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&WorldRay.Direction), InvWorld);
                LocalDir = DirectX::XMVector3Normalize(LocalDir); // 정규화 필수

                bool bHit = false;
                float ClosestWorldT = OutHitDistance; // 현재 AABB까지의 거리

                // --- Ray-Triangle 교차 검사 (Möller–Trumbore 알고리즘) ---
                for (size_t i = 0; i < MeshRes->SourceIndices.size(); i += 3)
                {
                    uint32_t i0 = MeshRes->SourceIndices[i];
                    uint32_t i1 = MeshRes->SourceIndices[i + 1];
                    uint32_t i2 = MeshRes->SourceIndices[i + 2];

                    DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&MeshRes->SourceVertices[i0].Position);
                    DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&MeshRes->SourceVertices[i1].Position);
                    DirectX::XMVECTOR V2 = DirectX::XMLoadFloat3(&MeshRes->SourceVertices[i2].Position);

                    DirectX::XMVECTOR Edge1 = DirectX::XMVectorSubtract(V1, V0);
                    DirectX::XMVECTOR Edge2 = DirectX::XMVectorSubtract(V2, V0);
                    DirectX::XMVECTOR H = DirectX::XMVector3Cross(LocalDir, Edge2);

                    float A = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Edge1, H));
                    if (A > -0.00001f && A < 0.00001f) continue; // Ray가 삼각형과 평행함

                    float F = 1.0f / A;
                    DirectX::XMVECTOR S = DirectX::XMVectorSubtract(LocalOrigin, V0);
                    float U = F * DirectX::XMVectorGetX(DirectX::XMVector3Dot(S, H));
                    if (U < 0.0f || U > 1.0f) continue;

                    DirectX::XMVECTOR Q = DirectX::XMVector3Cross(S, Edge1);
                    float V = F * DirectX::XMVectorGetX(DirectX::XMVector3Dot(LocalDir, Q));
                    if (V < 0.0f || U + V > 1.0f) continue;

                    float LocalT = F * DirectX::XMVectorGetX(DirectX::XMVector3Dot(Edge2, Q));

                    // 삼각형과 교차함!
                    if (LocalT > 0.00001f)
                    {
                        // 로컬 공간에서 부딪힌 점을 찾아 다시 월드 공간으로 변환 (스케일 오차 보정)
                        DirectX::XMVECTOR LocalIntersection = DirectX::XMVectorAdd(LocalOrigin, DirectX::XMVectorScale(LocalDir, LocalT));
                        DirectX::XMVECTOR WorldIntersection = DirectX::XMVector3TransformCoord(LocalIntersection, WorldMat);

                        // 실제 월드 공간 기준의 거리를 계산
                        DirectX::XMVECTOR DistVec = DirectX::XMVectorSubtract(WorldIntersection, DirectX::XMLoadFloat3(&WorldRay.Origin));
                        float WorldT = DirectX::XMVectorGetX(DirectX::XMVector3Length(DistVec));

                        // 지금까지 찾은 것 중 가장 가깝다면 갱신
                        if (WorldT < ClosestWorldT)
                        {
                            ClosestWorldT = WorldT;
                            bHit = true;
                        }
                    }
                }

                if (bHit) OutHitDistance = ClosestWorldT;
                return bHit;
            };

        // 3. Grid에 Ray와 람다 함수(정밀 검사)를 함께 넘겨줍니다.
        uint32_t HitIndex = 0;
        float HitDistance = 1000.0f; // 최대 거리 설정

        // 템플릿 처리된 Raycast 호출!
        if (SceneManager->GetGrid()->Raycast(WorldRay, 1000.0f, HitIndex, HitDistance, PreciseTriangleTest))
        {
            SceneManager->SelectObject(HitIndex);
        }
        else
        {
            SceneManager->ClearSelection();
        }

        bPendingPick = false;
    }
}

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
                << L" | FrustumVisible: " << VisibleObjects
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
    case WM_LBUTTONDOWN:
        if (AppInstance != nullptr)
        {
            AppInstance->bPendingPick = true;
            AppInstance->PickPosition.x = GET_X_LPARAM(lParam);
            AppInstance->PickPosition.y = GET_Y_LPARAM(lParam);
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
