#include <Scene/SceneManager.h>

namespace Scene
{
    namespace
    {
        Math::FBox MakeDefaultBounds()
        {
            return Math::FBox{ { -0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f } };
        }

        Math::FPacked3x4Matrix MakeIdentityWorldMatrix()
        {
            Math::FPacked3x4Matrix PackedMatrix{};
            PackedMatrix.Store(DirectX::XMMatrixIdentity());
            return PackedMatrix;
        }

        void WriteBounds(FSceneDataSOA& InSceneData, uint32_t InIndex, const Math::FBox& InBounds)
        {
            InSceneData.MinX[InIndex] = InBounds.Min.x;
            InSceneData.MinY[InIndex] = InBounds.Min.y;
            InSceneData.MinZ[InIndex] = InBounds.Min.z;
            InSceneData.MaxX[InIndex] = InBounds.Max.x;
            InSceneData.MaxY[InIndex] = InBounds.Max.y;
            InSceneData.MaxZ[InIndex] = InBounds.Max.z;
        }

        Math::FBox ReadBounds(const FSceneDataSOA& InSceneData, uint32_t InIndex)
        {
            return Math::FBox
            {
                { InSceneData.MinX[InIndex], InSceneData.MinY[InIndex], InSceneData.MinZ[InIndex] },
                { InSceneData.MaxX[InIndex], InSceneData.MaxY[InIndex], InSceneData.MaxZ[InIndex] }
            };
        }
    }

    USceneManager::USceneManager() {}
    USceneManager::~USceneManager() {}

    void USceneManager::Initialize()
    {
        SceneData = std::make_unique<FSceneDataSOA>();
        ObjectCount = 0;
    }

    void USceneManager::Update(float DeltaTime)
    {
    }

    bool USceneManager::IsValidIndex(uint32_t InIndex) const
    {
        return SceneData != nullptr && InIndex < ObjectCount;
    }

    void USceneManager::ResetScene()
    {
        if (!SceneData)
        {
            return;
        }

        for (uint32_t Index = 0; Index < ObjectCount; ++Index)
        {
            SceneData->MeshIDs[Index] = 0;
            SceneData->MaterialIDs[Index] = 0;
            SceneData->IsVisible[Index] = false;
            SceneData->WorldMatrices[Index] = MakeIdentityWorldMatrix();
            WriteBounds(*SceneData, Index, MakeDefaultBounds());
        }

        ObjectCount = 0;
        SceneData->ResetRenderQueue();
    }

    bool USceneManager::EnsureObjectCount(uint32_t InObjectCount)
    {
        if (InObjectCount > FSceneDataSOA::MAX_OBJECTS)
        {
            return false;
        }

        if (!SceneData)
        {
            Initialize();
        }

        const Math::FBox DefaultBounds = MakeDefaultBounds();
        const Math::FPacked3x4Matrix DefaultMatrix = MakeIdentityWorldMatrix();

        if (InObjectCount > ObjectCount)
        {
            for (uint32_t Index = ObjectCount; Index < InObjectCount; ++Index)
            {
                SceneData->MeshIDs[Index] = 0;
                SceneData->MaterialIDs[Index] = 0;
                SceneData->IsVisible[Index] = true;
                SceneData->WorldMatrices[Index] = DefaultMatrix;
                WriteBounds(*SceneData, Index, DefaultBounds);
            }
        }
        else
        {
            for (uint32_t Index = InObjectCount; Index < ObjectCount; ++Index)
            {
                SceneData->IsVisible[Index] = false;
            }
        }

        ObjectCount = InObjectCount;
        return true;
    }

    bool USceneManager::AddObject(const Math::FBox& InBounds, const Math::FMatrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        Math::FPacked3x4Matrix PackedWorldMatrix{};
        PackedWorldMatrix.Store(InWorldMatrix);
        return AddObjectPacked(InBounds, PackedWorldMatrix, InMeshID, InMaterialID);
    }

    bool USceneManager::AddObjectPacked(const Math::FBox& InBounds, const Math::FPacked3x4Matrix& InWorldMatrix, uint32_t InMeshID, uint32_t InMaterialID)
    {
        if (!EnsureObjectCount(ObjectCount + 1))
        {
            return false;
        }

        const uint32_t NewIndex = ObjectCount - 1;
        WriteBounds(*SceneData, NewIndex, InBounds);
        SceneData->WorldMatrices[NewIndex] = InWorldMatrix;
        SceneData->MeshIDs[NewIndex] = InMeshID;
        SceneData->MaterialIDs[NewIndex] = InMaterialID;
        SceneData->IsVisible[NewIndex] = true;
        return true;
    }

    bool USceneManager::GetBounds(uint32_t InIndex, Math::FBox& OutBounds) const
    {
        if (!IsValidIndex(InIndex))
        {
            return false;
        }

        OutBounds = ReadBounds(*SceneData, InIndex);
        return true;
    }

    bool USceneManager::SetBounds(uint32_t InIndex, const Math::FBox& InBounds)
    {
        if (!IsValidIndex(InIndex))
        {
            return false;
        }

        WriteBounds(*SceneData, InIndex, InBounds);
        return true;
    }

    bool USceneManager::GetWorldMatrixPacked(uint32_t InIndex, Math::FPacked3x4Matrix& OutWorldMatrix) const
    {
        if (!IsValidIndex(InIndex))
        {
            return false;
        }

        OutWorldMatrix = SceneData->WorldMatrices[InIndex];
        return true;
    }

    bool USceneManager::SetWorldMatrix(uint32_t InIndex, const Math::FMatrix& InWorldMatrix)
    {
        Math::FPacked3x4Matrix PackedWorldMatrix{};
        PackedWorldMatrix.Store(InWorldMatrix);
        return SetWorldMatrixPacked(InIndex, PackedWorldMatrix);
    }

    bool USceneManager::SetWorldMatrixPacked(uint32_t InIndex, const Math::FPacked3x4Matrix& InWorldMatrix)
    {
        if (!IsValidIndex(InIndex))
        {
            return false;
        }

        SceneData->WorldMatrices[InIndex] = InWorldMatrix;
        return true;
    }
}
