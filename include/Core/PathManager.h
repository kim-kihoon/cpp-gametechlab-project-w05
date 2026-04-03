#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <windows.h>

namespace ExtremeCore
{
    namespace fs = std::filesystem;

    class PathManager
    {
    public:
        static std::wstring GetDataPath()
        {
            return ToDirectoryString(ResolveDataRoot());
        }

        static std::wstring GetMeshPath()
        {
            return ToDirectoryString(ResolveDataRoot() / L"JungleApples");
        }

        static std::wstring GetScenePath()
        {
            return ToDirectoryString(ResolveDataRoot() / L"DefaultScene");
        }

        static std::wstring GetBinPath()
        {
            return ToDirectoryString(ResolveDataRoot() / L"Exported");
        }

    private:
        static fs::path ResolveDataRoot()
        {
            std::array<fs::path, 2> SeedDirectories = { fs::current_path(), GetModuleDirectory() };

            for (const fs::path& SeedDirectory : SeedDirectories)
            {
                for (fs::path Candidate = SeedDirectory; !Candidate.empty(); Candidate = Candidate.parent_path())
                {
                    const fs::path DataDirectory = Candidate / L"Data";
                    if (fs::exists(DataDirectory) && fs::is_directory(DataDirectory))
                    {
                        return DataDirectory.lexically_normal();
                    }

                    if (Candidate == Candidate.root_path())
                    {
                        break;
                    }
                }
            }

            return (fs::current_path() / L"Data").lexically_normal();
        }

        static fs::path GetModuleDirectory()
        {
            wchar_t ModulePathBuffer[MAX_PATH] = {};
            const DWORD PathLength = ::GetModuleFileNameW(nullptr, ModulePathBuffer, MAX_PATH);
            if (PathLength == 0)
            {
                return fs::current_path();
            }

            return fs::path(ModulePathBuffer).parent_path();
        }

        static std::wstring ToDirectoryString(const fs::path& InPath)
        {
            std::wstring Result = InPath.lexically_normal().wstring();
            if (!Result.empty() && Result.back() != L'\\' && Result.back() != L'/')
            {
                Result.push_back(fs::path::preferred_separator);
            }

            return Result;
        }
    };

    using FPathManager = PathManager;
}
