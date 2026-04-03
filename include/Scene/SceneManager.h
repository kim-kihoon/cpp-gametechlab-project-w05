#pragma once
#include <Math/MathTypes.h>
#include <Scene/SceneData.h>
#include <Scene/UniformGrid.h>
#include <memory>

namespace Scene
{
    /**
     * 전체 씬의 오브젝트와 그리드를 관리하는 매니저 클래스.
     */
    class USceneManager
    {
    public:
        USceneManager();
        ~USceneManager();

        void Initialize();
        void Update(float DeltaTime);

        FSceneDataSOA* GetSceneData() { return SceneData.get(); }
        const FSceneDataSOA* GetSceneData() const { return SceneData.get(); }
        UUniformGrid* GetGrid() { return Grid.get(); }

        uint32_t GetObjectCount() const { return ObjectCount; }
        static constexpr uint32_t GetMaxObjectCount() { return FSceneDataSOA::MAX_OBJECTS; }

        bool IsValidIndex(uint32_t InIndex) const;
        void ResetScene();
        bool EnsureObjectCount(uint32_t InObjectCount);

        bool AddObject(const Math::FBox& InBounds, const Math::FMatrix& InWorldMatrix, uint32_t InMeshID = 0, uint32_t InMaterialID = 0);
        bool AddObjectPacked(const Math::FBox& InBounds, const Math::FPacked3x4Matrix& InWorldMatrix, uint32_t InMeshID = 0, uint32_t InMaterialID = 0);

        bool GetBounds(uint32_t InIndex, Math::FBox& OutBounds) const;
        bool SetBounds(uint32_t InIndex, const Math::FBox& InBounds);

        bool GetWorldMatrixPacked(uint32_t InIndex, Math::FPacked3x4Matrix& OutWorldMatrix) const;
        bool SetWorldMatrix(uint32_t InIndex, const Math::FMatrix& InWorldMatrix);
        bool SetWorldMatrixPacked(uint32_t InIndex, const Math::FPacked3x4Matrix& InWorldMatrix);

    private:
        std::unique_ptr<FSceneDataSOA> SceneData;
        std::unique_ptr<UUniformGrid> Grid;
        uint32_t ObjectCount = 0;
    };
}
