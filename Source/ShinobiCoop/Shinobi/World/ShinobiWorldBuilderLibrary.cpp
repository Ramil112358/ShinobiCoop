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
