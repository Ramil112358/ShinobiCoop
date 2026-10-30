#include "Shinobi/Hero/ShinobiMutableMeshLibrary.h"

#include "ShinobiCoop.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "AnimationRuntime.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "UObject/Package.h"
#include "DNAReader.h"
#include "DNAUtils.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "MeshDescription.h"
#include "MeshQueries.h"
#include "SkeletalMeshAttributes.h"

namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;

	/** A surface to find closest points on, with each triangle's outward normal. */
	struct FSurface
	{
		FDynamicMesh3 Mesh;
		FDynamicMeshAABBTree3 Tree;
		double Sign = 1.0; // -1 when the triangle normals point into the body

		FSurface(const TArray<FVector>& Positions, const TArray<int32>& Triangles)
		{
			for (const FVector& Position : Positions)
			{
				Mesh.AppendVertex(Position);
			}
			for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
			{
				Mesh.AppendTriangle(Triangles[Index], Triangles[Index + 1], Triangles[Index + 2]);
			}
			// Which way the normals face: mostly away from the body's upright axis (a body is open at the neck, so no volume
			// sign; and the winding convention varies by source).
			const FVector Axis = FBox(Positions).GetCenter();
			double Facing = 0.0;
			for (const int32 Triangle : Mesh.TriangleIndicesItr())
			{
				const FVector Away = Mesh.GetTriCentroid(Triangle) - Axis;
				Facing += Mesh.GetTriArea(Triangle) * FVector::DotProduct(Mesh.GetTriNormal(Triangle), FVector(Away.X, Away.Y, 0.0).GetSafeNormal());
			}
			Sign = Facing < 0.0 ? -1.0 : 1.0;
			Tree.SetMesh(&Mesh, true);
		}

		/** The closest surface point to Point within MaxDistance, and how far Point is outside (negative: inside). */
		bool Closest(const FVector& Point, double MaxDistance, FVector& Out, double& Outside) const
		{
			int32 Triangle;
			FVector Bary;
			return Closest(Point, MaxDistance, Out, Outside, Triangle, Bary);
		}

		/** ...and the triangle (its corners index the surface's positions) and the point's barycentric coordinates in it. */
		bool Closest(const FVector& Point, double MaxDistance, FVector& Out, double& Outside, int32& Triangle, FVector& Bary) const
		{
			double DistanceSquared = 0.0;
			Triangle = Tree.FindNearestTriangle(Point, DistanceSquared, UE::Geometry::IMeshSpatial::FQueryOptions(MaxDistance));
			if (Triangle == IndexConstants::InvalidID)
			{
				return false;
			}
			const UE::Geometry::FDistPoint3Triangle3d Query = UE::Geometry::TMeshQueries<FDynamicMesh3>::TriangleDistance(Mesh, Triangle, Point);
			Out = Query.ClosestTrianglePoint;
			Bary = Query.TriangleBaryCoords;
			Outside = FVector::DotProduct(Point - Out, Mesh.GetTriNormal(Triangle)) * Sign;
			return true;
		}
	};

	/** Mesh's reference pose in component space. */
	TArray<FTransform> ComponentPose(const FReferenceSkeleton& Skeleton)
	{
		TArray<FTransform> Pose;
		for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
		{
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			Pose.Add(Parent == INDEX_NONE ? Skeleton.GetRefBonePose()[Bone] : Skeleton.GetRefBonePose()[Bone] * Pose[Parent]);
		}
		return Pose;
	}

	/**
	 * Description's vertex positions (vertex-ID order) with Mesh's body scaled to Target's head height and its limbs turned
	 * so each bone of LimbChains points to its child as Target's does, through Mesh's own skinning: a source body and its
	 * pieces then stand like Target (the Mutable Sample's and the UE mannequin's arms are raised higher than a MetaHuman's).
	 * The bones move as Body's do (by name; Mesh by default), so a piece and the body it was made on move alike.
	 */
	TArray<FVector> PoseLike(const FMeshDescription& Description, const USkeletalMesh& Mesh, const USkeletalMesh& Target, const USkeletalMesh* Body = nullptr)
	{
		static const TPair<const TCHAR*, const TCHAR*> LimbChains[] = {
			{TEXT("upperarm_l"), TEXT("lowerarm_l")}, {TEXT("lowerarm_l"), TEXT("hand_l")}, {TEXT("hand_l"), TEXT("middle_01_l")},
			{TEXT("upperarm_r"), TEXT("lowerarm_r")}, {TEXT("lowerarm_r"), TEXT("hand_r")}, {TEXT("hand_r"), TEXT("middle_01_r")},
			{TEXT("thigh_l"), TEXT("calf_l")}, {TEXT("calf_l"), TEXT("foot_l")}, {TEXT("thigh_r"), TEXT("calf_r")}, {TEXT("calf_r"), TEXT("foot_r")}};
		const FReferenceSkeleton& Skeleton = (Body ? *Body : Mesh).GetRefSkeleton();
		const FReferenceSkeleton& TargetSkeleton = Target.GetRefSkeleton();
		const TArray<FTransform> Old = ComponentPose(Skeleton);
		const TArray<FTransform> Wanted = ComponentPose(TargetSkeleton);
		const int32 Head = Skeleton.FindBoneIndex(TEXT("head")), TargetHead = TargetSkeleton.FindBoneIndex(TEXT("head"));
		const double Scale = Head != INDEX_NONE && TargetHead != INDEX_NONE && Old[Head].GetLocation().Z > 1.0
			? Wanted[TargetHead].GetLocation().Z / Old[Head].GetLocation().Z : 1.0;
		TArray<FTransform> Scaled, New;
		for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
		{
			Scaled.Add(FTransform(Old[Bone].GetRotation(), Old[Bone].GetLocation() * Scale));
			FTransform Local = Skeleton.GetRefBonePose()[Bone];
			Local.SetLocation(Local.GetLocation() * Scale);
			const int32 Parent = Skeleton.GetParentIndex(Bone);
			FTransform Here = Parent == INDEX_NONE ? Local : Local * New[Parent];
			for (const TPair<const TCHAR*, const TCHAR*>& Chain : LimbChains)
			{
				const int32 Child = Skeleton.FindBoneIndex(Chain.Value);
				const int32 From = TargetSkeleton.FindBoneIndex(Chain.Key), To = TargetSkeleton.FindBoneIndex(Chain.Value);
				if (Skeleton.GetBoneName(Bone) == Chain.Key && Child != INDEX_NONE && From != INDEX_NONE && To != INDEX_NONE)
				{
					const FVector Now = Here.TransformVector(Skeleton.GetRefBonePose()[Child].GetLocation() * Scale);
					Here.SetRotation(FQuat::FindBetweenVectors(Now, Wanted[To].GetLocation() - Wanted[From].GetLocation()) * Here.GetRotation());
				}
			}
			New.Add(Here);
		}
		// Mesh's bone indices in Skeleton (by name).
		TArray<int32> Map;
		for (int32 Bone = 0; Bone < Mesh.GetRefSkeleton().GetNum(); ++Bone)
		{
			Map.Add(Skeleton.FindBoneIndex(Mesh.GetRefSkeleton().GetBoneName(Bone)));
		}
		FSkeletalMeshConstAttributes Attributes(Description);
		const FSkinWeightsVertexAttributesConstRef Skin = Attributes.GetVertexSkinWeights();
		const TVertexAttributesConstRef<FVector3f> Points = Description.GetVertexPositions();
		TArray<FVector> Posed;
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			const FVector Point = FVector(Points[Vertex]) * Scale;
			FVector Sum = FVector::ZeroVector;
			float Total = 0.f;
			for (const UE::AnimationCore::FBoneWeight& Weight : Skin.Get(Vertex))
			{
				const int32 Bone = Map.IsValidIndex(Weight.GetBoneIndex()) ? Map[Weight.GetBoneIndex()] : INDEX_NONE;
				if (Bone != INDEX_NONE)
				{
					Sum += New[Bone].TransformPosition(Scaled[Bone].InverseTransformPosition(Point)) * Weight.GetWeight();
					Total += Weight.GetWeight();
				}
			}
			Posed.Add(Total > 0.f ? Sum / Total : Point);
		}
		return Posed;
	}

	/** A mesh description's vertex positions (plus morph Morph's deltas, when it has it) and triangles, in vertex-ID order. */
	bool Surface(const FMeshDescription& Description, TArray<FVector>& Positions, TArray<int32>& Triangles, FName Morph = NAME_None)
	{
		TMap<FVertexID, int32> Index;
		const TVertexAttributesConstRef<FVector3f> Points = Description.GetVertexPositions();
		const TVertexAttributesConstRef<FVector3f> Delta = Morph.IsNone() ? TVertexAttributesConstRef<FVector3f>()
			: FSkeletalMeshConstAttributes(Description).GetVertexMorphPositionDelta(Morph);
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			Index.Add(Vertex, Positions.Add(FVector(Points[Vertex] + (Delta.IsValid() ? Delta[Vertex] : FVector3f::ZeroVector))));
		}
		for (const FTriangleID Triangle : Description.Triangles().GetElementIDs())
		{
			for (const FVertexID Vertex : Description.GetTriangleVertices(Triangle))
			{
				Triangles.Add(Index[Vertex]);
			}
		}
		return Positions.Num() > 0;
	}
}
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

USkeletalMesh* UShinobiMutableMeshLibrary::ExportClothAsset(UObject* ClothAsset, const FString& AssetPath)
{
#if WITH_EDITOR
	const UChaosClothAssetBase* Cloth = Cast<UChaosClothAssetBase>(ClothAsset);
	if (!Cloth)
	{
		return nullptr;
	}
	const FString Name = FPackageName::GetShortName(AssetPath);
	UPackage* Package = CreatePackage(*AssetPath);
	USkeletalMesh* Mesh = FindObject<USkeletalMesh>(Package, *Name);
	const bool bNew = !Mesh;
	if (bNew)
	{
		Mesh = NewObject<USkeletalMesh>(Package, *Name, RF_Public | RF_Standalone);
	}
	if (!Cloth->ExportToSkeletalMesh(*Mesh))
	{
		return nullptr;
	}
	if (!Mesh->GetSkeleton())
	{
		Mesh->SetSkeleton(const_cast<USkeleton*>(Cloth->GetSkeleton())); // (the converter leaves it unset)
	}
	if (bNew)
	{
		FAssetRegistryModule::AssetCreated(Mesh);
	}
	Mesh->MarkPackageDirty();
	return Mesh;
#else
	return nullptr;
#endif
}

bool UShinobiMutableMeshLibrary::ReadDnaBody(const FString& DnaPath, TArray<FVector>& Positions, TArray<int32>& Triangles)
{
	Positions.Reset();
	Triangles.Reset();
#if WITH_EDITOR
	const TSharedPtr<IDNAReader> Reader = ReadDNAFromFile(DnaPath, EDNADataLayer::Geometry);
	if (!Reader || Reader->GetMeshCount() == 0)
	{
		return false;
	}
	for (uint32 Vertex = 0; Vertex < Reader->GetVertexPositionCount(0); ++Vertex)
	{
		Positions.Add(Reader->GetVertexPosition(0, Vertex));
	}
	// Faces index vertex layouts (position + UV + normal); each is a fan of 3 or more.
	for (uint32 Face = 0; Face < Reader->GetFaceCount(0); ++Face)
	{
		const TArrayView<const uint32> Layouts = Reader->GetFaceVertexLayoutIndices(0, Face);
		for (int32 Corner = 2; Corner < Layouts.Num(); ++Corner)
		{
			for (const uint32 Layout : {Layouts[0], Layouts[Corner - 1], Layouts[Corner]})
			{
				Triangles.Add(int32(Reader->GetVertexLayout(0, Layout).Position));
			}
		}
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("ReadDnaBody %s: %d vertices, %d triangles, bounds %s"), *DnaPath, Positions.Num(), Triangles.Num() / 3,
		*FBox(Positions).ToString());
	return Triangles.Num() > 0;
#else
	return false;
#endif
}

bool UShinobiMutableMeshLibrary::ReadMeshSurface(USkeletalMesh* Mesh, TArray<FVector>& Positions, TArray<int32>& Triangles, FName Morph)
{
	Positions.Reset();
	Triangles.Reset();
#if WITH_EDITOR
	FMeshDescription Description;
	return Mesh && Mesh->CloneMeshDescription(0, Description) && Surface(Description, Positions, Triangles, Morph);
#else
	return false;
#endif
}

bool UShinobiMutableMeshLibrary::ReadPosedSurface(USkeletalMesh* Mesh, USkeletalMesh* Target, TArray<FVector>& Positions, TArray<int32>& Triangles)
{
#if WITH_EDITOR
	FMeshDescription Description;
	if (!Target || !Mesh || !Mesh->CloneMeshDescription(0, Description) || !ReadMeshSurface(Mesh, Positions, Triangles))
	{
		return false;
	}
	Positions = PoseLike(Description, *Mesh, *Target);
	return true;
#else
	return false;
#endif
}

int32 UShinobiMutableMeshLibrary::RebindPiece(USkeletalMesh* Out, USkeletalMesh* Base, FName FitMorph, USkeletalMesh* Piece, USkeletalMesh* PoseFrom)
{
#if WITH_EDITOR
	FMeshDescription Body, Description;
	TArray<FVector> Positions, BasePositions;
	TArray<int32> Triangles, BaseTriangles;
	if (!Out || !Base || !Piece || !Base->CloneMeshDescription(0, Body) || !Surface(Body, Positions, Triangles, FitMorph)
		|| !Surface(Body, BasePositions, BaseTriangles) || !Piece->CloneMeshDescription(0, Description))
	{
		return 0;
	}
	// The base's skin weights by Surface's vertex order.
	TArray<FVertexID> Ids;
	for (const FVertexID Vertex : Body.Vertices().GetElementIDs())
	{
		Ids.Add(Vertex);
	}
	const FSkinWeightsVertexAttributesConstRef BodySkin = FSkeletalMeshConstAttributes(Body).GetVertexSkinWeights();
	const FSurface Fit(Positions, Triangles);
	const bool bPose = PoseFrom != nullptr;
	const TArray<FVector> Posed = bPose ? PoseLike(Description, *Piece, *Base, PoseFrom) : TArray<FVector>();
	FSkeletalMeshAttributes Attributes(Description);
	Attributes.Register();
	FSkinWeightsVertexAttributesRef Skin = Attributes.GetVertexSkinWeights();
	TVertexAttributesRef<FVector3f> Points = Description.GetVertexPositions();
	int32 Index = 0;
	for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
	{
		const FVector Point = bPose ? Posed[Index++] : FVector(Points[Vertex]);
		Points[Vertex] = FVector3f(Point);
		FVector Closest, Bary;
		double Outside = 0.0;
		int32 Triangle = INDEX_NONE;
		TMap<int32, float> Blend;
		if (Fit.Closest(Point, 100.0, Closest, Outside, Triangle, Bary))
		{
			const UE::Geometry::FIndex3i Corners = Fit.Mesh.GetTriangle(Triangle);
			FVector OnBase = FVector::ZeroVector;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				OnBase += BasePositions[Corners[Corner]] * Bary[Corner];
				for (const UE::AnimationCore::FBoneWeight& Weight : BodySkin.Get(Ids[Corners[Corner]]))
				{
					Blend.FindOrAdd(Weight.GetBoneIndex()) += Weight.GetWeight() * float(Bary[Corner]);
				}
			}
			Points[Vertex] = FVector3f(OnBase + (Point - Closest)); // from the fit shape onto the base, its offset kept
		}
		Blend.ValueSort([](float A, float B) { return A > B; });
		TArray<UE::AnimationCore::FBoneWeight> Weights;
		float Total = 0.f;
		for (const TPair<int32, float>& Each : Blend)
		{
			if (Weights.Num() < 8 && Each.Value > 0.f)
			{
				Weights.Emplace(Each.Key, Each.Value);
				Total += Each.Value;
			}
		}
		for (UE::AnimationCore::FBoneWeight& Weight : Weights)
		{
			Weight.SetWeight(Weight.GetWeight() / Total);
		}
		Skin.Set(Vertex, Weights);
	}
	Out->SetMaterials(Piece->GetMaterials());
	for (int32 Lod = 0; Lod < Out->GetLODNum(); ++Lod)
	{
		FMeshDescription Copy = Description;
		Out->CreateMeshDescription(Lod, MoveTemp(Copy));
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bMarkPackageDirty = true;
		Out->CommitMeshDescription(Lod, Params);
	}
	Out->Build();
	return Index ? Index : Description.Vertices().Num();
#else
	return 0;
#endif
}

FString UShinobiMutableMeshLibrary::BakeFitMorph(USkeletalMesh* Base, FName Morph, const TArray<FVector>& Positions, const TArray<int32>& Triangles, float MaxMove,
	int32 Smooth, const TArray<FName>& KeepBones, int32 KeepFade)
{
	FString Report;
#if WITH_EDITOR
	if (!Base || Morph.IsNone() || Triangles.Num() < 3)
	{
		return TEXT("no mesh, morph name or surface");
	}
	const FSurface Body(Positions, Triangles);
	// KeepBones and their children.
	const FReferenceSkeleton& Skeleton = Base->GetRefSkeleton();
	TArray<bool> Kept;
	Kept.Init(false, Skeleton.GetNum());
	for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
	{
		const int32 Parent = Skeleton.GetParentIndex(Bone);
		Kept[Bone] = KeepBones.Contains(Skeleton.GetBoneName(Bone)) || (Parent != INDEX_NONE && Kept[Parent]);
	}
	for (int32 Lod = 0; Lod < Base->GetLODNum(); ++Lod)
	{
		FMeshDescription Description;
		if (!Base->CloneMeshDescription(Lod, Description))
		{
			continue;
		}
		FSkeletalMeshAttributes Attributes(Description);
		Attributes.Register();
		const TVertexAttributesConstRef<FVector3f> Points = Description.GetVertexPositions();
		const FSkinWeightsVertexAttributesRef Skin = Attributes.GetVertexSkinWeights();
		// How much each vertex stays: its skin weight on the kept bones, faded out over KeepFade rings of neighbours so the
		// morph doesn't stop in one ring (a sheared wrist ring spikes the cuffs bound to it).
		TMap<FVertexID, float> Keep;
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			float Sum = 0.f;
			for (const UE::AnimationCore::FBoneWeight& Weight : Skin.Get(Vertex))
			{
				Sum += Kept.IsValidIndex(Weight.GetBoneIndex()) && Kept[Weight.GetBoneIndex()] ? Weight.GetWeight() : 0.f;
			}
			Keep.Add(Vertex, FMath::Min(Sum, 1.f));
		}
		for (int32 Pass = 0; Pass < KeepFade; ++Pass)
		{
			TMap<FVertexID, float> Next;
			for (const TPair<FVertexID, float>& Each : Keep)
			{
				float Most = Each.Value;
				for (const FVertexID Neighbour : Description.GetVertexAdjacentVertices(Each.Key))
				{
					Most = FMath::Max(Most, Keep[Neighbour] * 0.85f); // grows the kept region outward, fading
				}
				Next.Add(Each.Key, Most);
			}
			Keep = MoveTemp(Next);
		}
		TMap<FVertexID, FVector> Move;
		for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
		{
			FVector Closest;
			double Outside = 0.0;
			const FVector Point(Points[Vertex]);
			Move.Add(Vertex, Body.Closest(Point, MaxMove, Closest, Outside) ? (Closest - Point) * (1.0 - Keep[Vertex]) : FVector::ZeroVector);
		}
		// Smoothed over neighbours, so nearby vertices don't snap to different sides of a crease (armpits, crotch).
		for (int32 Pass = 0; Pass < Smooth; ++Pass)
		{
			TMap<FVertexID, FVector> Next;
			for (const TPair<FVertexID, FVector>& Each : Move)
			{
				FVector Sum = FVector::ZeroVector;
				int32 Count = 0;
				for (const FVertexID Neighbour : Description.GetVertexAdjacentVertices(Each.Key))
				{
					Sum += Move[Neighbour];
					++Count;
				}
				Next.Add(Each.Key, Count ? 0.5 * Each.Value + 0.5 * Sum / Count : Each.Value);
			}
			Move = MoveTemp(Next);
		}
		Attributes.UnregisterMorphTargetAttribute(Morph);
		Attributes.RegisterMorphTargetAttribute(Morph, false);
		TVertexAttributesRef<FVector3f> Delta = Attributes.GetVertexMorphPositionDelta(Morph);
		double Total = 0.0, Most = 0.0;
		int32 Moved = 0;
		for (const TPair<FVertexID, FVector>& Each : Move)
		{
			const FVector Step = Each.Value.GetClampedToMaxSize(MaxMove);
			Delta[Each.Key] = FVector3f(Step);
			Total += Step.Size();
			Most = FMath::Max(Most, Step.Size());
			Moved += Step.Size() > 0.1 ? 1 : 0;
		}
		if (Lod == 0)
		{
			Report = FString::Printf(TEXT("moved %d of %d vertices, mean %.2f cm, max %.2f cm"), Moved, Move.Num(), Move.Num() ? Total / Move.Num() : 0.0, Most);
		}
		Base->CreateMeshDescription(Lod, MoveTemp(Description));
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bMarkPackageDirty = true;
		Base->CommitMeshDescription(Lod, Params);
	}
	Base->Build();
#endif
	return Report;
}

int32 UShinobiMutableMeshLibrary::MakeClipVolume(USkeletalMesh* Clip, USkeletalMesh* Base, FName FitMorph, USkeletalMesh* Piece, float Reach, int32 Erode, float Out, float Depth)
{
	int32 Covered = 0;
#if WITH_EDITOR
	FMeshDescription Description;
	TArray<FVector> Positions, PiecePoints;
	TArray<int32> Triangles, PieceTriangles;
	if (!Clip || !Base || !Base->CloneMeshDescription(0, Description) || !Surface(Description, Positions, Triangles, FitMorph)
		|| !ReadMeshSurface(Piece, PiecePoints, PieceTriangles))
	{
		return 0;
	}
	// Indices follow Surface's vertex order.
	TArray<FVertexID> Ids;
	for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
	{
		Ids.Add(Vertex);
	}
	const FSurface Body(Positions, Triangles);
	const FSurface Cloth(PiecePoints, PieceTriangles);
	TArray<FVector> Normals;
	Normals.Init(FVector::ZeroVector, Positions.Num());
	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const FVector Area = FVector::CrossProduct(Positions[Triangles[Index + 1]] - Positions[Triangles[Index]], Positions[Triangles[Index + 2]] - Positions[Triangles[Index]]);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Normals[Triangles[Index + Corner]] += Area * Body.Sign;
		}
	}
	TArray<bool> In;
	In.Init(false, Positions.Num());
	for (int32 Vertex = 0; Vertex < Positions.Num(); ++Vertex)
	{
		Normals[Vertex] = Normals[Vertex].GetSafeNormal();
		double Hit = 0.0;
		int32 Triangle = IndexConstants::InvalidID;
		In[Vertex] = Cloth.Tree.FindNearestHitTriangle(FRay3d(Positions[Vertex] - Normals[Vertex], Normals[Vertex]), Hit, Triangle) && Hit <= Reach + 1.0;
	}
	// The covered triangles, eroded at the rim: a vertex on a triangle that isn't covered leaves.
	TArray<int32> Region;
	for (int32 Pass = 0; Pass <= Erode; ++Pass)
	{
		Region.Reset();
		TArray<bool> Rim;
		Rim.Init(false, Positions.Num());
		for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
		{
			const bool bAll = In[Triangles[Index]] && In[Triangles[Index + 1]] && In[Triangles[Index + 2]];
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				Rim[Triangles[Index + Corner]] |= !bAll;
			}
			if (bAll)
			{
				Region.Add(Index);
			}
		}
		if (Pass < Erode)
		{
			for (int32 Vertex = 0; Vertex < Positions.Num(); ++Vertex)
			{
				In[Vertex] = In[Vertex] && !Rim[Vertex];
			}
		}
	}
	Covered = Region.Num();
	if (!Covered)
	{
		return 0;
	}
	// The shell, wound outward: the region pushed out, a reversed copy pushed in, walls along each rim edge.
	FSkeletalMeshAttributes Attributes(Description);
	FSkinWeightsVertexAttributesRef Skin = Attributes.GetVertexSkinWeights();
	TArray<TArray<UE::AnimationCore::FBoneWeight>> Weights;
	for (const FVertexID Vertex : Ids)
	{
		TArray<UE::AnimationCore::FBoneWeight>& Each = Weights.AddDefaulted_GetRef();
		for (const UE::AnimationCore::FBoneWeight& Weight : Skin.Get(Vertex))
		{
			Each.Add(Weight);
		}
	}
	const FName Slot = Attributes.GetPolygonGroupMaterialSlotNames()[*Description.PolygonGroups().GetElementIDs().CreateConstIterator()];
	TArray<FIntVector> Faces;
	TMap<TPair<int32, int32>, int32> EdgeUse;
	for (const int32 Index : Region)
	{
		const bool bOutward = Body.Sign > 0.0;
		const FIntVector Face(Triangles[Index], bOutward ? Triangles[Index + 1] : Triangles[Index + 2], bOutward ? Triangles[Index + 2] : Triangles[Index + 1]);
		Faces.Add(Face);
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 A = Face[Corner], B = Face[(Corner + 1) % 3];
			++EdgeUse.FindOrAdd(TPair<int32, int32>(FMath::Min(A, B), FMath::Max(A, B)));
		}
	}
	Description.Empty();
	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Slot;
	TMap<int32, FVertexID> Outer, Inner;
	auto Place = [&](TMap<int32, FVertexID>& Layer, int32 Vertex, float Offset) -> FVertexID
	{
		if (const FVertexID* Found = Layer.Find(Vertex))
		{
			return *Found;
		}
		const FVertexID New = Description.CreateVertex();
		Description.GetVertexPositions()[New] = FVector3f(Positions[Vertex] + Normals[Vertex] * Offset);
		Skin.Set(New, Weights[Vertex]);
		return Layer.Add(Vertex, New);
	};
	auto Triangle = [&](FVertexID A, FVertexID B, FVertexID C)
	{
		TArray<FVertexInstanceID> Corners;
		for (const FVertexID Vertex : {A, B, C})
		{
			Corners.Add(Description.CreateVertexInstance(Vertex));
		}
		Description.CreateTriangle(Group, Corners);
	};
	for (const FIntVector& Face : Faces)
	{
		Triangle(Place(Outer, Face.X, Out), Place(Outer, Face.Y, Out), Place(Outer, Face.Z, Out));
		Triangle(Place(Inner, Face.X, -Depth), Place(Inner, Face.Z, -Depth), Place(Inner, Face.Y, -Depth));
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 A = Face[Corner], B = Face[(Corner + 1) % 3];
			if (EdgeUse[TPair<int32, int32>(FMath::Min(A, B), FMath::Max(A, B))] == 1) // a rim edge
			{
				Triangle(Outer[B], Outer[A], Inner[A]);
				Triangle(Outer[B], Inner[A], Inner[B]);
			}
		}
	}
	for (int32 Lod = 0; Lod < Clip->GetLODNum(); ++Lod)
	{
		FMeshDescription Copy = Description;
		Clip->CreateMeshDescription(Lod, MoveTemp(Copy));
		USkeletalMesh::FCommitMeshDescriptionParams Params;
		Params.bMarkPackageDirty = true;
		Clip->CommitMeshDescription(Lod, Params);
	}
	Clip->Build();
#endif
	return Covered;
}

float UShinobiMutableMeshLibrary::FractionInside(USkeletalMesh* Piece, USkeletalMesh* Body, float Tolerance, FName Morph)
{
#if WITH_EDITOR
	TArray<FVector> PiecePoints, BodyPoints;
	TArray<int32> PieceTriangles, BodyTriangles;
	if (!ReadMeshSurface(Piece, PiecePoints, PieceTriangles) || !ReadMeshSurface(Body, BodyPoints, BodyTriangles, Morph))
	{
		return -1.f;
	}
	const FSurface Surface(BodyPoints, BodyTriangles);
	int32 Inside = 0;
	for (const FVector& Point : PiecePoints)
	{
		FVector Closest;
		double Outside = 0.0;
		Inside += Surface.Closest(Point, 20.0, Closest, Outside) && Outside < -Tolerance ? 1 : 0;
	}
	return float(Inside) / PiecePoints.Num();
#else
	return -1.f;
#endif
}
