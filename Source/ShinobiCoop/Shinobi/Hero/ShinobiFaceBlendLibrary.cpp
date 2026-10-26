#include "Shinobi/Hero/ShinobiFaceBlendLibrary.h"

#include "ShinobiCoop.h"
#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "AnimationRuntime.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#endif

FString UShinobiFaceBlendLibrary::BakeFaceBlendMorphs(USkeletalMesh* Face, const TArray<USkeletalMesh*>& Others, const TArray<FName>& Names, int32 MaxLods)
{
	FString Report;
#if WITH_EDITOR
	if (!Face || Others.Num() != Names.Num())
	{
		return TEXT("BakeFaceBlendMorphs: no face, or Others and Names differ in length");
	}
	auto RootOf = [](const USkeletalMesh* Mesh) -> FVector3f
	{
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 Root = Ref.FindBoneIndex(TEXT("FACIAL_C_FacialRoot"));
		return Root == INDEX_NONE ? FVector3f::ZeroVector : FVector3f(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Root).GetLocation());
	};
	const FVector3f OwnRoot = RootOf(Face);
	TArray<int32> Changed;
	for (int32 Lod = 0; Lod < FMath::Min(MaxLods, Face->GetLODNum()); ++Lod)
	{
		FMeshDescription Own;
		if (!Face->CloneMeshDescription(Lod, Own))
		{
			Report += FString::Printf(TEXT(" LOD%d: no mesh description;"), Lod);
			continue;
		}
		FSkeletalMeshAttributes Attributes(Own);
		Attributes.Register();
		const TVertexAttributesConstRef<FVector3f> OwnPositions = Own.GetVertexPositions();
		int32 Baked = 0;
		for (int32 Index = 0; Index < Others.Num(); ++Index)
		{
			FMeshDescription Theirs;
			if (!Others[Index] || !Others[Index]->CloneMeshDescription(Lod, Theirs))
			{
				Report += FString::Printf(TEXT(" LOD%d %s: no mesh description;"), Lod, *Names[Index].ToString());
				continue;
			}
			const TVertexAttributesConstRef<FVector3f> TheirPositions = Theirs.GetVertexPositions();
			const FVector3f TheirRoot = RootOf(Others[Index]);
			// Which of their vertices each of ours is: the same vertex ID when both meshes have the same vertices, else
			// the nearest (root-aligned) of theirs sharing one of its UVs (MetaHuman faces share a UV layout, not always
			// a vertex count: City Sample and Creator builds differ).
			constexpr float Reach = 6.f; // cm: no face differs more; farther is a different part sharing UV space
			TArray<FVertexID> Match;
			auto MatchByUV = [&]()
			{
				Match.Init(INDEX_NONE, Own.Vertices().GetArraySize());
				const TVertexInstanceAttributesConstRef<FVector2f> TheirUVs = Theirs.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate);
				const TVertexInstanceAttributesConstRef<FVector2f> OwnUVs = Own.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate);
				constexpr float Cell = 1.f / 2048.f;
				auto Key = [](const FVector2f& UV, int32 DX, int32 DY) { return FIntPoint(FMath::FloorToInt(UV.X / Cell) + DX, FMath::FloorToInt(UV.Y / Cell) + DY); };
				TMultiMap<FIntPoint, FVertexID> ByUV;
				for (const FVertexInstanceID Instance : Theirs.VertexInstances().GetElementIDs())
				{
					ByUV.Add(Key(TheirUVs[Instance], 0, 0), Theirs.GetVertexInstanceVertex(Instance));
				}
				for (const FVertexID Vertex : Own.Vertices().GetElementIDs())
				{
					const FVector3f Here = OwnPositions[Vertex] - OwnRoot;
					float Best = Reach;
					for (const FVertexInstanceID Instance : Own.GetVertexVertexInstanceIDs(Vertex))
					{
						for (int32 DX = -1; DX <= 1; ++DX)
						{
							for (int32 DY = -1; DY <= 1; ++DY)
							{
								TArray<FVertexID> Candidates;
								ByUV.MultiFind(Key(OwnUVs[Instance], DX, DY), Candidates);
								for (const FVertexID Candidate : Candidates)
								{
									const float Distance = FVector3f::Dist(TheirPositions[Candidate] - TheirRoot, Here);
									if (Distance < Best)
									{
										Best = Distance;
										Match[Vertex] = Candidate;
									}
								}
							}
						}
					}
				}
			};
			int32 Matched = 0, Far = 0;
			float Largest = 0.f;
			auto Measure = [&]()
			{
				Matched = Far = 0;
				Largest = 0.f;
				for (const FVertexID Vertex : Own.Vertices().GetElementIDs())
				{
					if (Match[Vertex] != INDEX_NONE)
					{
						const float D = FVector3f::Dist(TheirPositions[Match[Vertex]] - TheirRoot, OwnPositions[Vertex] - OwnRoot);
						Largest = FMath::Max(Largest, D);
						Far += D > Reach ? 1 : 0;
						++Matched;
					}
				}
				return float(Matched) >= 0.9f * Own.Vertices().Num() && Far <= Matched / 100;
			};
			// The same vertex IDs when both meshes have the same vertices (and that lines up), else by UV (MetaHuman faces
			// share a UV layout, not always a vertex count: City Sample and Creator builds differ).
			bool bGood = false;
			if (Theirs.Vertices().Num() == Own.Vertices().Num())
			{
				Match.Init(INDEX_NONE, Own.Vertices().GetArraySize());
				for (const FVertexID Vertex : Own.Vertices().GetElementIDs())
				{
					Match[Vertex] = Vertex;
				}
				bGood = Measure();
			}
			if (!bGood)
			{
				MatchByUV();
				bGood = Measure();
			}
			const float MatchedShare = float(Matched) / FMath::Max(1, Own.Vertices().Num());
			if (!bGood)
			{
				Report += FString::Printf(TEXT(" LOD%d %s: rejected (matched %.0f%%, %d far);"), Lod, *Names[Index].ToString(), MatchedShare * 100.f, Far);
				continue;
			}
			const FName Morph(*(TEXT("Blend_") + Names[Index].ToString()));
			Attributes.UnregisterMorphTargetAttribute(Morph);
			Attributes.RegisterMorphTargetAttribute(Morph, false);
			TVertexAttributesRef<FVector3f> Delta = Attributes.GetVertexMorphPositionDelta(Morph);
			for (const FVertexID Vertex : Own.Vertices().GetElementIDs())
			{
				Delta[Vertex] = Match[Vertex] == INDEX_NONE ? FVector3f::ZeroVector
					: (TheirPositions[Match[Vertex]] - TheirRoot) - (OwnPositions[Vertex] - OwnRoot);
			}
			if (Lod == 0)
			{
				Report += FString::Printf(TEXT(" %s ok (matched %.0f%%, max %.1f cm);"), *Names[Index].ToString(), MatchedShare * 100.f, Largest);
			}
			++Baked;
		}
		if (Baked > 0)
		{
			Face->CreateMeshDescription(Lod, MoveTemp(Own));
			USkeletalMesh::FCommitMeshDescriptionParams Params;
			Params.bMarkPackageDirty = true;
			Face->CommitMeshDescription(Lod, Params);
			Changed.Add(Lod);
		}
	}
	if (Changed.Num())
	{
		Face->Build();
		Face->PostEditChange();
	}
	Report = FString::Printf(TEXT("%s: LODs %s;%s"), *Face->GetName(), *FString::JoinBy(Changed, TEXT(","), [](int32 L) { return FString::FromInt(L); }), *Report);
	UE_LOG(LogShinobiCoop, Display, TEXT("%s"), *Report);
#endif
	return Report;
}
