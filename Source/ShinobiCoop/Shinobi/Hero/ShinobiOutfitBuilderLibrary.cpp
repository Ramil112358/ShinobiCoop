#include "Shinobi/Hero/ShinobiOutfitBuilderLibrary.h"

#include "ShinobiCoop.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"

#if WITH_EDITOR
#include "AnimationRuntime.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Modules/ModuleManager.h"
#include "PointWeightMap.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshClothingSystemUtilities.h"
#endif

#if WITH_EDITOR
namespace
{
	/** Height (cm, component space) of the Hips bone in the reference pose; 0 without one. */
	float RefHipsHeight(const USkeletalMesh* Mesh)
	{
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			if (Ref.GetBoneName(Bone).ToString().EndsWith(TEXT("Hips")))
			{
				return FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation().Z;
			}
		}
		return 0.f;
	}

	/** Physical (simulation) vertices sit exactly on render vertices: key both by position to the 0.01 cm. */
	FIntVector PositionKey(const FVector3f& Position)
	{
		return FIntVector(FMath::RoundToInt(Position.X * 100.f), FMath::RoundToInt(Position.Y * 100.f), FMath::RoundToInt(Position.Z * 100.f));
	}

	void SetDefaultConfig(UClothingAssetCommon* Cloth)
	{
		UChaosClothConfig* Config = Cloth->GetClothConfig<UChaosClothConfig>();
		if (!Config)
		{
			return;
		}
		Config->Density = 0.35f;
		Config->EdgeStiffnessWeighted = {1.f, 1.f};
		Config->BendingStiffnessWeighted = {0.5f, 0.5f};
		Config->DampingCoefficient = 0.1f;
		Config->Drag = {0.07f, 0.07f};
		Config->Lift = {0.07f, 0.07f};
		Config->CollisionThickness = 1.f;
		Config->FrictionCoefficient = 0.8f;
		Config->bUseSelfCollisions = false;
		Config->LinearVelocityScale = FVector(0.6f);
		Config->AngularVelocityScale = 0.6f;
	}
}
#endif

bool UShinobiOutfitBuilderLibrary::BuildCloth(USkeletalMesh* Mesh, UPhysicsAsset* Collision, float HemMaxDistanceCm, float SleeveMaxDistanceCm)
{
#if WITH_EDITOR
	FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
	if (!Model || Model->LODModels.Num() == 0 || Model->LODModels[0].Sections.Num() == 0)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildCloth: %s has no LOD 0 section 0"), Mesh ? *Mesh->GetName() : TEXT("(null)"));
		return false;
	}
	// Start over: unbind and drop any clothing from an earlier build.
	FSkeletalMeshClothingSystemUtilities::RemoveClothingFromSection(Mesh, 0, 0, nullptr);
	Mesh->GetMeshClothingAssets().Reset();

	UClothingAssetFactoryBase* Factory = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>("ClothingSystemEditorInterface").GetClothingAssetFactory();
	FSkeletalMeshClothBuildParams Params;
	Params.AssetName = Mesh->GetName() + TEXT("_Cloth");
	Params.LodIndex = 0;
	Params.SourceSection = 0;
	Params.PhysicsAsset = Collision;
	UClothingAssetCommon* Cloth = Cast<UClothingAssetCommon>(Factory ? Factory->CreateFromSkeletalMesh(Mesh, Params) : nullptr);
	if (!Cloth || Cloth->LodData.Num() == 0)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildCloth: could not make clothing from %s"), *Mesh->GetName());
		return false;
	}
	Mesh->AddClothingAsset(Cloth);
	SetDefaultConfig(Cloth);

	// Max Distance per physical vertex, from the render vertex at the same spot.
	const TArray<FSoftSkinVertex>& Render = Model->LODModels[0].Sections[0].SoftVertices;
	const bool bPainted = Render.ContainsByPredicate([](const FSoftSkinVertex& Vertex) { return Vertex.Color.B != 255; });
	TMap<FIntVector, int32> RenderAt;
	for (int32 Index = 0; Index < Render.Num(); ++Index)
	{
		RenderAt.Add(PositionKey(Render[Index].Position), Index);
	}
	const float HipsZ = RefHipsHeight(Mesh);
	FClothLODDataCommon& Lod = Cloth->LodData[0];
	const TArray<FVector3f>& Physical = Lod.PhysicalMeshData.Vertices;
	FPointWeightMap* MaxDistance = Lod.PointWeightMaps.FindByPredicate([](const FPointWeightMap& Map) { return Map.CurrentTarget == uint8(EWeightMapTargetCommon::MaxDistance); });
	if (!MaxDistance)
	{
		MaxDistance = &Lod.PointWeightMaps.AddDefaulted_GetRef();
	}
	MaxDistance->Initialize(Physical.Num());
	MaxDistance->Name = TEXT("MaxDistance");
	MaxDistance->CurrentTarget = uint8(EWeightMapTargetCommon::MaxDistance);
	MaxDistance->bEnabled = true;
	int32 Unmatched = 0;
	for (int32 Index = 0; Index < Physical.Num(); ++Index)
	{
		float Hem = 0.f, Sleeve = 0.f;
		if (bPainted)
		{
			if (const int32* At = RenderAt.Find(PositionKey(Physical[Index])))
			{
				Hem = Render[*At].Color.R / 255.f;
				Sleeve = Render[*At].Color.G / 255.f;
			}
			else
			{
				++Unmatched;
			}
		}
		else
		{
			Hem = FMath::Clamp((HipsZ - Physical[Index].Z) / 30.f, 0.f, 1.f);
		}
		MaxDistance->Values[Index] = FMath::Max(Hem * HemMaxDistanceCm, Sleeve * SleeveMaxDistanceCm);
	}
	if (Unmatched > 0)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildCloth: %s: %d of %d cloth vertices found no render vertex (pinned)"), *Mesh->GetName(), Unmatched, Physical.Num());
	}
	Cloth->ApplyParameterMasks();

	FString Error;
	if (!FSkeletalMeshClothingSystemUtilities::AssignClothingToSection(Mesh, Cloth, 0, 0, 0, &Error))
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildCloth: could not bind the cloth to %s: %s"), *Mesh->GetName(), *Error);
		return false;
	}
	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	UE_LOG(LogShinobiCoop, Log, TEXT("BuildCloth: %s: %d cloth vertices (%s), collision %s"), *Mesh->GetName(), Physical.Num(),
		bPainted ? TEXT("painted") : TEXT("stand-in hem"), Collision ? *Collision->GetName() : TEXT("none"));
	return true;
#else
	return false;
#endif
}
