#pragma once
#include <string>
#include <filesystem>

namespace ExtremeCore
{
    namespace fs = std::filesystem;

    class PathManager
    {
    public:
        // 앱이 실행되는 위치를 기준으로 Data 폴더 경로를 찾습니다.
        static std::wstring GetDataPath()
        {
            // 실제 배포 시에는 executable 경로 기준, 
            // 개발 시에는 프로젝트 루트 기준 (보통 ../Data)
            return L"Data/"; 
        }

        static std::wstring GetMeshPath()
        {
            return GetDataPath() + L"JungleApples/";
        }

        static std::wstring GetScenePath()
        {
            return GetDataPath() + L"DefaultScene/";
        }

        static std::wstring GetBinPath()
        {
            return GetDataPath() + L"Exported/";
        }
    };
}