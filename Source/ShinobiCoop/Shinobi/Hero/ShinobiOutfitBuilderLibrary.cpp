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
#include "ClothConfigBase.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsAssetUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshClothingSystemUtilities.h"
#endif

#if WITH_EDITOR
namespace
{
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
		Config->BendingStiffnessWeighted = {0.8f, 0.8f}; // softer, and an idle hem crumples into strips
		Config->DampingCoefficient = 0.1f;
		Config->Drag = {0.07f, 0.07f};
		Config->Lift = {0.07f, 0.07f};
		Config->CollisionThickness = 1.f;
		Config->FrictionCoefficient = 0.8f;
		Config->bUseSelfCollisions = false;
		Config->LinearVelocityScale = FVector(0.6f);
		Config->AngularVelocityScale = 0.6f;
		// A light pull back to the fitted (skinned) shape: an idle hem keeps its flare, and still swings and trails in motion.
		Config->AnimDriveStiffness = {0.15f, 0.15f};
		Config->AnimDriveDamping = {0.2f, 0.2f};
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
	if (!bPainted)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildCloth: %s has no cloth regions in its vertex colours (skin_outfit.py paints them; import with vertex colours)"), *Mesh->GetName());
		return false;
	}
	TMap<FIntVector, int32> RenderAt;
	for (int32 Index = 0; Index < Render.Num(); ++Index)
	{
		RenderAt.Add(PositionKey(Render[Index].Position), Index);
	}
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
		if (const int32* At = RenderAt.Find(PositionKey(Physical[Index])))
		{
			Hem = Render[*At].Color.R / 255.f;
			Sleeve = Render[*At].Color.G / 255.f;
		}
		else
		{
			++Unmatched;
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
		TEXT("painted"), Collision ? *Collision->GetName() : TEXT("none"));
	return true;
#else
	return false;
#endif
}

bool UShinobiOutfitBuilderLibrary::CopyClothTuning(USkeletalMesh* Source, const TArray<USkeletalMesh*>& Targets)
{
#if WITH_EDITOR
	struct FCopy
	{
		UClothingAssetCommon* Cloth = nullptr;
		TArray<FVector3f> Master; // per physical vertex: where it sits in the master garment (m)
	};
	auto Read = [](USkeletalMesh* Mesh, FCopy& Out) -> bool
	{
		FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
		Out.Cloth = Mesh && Mesh->GetMeshClothingAssets().Num() ? Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]) : nullptr;
		if (!Model || !Model->LODModels.Num() || !Model->LODModels[0].Sections.Num() || !Out.Cloth || !Out.Cloth->LodData.Num())
		{
			UE_LOG(LogShinobiCoop, Warning, TEXT("CopyClothTuning: %s has no cloth on LOD 0 section 0"), Mesh ? *Mesh->GetName() : TEXT("(null)"));
			return false;
		}
		const TArray<FSoftSkinVertex>& Render = Model->LODModels[0].Sections[0].SoftVertices;
		TMap<FIntVector, int32> RenderAt;
		for (int32 Index = 0; Index < Render.Num(); ++Index)
		{
			RenderAt.Add(PositionKey(Render[Index].Position), Index);
		}
		for (const FVector3f& Position : Out.Cloth->LodData[0].PhysicalMeshData.Vertices)
		{
			const int32* At = RenderAt.Find(PositionKey(Position));
			Out.Master.Add(At ? FVector3f(Render[*At].UVs[1].X, Render[*At].UVs[1].Y, Render[*At].UVs[2].X) : FVector3f(1e6f));
		}
		return true;
	};
	FCopy From;
	if (!Read(Source, From))
	{
		return false;
	}
	// Source vertices bucketed by master position (1 cm cells) for the nearest-vertex lookups.
	constexpr float Cell = 0.01f;
	auto CellOf = [](const FVector3f& P) { return FIntVector(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell), FMath::FloorToInt(P.Z / Cell)); };
	TMultiMap<FIntVector, int32> Buckets;
	for (int32 Index = 0; Index < From.Master.Num(); ++Index)
	{
		Buckets.Add(CellOf(From.Master[Index]), Index);
	}
	auto Nearest = [&](const FVector3f& P) -> int32
	{
		const FIntVector Home = CellOf(P);
		for (int32 Reach = 0; Reach <= 8; ++Reach) // out to 8 cm, a ring of cells at a time
		{
			int32 Best = INDEX_NONE;
			float BestDist = TNumericLimits<float>::Max();
			for (int32 X = -Reach; X <= Reach; ++X)
			for (int32 Y = -Reach; Y <= Reach; ++Y)
			for (int32 Z = -Reach; Z <= Reach; ++Z)
			{
				if (FMath::Max3(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z)) != Reach)
				{
					continue;
				}
				TArray<int32, TInlineAllocator<8>> Found;
				Buckets.MultiFind(Home + FIntVector(X, Y, Z), Found);
				for (const int32 Index : Found)
				{
					const float Dist = FVector3f::DistSquared(From.Master[Index], P);
					if (Dist < BestDist)
					{
						BestDist = Dist;
						Best = Index;
					}
				}
			}
			if (Best != INDEX_NONE)
			{
				return Best;
			}
		}
		return INDEX_NONE;
	};
	bool bAll = true;
	for (USkeletalMesh* Target : Targets)
	{
		FCopy To;
		if (Target == Source || !Read(Target, To))
		{
			bAll &= Target == Source;
			continue;
		}
		To.Cloth->ClothConfigs.Reset();
		for (const TPair<FName, TObjectPtr<UClothConfigBase>>& Config : From.Cloth->ClothConfigs)
		{
			To.Cloth->ClothConfigs.Add(Config.Key, DuplicateObject<UClothConfigBase>(Config.Value, To.Cloth));
		}
		TArray<int32> Match;
		int32 Missed = 0;
		for (const FVector3f& P : To.Master)
		{
			Match.Add(Nearest(P));
			Missed += Match.Last() == INDEX_NONE;
		}
		FClothLODDataCommon& Lod = To.Cloth->LodData[0];
		for (const FPointWeightMap& Map : From.Cloth->LodData[0].PointWeightMaps)
		{
			if (!Map.bEnabled || Map.CurrentTarget == uint8(EWeightMapTargetCommon::None))
			{
				continue;
			}
			FPointWeightMap* Into = Lod.PointWeightMaps.FindByPredicate([&Map](const FPointWeightMap& Other) { return Other.CurrentTarget == Map.CurrentTarget; });
			if (!Into)
			{
				Into = &Lod.PointWeightMaps.AddDefaulted_GetRef();
			}
			Into->Initialize(Match.Num());
			Into->Name = Map.Name;
			Into->CurrentTarget = Map.CurrentTarget;
			Into->bEnabled = true;
			for (int32 Index = 0; Index < Match.Num(); ++Index)
			{
				Into->Values[Index] = Match[Index] != INDEX_NONE ? Map.Values[Match[Index]] : 0.f;
			}
		}
		To.Cloth->ApplyParameterMasks();
		Target->PostEditChange();
		Target->MarkPackageDirty();
		UE_LOG(LogShinobiCoop, Log, TEXT("CopyClothTuning: %s -> %s (%d of %d cloth vertices without a match: pinned)"),
			*Source->GetName(), *Target->GetName(), Missed, Match.Num());
	}
	return bAll;
#else
	return false;
#endif
}

bool UShinobiOutfitBuilderLibrary::AddTailCapsule(USkeletalMesh* Body, UPhysicsAsset* Physics, FVector RootFromHips, FVector TipFromHips, float Radius)
{
#if WITH_EDITOR
	if (!Body || !Physics)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("AddTailCapsule: needs a body and its cloth collision"));
		return false;
	}
	const FReferenceSkeleton& Ref = Body->GetRefSkeleton();
	int32 Hips = INDEX_NONE;
	for (int32 Bone = 0; Bone < Ref.GetNum() && Hips == INDEX_NONE; ++Bone)
	{
		Hips = Ref.GetBoneName(Bone).ToString().EndsWith(TEXT("Hips")) ? Bone : INDEX_NONE;
	}
	const int32 BodyIndex = Hips != INDEX_NONE ? Physics->FindBodyIndex(Ref.GetBoneName(Hips)) : INDEX_NONE;
	if (BodyIndex == INDEX_NONE)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("AddTailCapsule: %s has no Hips body"), *Physics->GetName());
		return false;
	}
	// Blender (Z up, facing -Y, m) to UE component space (Z up, facing +Y, cm), from the Hips joint; then into its bone space.
	const FTransform HipsPose = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Hips);
	auto ToComponent = [&HipsPose](const FVector& FromHips) { return HipsPose.GetLocation() + FVector(FromHips.X, -FromHips.Y, FromHips.Z) * 100.0; };
	const FVector Root = HipsPose.InverseTransformPosition(ToComponent(RootFromHips));
	const FVector Tip = HipsPose.InverseTransformPosition(ToComponent(TipFromHips));
	FKAggregateGeom& Geometry = Physics->SkeletalBodySetups[BodyIndex]->AggGeom;
	Geometry.SphylElems.RemoveAll([](const FKSphylElem& Elem) { return Elem.GetName() == FName(TEXT("Tail")); });
	FKSphylElem Capsule;
	Capsule.SetName(TEXT("Tail"));
	Capsule.Center = (Root + Tip) / 2;
	Capsule.Rotation = FRotationMatrix::MakeFromZ(Tip - Root).Rotator(); // a capsule's axis is its Z
	// (in the Hips body's own units: a Mixamo body imported with its rig's scale has the bone scaled, its ragdoll shapes too)
	const float BoneScale = FMath::Max(HipsPose.GetScale3D().GetAbsMax(), UE_KINDA_SMALL_NUMBER);
	Capsule.Radius = Radius * 100.f / BoneScale;
	Capsule.Length = (Tip - Root).Size();
	Geometry.SphylElems.Add(Capsule);
	Physics->SkeletalBodySetups[BodyIndex]->InvalidatePhysicsData();
	Physics->SkeletalBodySetups[BodyIndex]->CreatePhysicsMeshes();
	Physics->MarkPackageDirty();
	UE_LOG(LogShinobiCoop, Log, TEXT("AddTailCapsule: %s: %.0f cm long, %.0f cm radius (bone scale %.3f)"), *Physics->GetName(),
		Capsule.Length * BoneScale, Capsule.Radius * BoneScale, BoneScale);
	return true;
#else
	return false;
#endif
}

UPhysicsAsset* UShinobiOutfitBuilderLibrary::BuildClothCollision(USkeletalMesh* Body, const FString& AssetPath)
{
#if WITH_EDITOR
	if (!Body || Body->GetRefSkeleton().GetNum() == 0)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildClothCollision: no body"));
		return nullptr;
	}
	const FString Name = FPackageName::GetShortName(AssetPath);
	// Load the saved one first: a new object in an unloaded package is overwritten by the old file when anything loads it
	// later, so every rebuild was lost.
	UPhysicsAsset* Physics = LoadObject<UPhysicsAsset>(nullptr, *(AssetPath + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Physics)
	{
		Physics = NewObject<UPhysicsAsset>(CreatePackage(*AssetPath), *Name, RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Physics);
	}
	Physics->SkeletalBodySetups.Reset();
	Physics->ConstraintSetup.Reset();
	Physics->UpdateBodySetupIndexMap(); // (stale maps of the loaded bodies crash CreateFromSkeletalMesh)
	Physics->UpdateBoundsBodiesArray();
	// The root bone's component-space scale is the rig's unit (100 on these Mixamo imports): sizes below are in bone units.
	const float BoneScale = FMath::Max(FAnimationRuntime::GetComponentSpaceTransformRefPose(Body->GetRefSkeleton(), 0).GetScale3D().GetAbsMax(), UE_KINDA_SMALL_NUMBER);
	FPhysAssetCreateParams Params;
	Params.MinBoneSize = 8.f / BoneScale; // bones shorter than 8 cm (fingers, toes) get no capsule of their own
	Params.GeomType = EFG_Sphyl;
	Params.VertWeight = EVW_DominantWeight;
	Params.bCreateConstraints = false; // collision only: nothing simulates it
	FText Error;
	if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Physics, Body, Params, Error, /*bSetToMesh=*/false, /*bShowProgress=*/false))
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("BuildClothCollision: %s: %s"), *Body->GetName(), *Error.ToString());
		return nullptr;
	}
	// A capsule centred beyond its own bone wraps the next limb's skin: a MetaHuman's arm bones carry no weights, so the
	// clavicle's takes the bind pose's upper arm, stays out there when the arm drops and pushes the sleeve into spikes.
	const FReferenceSkeleton& Ref = Body->GetRefSkeleton();
	int32 Dropped = 0;
	for (int32 Index = Physics->SkeletalBodySetups.Num() - 1; Index >= 0; --Index)
	{
		USkeletalBodySetup* Setup = Physics->SkeletalBodySetups[Index];
		const int32 Bone = Ref.FindBoneIndex(Setup->BoneName);
		float Reach = 0.f; // to its farthest child joint, in bone units
		for (int32 Child = Bone + 1; Child < Ref.GetNum(); ++Child)
		{
			if (Ref.GetParentIndex(Child) == Bone)
			{
				Reach = FMath::Max(Reach, float(Ref.GetRefBonePose()[Child].GetLocation().Size()));
			}
		}
		const FKSphylElem* Capsule = Setup->AggGeom.SphylElems.Num() == 1 ? &Setup->AggGeom.SphylElems[0] : nullptr;
		if (Reach > 0.f && Capsule && Capsule->Center.Size() > Reach + Capsule->Radius) // (it doesn't even touch its bone)
		{
			UE_LOG(LogShinobiCoop, Log, TEXT("BuildClothCollision: %s: no capsule on %s (centred %.0f out, its bone reaches %.0f)"), *Physics->GetName(),
				*Setup->BoneName.ToString(), Capsule->Center.Size(), Reach);
			Physics->SkeletalBodySetups.RemoveAt(Index);
			Setup->MarkAsGarbage();
			++Dropped;
		}
	}
	Physics->UpdateBodySetupIndexMap();
	Physics->UpdateBoundsBodiesArray();
	// A capsule fits its skin's bounds, so it stands out of a flat section (the hips, the belly) and blew the fitted coat
	// out into a ball: shrink each to the SkinInsideShare quantile of its skin's distances from its axis, inside the body. (Cloth
	// sinking that little into skin the coat hides anyway doesn't show.)
	constexpr float SkinInsideShare = 0.25f;
	TArray<FTransform> RefComponent;
	FAnimationRuntime::FillUpComponentSpaceTransforms(Ref, Ref.GetRefBonePose(), RefComponent);
	TMap<int32, TArray<FVector>> SkinByBone; // render vertices by their strongest bone
	for (const FSkelMeshSection& Section : Body->GetImportedModel()->LODModels[0].Sections)
	{
		for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
		{
			int32 Top = 0;
			for (int32 Influence = 1; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
			{
				Top = Vertex.InfluenceWeights[Influence] > Vertex.InfluenceWeights[Top] ? Influence : Top;
			}
			SkinByBone.FindOrAdd(Section.BoneMap[Vertex.InfluenceBones[Top]]).Add(FVector(Vertex.Position));
		}
	}
	int32 Shrunk = 0;
	for (USkeletalBodySetup* Setup : Physics->SkeletalBodySetups)
	{
		const int32 Bone = Ref.FindBoneIndex(Setup->BoneName);
		const TArray<FVector>* Skin = SkinByBone.Find(Bone);
		if (Setup->AggGeom.SphylElems.Num() != 1 || !Skin || Skin->Num() < 20)
		{
			continue;
		}
		FKSphylElem& Capsule = Setup->AggGeom.SphylElems[0];
		const FTransform World = FTransform(Capsule.Rotation, Capsule.Center) * RefComponent[Bone];
		const FVector A = World.TransformPosition(FVector(0, 0, -Capsule.Length / 2)), B = World.TransformPosition(FVector(0, 0, Capsule.Length / 2));
		TArray<float> Distances;
		for (const FVector& Point : *Skin)
		{
			Distances.Add(FMath::PointDistToSegment(Point, A, B));
		}
		Distances.Sort();
		const float Inside = Distances[int32(Distances.Num() * SkinInsideShare)] / RefComponent[Bone].GetMaximumAxisScale();
		if (Inside < Capsule.Radius)
		{
			Capsule.Radius = FMath::Max(Inside, 0.5f);
			++Shrunk;
		}
	}
	Physics->SetPreviewMesh(Body);
	Physics->MarkPackageDirty();
	UE_LOG(LogShinobiCoop, Log, TEXT("BuildClothCollision: %s: %d bodies, %d dropped, %d shrunk inside the skin (bone scale %.0f)"), *Physics->GetName(),
		Physics->SkeletalBodySetups.Num(), Dropped, Shrunk, BoneScale);
	return Physics;
#else
	return nullptr;
#endif
}
