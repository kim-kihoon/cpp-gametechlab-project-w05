#include <Scene/SceneSerializer.h>
#include <Core/PathManager.h>
#include <Math/MathTypes.h>
#include <Scene/SceneData.h>
#include <Scene/SceneManager.h>
#include <filesystem>
#include <fstream>

namespace Scene
{
    std::wstring FSceneSerializer::GetDefaultBinaryScenePath()
    {
        return (std::filesystem::path(ExtremeCore::FPathManager::GetBinPath()) / L"SceneMatrices.bin").wstring();
    }

    bool FSceneSerializer::SaveWorldMatrices(const USceneManager& InSceneManager, const std::wstring& InOutputPath)
    {
        const FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        if (!SceneData)
        {
            return false;
        }

        const std::filesystem::path OutputPath = InOutputPath.empty()
            ? std::filesystem::path(GetDefaultBinaryScenePath())
            : std::filesystem::path(InOutputPath);

        if (!OutputPath.parent_path().empty())
        {
            std::filesystem::create_directories(OutputPath.parent_path());
        }

        std::ofstream OutputStream(OutputPath, std::ios::binary | std::ios::trunc);
        if (!OutputStream.is_open())
        {
            return false;
        }

        FSceneBinaryHeader Header = {};
        Header.MatrixCount = InSceneManager.GetObjectCount();

        OutputStream.write(reinterpret_cast<const char*>(&Header), sizeof(Header));
        OutputStream.write(
            reinterpret_cast<const char*>(SceneData->WorldMatrices.data()),
            sizeof(Math::FPacked3x4Matrix) * Header.MatrixCount);

        return OutputStream.good();
    }

    bool FSceneSerializer::LoadWorldMatrices(USceneManager& InSceneManager, const std::wstring& InInputPath)
    {
        const std::filesystem::path InputPath = InInputPath.empty()
            ? std::filesystem::path(GetDefaultBinaryScenePath())
            : std::filesystem::path(InInputPath);

        std::ifstream InputStream(InputPath, std::ios::binary);
        if (!InputStream.is_open())
        {
            return false;
        }

        FSceneBinaryHeader Header = {};
        InputStream.read(reinterpret_cast<char*>(&Header), sizeof(Header));
        if (!InputStream.good() ||
            Header.Magic != FSceneBinaryHeader::MAGIC ||
            Header.Version != FSceneBinaryHeader::VERSION ||
            Header.MatrixCount > FSceneDataSOA::MAX_OBJECTS)
        {
            return false;
        }

        if (!InSceneManager.EnsureObjectCount(Header.MatrixCount))
        {
            return false;
        }

        FSceneDataSOA* SceneData = InSceneManager.GetSceneData();
        if (!SceneData)
        {
            return false;
        }

        InputStream.read(
            reinterpret_cast<char*>(SceneData->WorldMatrices.data()),
            sizeof(Math::FPacked3x4Matrix) * Header.MatrixCount);

        return InputStream.good();
    }
}
