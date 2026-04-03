#include <Core/App.h>

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    std::unique_ptr<UApp> AppInstance = std::make_unique<UApp>();

    if (!AppInstance->Initialize(hInstance, nCmdShow))
    {
        return -1;
    }

    return AppInstance->Run();
}
