#include "Shinobi/World/ShinobiWorldBuilderLibrary.h"
#include "Engine/World.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "LandscapeUtils.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#if WITH_EDITOR
#include "StaticMeshAttributes.h"
#include "MeshUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#endif
#include "ShinobiCoop.h"

#if WITH_EDITOR
namespace
{
	/** Loads a PNG into a square Size x Size buffer of the requested gray format; false (logged) on mismatch. */
	bool LoadGray(const FString& Path, ERawImageFormat::Type Format, int32& InOutSize, FImage& Out)
	{
		FImage Loaded;
		if (!FImageUtils::LoadImage(*Path, Loaded))
		{
			UE_LOG(LogShinobiCoop, Error, TEXT("World builder: can't read %s"), *Path);
			return false;
		}
		Loaded.CopyTo(Out, Format, EGammaSpace::Linear);
		if (Out.SizeX != Out.SizeY || (InOutSize != 0 && Out.SizeX != InOutSize))
		{
			UE_LOG(LogShinobiCoop, Error, TEXT("World builder: %s is %dx%d, expected square %d"), *Path, Out.SizeX, Out.SizeY, InOutSize);
			return false;
		}
		InOutSize = Out.SizeX;
		return true;
	}
}
#endif

ALandscape* UShinobiWorldBuilderLibrary::CreateLandscape(UWorld* World, const FString& HeightmapPng, UMaterialInterface* Material,
	const TMap<FName, FString>& WeightmapPngs, const FString& LayerInfoPath, int32 WorldPartitionGridSize)
{
#if WITH_EDITOR
	if (!World)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("World builder: no world"));
		return nullptr;
	}
	int32 Size = 0;
	FImage Height;
	if (!LoadGray(HeightmapPng, ERawImageFormat::G16, Size, Height))
	{
		return nullptr;
	}
	const int32 Quads = Size - 1;
	if (Quads % QuadsPerSection != 0)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("World builder: heightmap size %d is not N*%d+1"), Size, QuadsPerSection);
		return nullptr;
	}
	TArray<uint16> HeightData;
	HeightData.Append(reinterpret_cast<const uint16*>(Height.RawData.GetData()), Size * Size);
	uint16 MinH = MAX_uint16, MaxH = 0;
	for (const uint16 H : HeightData)
	{
		MinH = FMath::Min(MinH, H);
		MaxH = FMath::Max(MaxH, H);
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("World builder: heightmap %s %dx%d, format %d, values %u..%u, centre %u"), *HeightmapPng, Size, Size,
		static_cast<int32>(Height.Format), MinH, MaxH, HeightData[(Size / 2) * Size + Size / 2]);

	TArray<FLandscapeImportLayerInfo> Layers;
	for (const TPair<FName, FString>& Pair : WeightmapPngs)
	{
		FImage Weight;
		if (!LoadGray(Pair.Value, ERawImageFormat::G8, Size, Weight))
		{
			return nullptr;
		}
		FLandscapeImportLayerInfo& Layer = Layers.Emplace_GetRef(Pair.Key);
		Layer.LayerInfo = UE::Landscape::CreateTargetLayerInfo(Pair.Key, LayerInfoPath, FString::Printf(TEXT("LI_%s"), *Pair.Key.ToString()));
		Layer.LayerData = MoveTemp(Weight.RawData);
		if (!Layer.LayerInfo)
		{
			UE_LOG(LogShinobiCoop, Error, TEXT("World builder: can't create layer info for %s in %s"), *Pair.Key.ToString(), *LayerInfoPath);
			return nullptr;
		}
	}

	const double Scale = QuadMeters * 100.0;
	const FVector Corner(-Quads * Scale / 2.0, -Quads * Scale / 2.0, 0.0);
	ALandscape* Landscape = World->SpawnActor<ALandscape>(Corner, FRotator::ZeroRotator);
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(FVector(Scale, Scale, 100.0));
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((Size * Size) / (2048 * 2048) + 1), (uint32)2);

	TMap<FGuid, TArray<uint16>> HeightPerLayer;
	HeightPerLayer.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayersPerLayer;
	LayersPerLayer.Add(FGuid(), Layers);
	Landscape->Import(FGuid::NewGuid(), 0, 0, Quads, Quads, 1, QuadsPerSection, HeightPerLayer, TEXT(""), LayersPerLayer,
		ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("World builder: landscape import produced no landscape info"));
		return nullptr;
	}
	Info->UpdateLayerInfoMap(Landscape);
	for (const FLandscapeImportLayerInfo& Layer : Layers)
	{
		Landscape->AddTargetLayer(Layer.LayerName, FLandscapeTargetLayerSettings(Layer.LayerInfo));
		const int32 Index = Info->GetLayerInfoIndex(Layer.LayerName);
		if (Index != INDEX_NONE)
		{
			Info->Layers[Index].LayerInfoObj = Layer.LayerInfo;
		}
	}
	// Landscapes keep their heights in edit layers, merged into the final heightmaps on the GPU. Nothing ticks the merge in
	// a commandlet, so force it now (the commandlet needs -AllowCommandletRendering), or the saved landscape is flat.
	Landscape->ForceLayersFullUpdate();
	Landscape->ForceUpdateLayersContent();
	const FBox Bounds = Landscape->GetComponentsBoundingBox(true);
	if (Bounds.GetSize().Z < 100.0)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("World builder: landscape is still flat after the edit-layer merge (run with -AllowCommandletRendering)"));
		return nullptr;
	}
	if (ULandscapeSubsystem* Landscapes = World->GetSubsystem<ULandscapeSubsystem>(); Landscapes && Landscapes->IsGridBased())
	{
		Landscapes->ChangeGridSize(Info, WorldPartitionGridSize);
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("World builder: landscape %dx%d quads, %d layers"), Quads, Quads, Layers.Num());
	return Landscape;
#else
	return nullptr;
#endif
}

float UShinobiWorldBuilderLibrary::SetTrunkCollision(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	const FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
	if (!Description || Description->Vertices().Num() == 0)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("SetTrunkCollision: %s has no mesh description"), Mesh ? *Mesh->GetName() : TEXT("null"));
		return 0.f;
	}
	const TVertexAttributesConstRef<FVector3f> Positions = FStaticMeshConstAttributes(*Description).GetVertexPositions();
	float MinZ = TNumericLimits<float>::Max(), MaxZ = TNumericLimits<float>::Lowest();
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		MinZ = FMath::Min(MinZ, Positions[Vertex].Z);
		MaxZ = FMath::Max(MaxZ, Positions[Vertex].Z);
	}
	const float Height = MaxZ - MinZ, Band = MinZ + 0.1f * Height;
	TArray<FVector2f> Base;
	FVector2f Centre = FVector2f::ZeroVector;
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		if (Positions[Vertex].Z <= Band)
		{
			Base.Add(FVector2f(Positions[Vertex].X, Positions[Vertex].Y));
			Centre += Base.Last();
		}
	}
	Centre /= FMath::Max(1, Base.Num());
	TArray<float> Distances;
	for (const FVector2f& Point : Base)
	{
		Distances.Add(FVector2f::Distance(Point, Centre));
	}
	Distances.Sort();
	const float Radius = FMath::Clamp(Distances.IsEmpty() ? 0.f : Distances[Distances.Num() * 4 / 5], 10.f, 80.f);
	const float Top = FMath::Min(0.6f * Height, 600.f);

	Mesh->CreateBodySetup();
	UBodySetup* Body = Mesh->GetBodySetup();
	Body->Modify();
	Body->RemoveSimpleCollision();
	FKSphylElem Capsule(Radius, FMath::Max(0.f, Top - 2.f * Radius));
	Capsule.Center = FVector(Centre.X, Centre.Y, MinZ + FMath::Max(Top * 0.5f, Radius));
	Body->AggGeom.SphylElems.Add(Capsule);
	Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
	Mesh->MarkPackageDirty();
	return Radius;
#else
	return 0.f;
#endif
}

FVector UShinobiWorldBuilderLibrary::RecenterPivot(UStaticMesh* Mesh)
{
#if WITH_EDITOR
	FMeshDescription* Description = Mesh ? Mesh->GetMeshDescription(0) : nullptr;
	if (!Description || Description->Vertices().Num() == 0)
	{
		return FVector::ZeroVector;
	}
	TVertexAttributesRef<FVector3f> Positions = FStaticMeshAttributes(*Description).GetVertexPositions();
	FBox3f Bounds(ForceInit);
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		Bounds += Positions[Vertex];
	}
	const FVector3f Offset(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	if (Offset.Size() < 0.5f)
	{
		return FVector::ZeroVector;
	}
	for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
	{
		Positions[Vertex] -= Offset;
	}
	Mesh->Modify();
	Mesh->CommitMeshDescription(0);
	Mesh->Build();
	Mesh->MarkPackageDirty();
	return FVector(Offset);
#else
	return FVector::ZeroVector;
#endif
}

UStaticMesh* UShinobiWorldBuilderLibrary::BakeSkeletalToStatic(UWorld* World, USkeletalMesh* Skeletal, const FString& PackageName)
{
#if WITH_EDITOR
	if (!World || !Skeletal)
	{
		return nullptr;
	}
	AActor* Holder = World->SpawnActor<AActor>();
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Holder);
	Holder->SetRootComponent(Component);
	Component->SetSkeletalMeshAsset(Skeletal);
	Component->RegisterComponent();
	IMeshUtilities& Utilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	UStaticMesh* Static = Utilities.ConvertMeshesToStaticMesh({ Component }, FTransform::Identity, PackageName);
	Holder->Destroy();
	if (!Static)
	{
		UE_LOG(LogShinobiCoop, Error, TEXT("BakeSkeletalToStatic: %s failed"), *Skeletal->GetName());
		return nullptr;
	}

	// Megaplants are Nanite assemblies: the skeletal mesh is the trunk, the leafy branches are static mesh parts placed
	// at nodes (often relative to a bone). Carry the assembly over, with bone-relative nodes baked into the reference
	// pose, and give the static mesh the skeletal mesh's material list so the parts' material remaps still line up.
	const FNaniteAssemblyData& Assembly = Skeletal->GetNaniteSettings().NaniteAssemblyData;
	if (Assembly.IsValid())
	{
		TArray<FStaticMaterial> Materials;
		for (const FSkeletalMaterial& Material : Skeletal->GetMaterials())
		{
			Materials.Add(FStaticMaterial(Material.MaterialInterface, Material.MaterialSlotName, Material.ImportedMaterialSlotName));
		}
		const TArray<FStaticMaterial> Baked = Static->GetStaticMaterials();
		FMeshSectionInfoMap& Sections = Static->GetSectionInfoMap();
		for (int32 Lod = 0; Lod < Static->GetNumSourceModels(); ++Lod)
		{
			for (int32 Section = 0; Section < Sections.GetSectionNumber(Lod); ++Section)
			{
				FMeshSectionInfo Info = Sections.Get(Lod, Section);
				const UMaterialInterface* Used = Baked.IsValidIndex(Info.MaterialIndex) ? Baked[Info.MaterialIndex].MaterialInterface.Get() : nullptr;
				const int32 Index = Materials.IndexOfByPredicate([Used](const FStaticMaterial& M) { return M.MaterialInterface == Used; });
				if (Index != INDEX_NONE)
				{
					Info.MaterialIndex = Index;
					Sections.Set(Lod, Section, Info);
				}
			}
		}
		Static->SetStaticMaterials(Materials);

		const FReferenceSkeleton& Skeleton = Skeletal->GetRefSkeleton();
		TArray<FTransform> BoneToComponent;
		BoneToComponent.SetNum(Skeleton.GetRawBoneNum());
		for (int32 Bone = 0; Bone < BoneToComponent.Num(); ++Bone)
		{
			const int32 Parent = Skeleton.GetRawParentIndex(Bone);
			BoneToComponent[Bone] = Parent >= 0 ? Skeleton.GetRawRefBonePose()[Bone] * BoneToComponent[Parent] : Skeleton.GetRawRefBonePose()[Bone];
		}
		FNaniteAssemblyData BakedAssembly = Assembly;
		int32 Converted = 0;
		for (FNaniteAssemblyNode& Node : BakedAssembly.Nodes)
		{
			if (Node.TransformSpace == ENaniteAssemblyNodeTransformSpace::BoneRelative && Node.BoneInfluences.Num() > 0)
			{
				const FNaniteAssemblyBoneInfluence* Main = &Node.BoneInfluences[0];
				for (const FNaniteAssemblyBoneInfluence& Influence : Node.BoneInfluences)
				{
					Main = Influence.BoneWeight > Main->BoneWeight ? &Influence : Main;
				}
				if (BoneToComponent.IsValidIndex(Main->BoneIndex))
				{
					Node.Transform = FTransform3f(FTransform(Node.Transform) * BoneToComponent[Main->BoneIndex]);
					++Converted;
				}
			}
			Node.TransformSpace = ENaniteAssemblyNodeTransformSpace::Local;
			Node.BoneInfluences.Empty();
		}
		FMeshNaniteSettings Nanite = Static->GetNaniteSettings();
		Nanite.bEnabled = true;
		Nanite.NaniteAssemblyData = BakedAssembly;
		Static->SetNaniteSettings(Nanite);
		Static->Build();
		Static->MarkPackageDirty();
		UE_LOG(LogShinobiCoop, Log, TEXT("BakeSkeletalToStatic: %s: %d parts, %d nodes (%d bone-relative baked), %d materials"),
			*Skeletal->GetName(), BakedAssembly.Parts.Num(), BakedAssembly.Nodes.Num(), Converted, Materials.Num());
	}
	return Static;
#else
	return nullptr;
#endif
}
