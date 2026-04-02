#include <Core/App.h>

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // 1. 앱 인스턴스 생성
    std::unique_ptr<App> pApp = std::make_unique<App>();

    // 2. 초기화 (윈도우 생성, DX11, ImGui 등)
    if (!pApp->Initialize(hInstance, nCmdShow))
    {
        MessageBox(nullptr, L"Failed to Initialize App!", L"Error", MB_OK);
        return -1;
    }

    // 3. 메인 루프 실행
    return pApp->Run();
}
