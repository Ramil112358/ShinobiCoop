#include "Shinobi/Hero/ShinobiMutableMeshLibrary.h"

#include "ShinobiCoop.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "AnimationRuntime.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#endif

int32 UShinobiMutableMeshLibrary::AddBoneSkinning(USkeletalMesh* Mesh, FName Bone, int32 Count, float Weight)
{
	int32 Lod0Added = 0;
#if WITH_EDITOR
	const int32 BoneIndex = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
	if (BoneIndex == INDEX_NONE || Count <= 0)
	{
		return 0;
	}
	const FVector3f Target(FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), BoneIndex).GetLocation());
	for (int32 Lod = 0; Lod < Mesh->GetLODNum(); ++Lod)
	{
		FMeshDescription Description;
		if (!Mesh->CloneMeshDescription(Lod, Description))
		{
			continue;
		}
		FSkeletalMeshAttributes Attributes(Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Description.GetVertexPositions();
		FSkinWeightsVertexAttributesRef Skin = Attributes.GetVertexSkinWeights();
		TArray<FVertexID> Nearest;
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			Nearest.Add(Vertex);
		}
		Nearest.Sort([&](const FVertexID A, const FVertexID B) { return FVector3f::DistSquared(Positions[A], Target) < FVector3f::DistSquared(Positions[B], Target); });
		Nearest.SetNum(FMath::Min(Count, Nearest.Num()));
		for (const FVertexID Vertex : Nearest)
		{
			TArray<UE::AnimationCore::FBoneWeight> Weights;
			for (const UE::AnimationCore::FBoneWeight& Existing : Skin.Get(Vertex))
			{
				if (Existing.GetBoneIndex() != BoneIndex)
				{
					Weights.Emplace(Existing.GetBoneIndex(), Existing.GetWeight() * (1.f - Weight));
				}
			}
			Weights.Emplace(BoneIndex, Weight);
			Skin.Set(Vertex, Weights);
		}
		Lod0Added = Lod == 0 ? Nearest.Num() : Lod0Added;
		Mesh->CreateMeshDescription(Lod, MoveTemp(Description));
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bMarkPackageDirty = true;
		Mesh->CommitMeshDescription(Lod, Params);
	}
	Mesh->Build();
#endif
	return Lod0Added;
}
