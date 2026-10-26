#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiCoop.h"
#include "Components/LODSyncComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"

UShinobiBodyComponent::UShinobiBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule and the driver do the colliding
}

bool UShinobiBodyComponent::Wear(const TCHAR* MeshPath, const TCHAR* RetargeterPath, const TCHAR* MetaHumanClassPath)
{
	// Quiet: a character whose model isn't made yet is expected.
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UIKRetargeter* Found = Body ? LoadObject<UIKRetargeter>(nullptr, RetargeterPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Found)
	{
		return false;
	}
	// Retargeter first: a new mesh re-initialises the anim instance, which reads it.
	Retargeter = Found;
	SetSkeletalMeshAsset(Body);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	UpdateBody(FShinobiAppearance()); // a roster body is worn as made: no creator shapes or height
	FittingName.Reset();
	WearStaticHead(nullptr);
	WearMetaHumanParts(MetaHumanClassPath);
	return true;
}

bool UShinobiBodyComponent::WearAppearance(const FShinobiAppearance& Look)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const FShinobiCreatorFrame* Frame = Catalog ? Catalog->FindFrame(Look.Sex, Look.Build) : nullptr;
	USkeletalMesh* Body = Frame ? Frame->Body.LoadSynchronous() : nullptr;
	// The City Sample crowd's retargeter (made for the AnimGen Londoners): every frame shares metahuman_base_skel.
	UIKRetargeter* Found = Body ? LoadObject<UIKRetargeter>(nullptr, TEXT("/Game/Shinobi/AnimGen/RTG_MannyToCrowd.RTG_MannyToCrowd"), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Found || !GetOwner())
	{
		return false;
	}
	Retargeter = Found;
	SetSkeletalMeshAsset(Body);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	FittingName = UShinobiCreatorCatalog::FittingName(Look.Sex, Look.Build);
	WearStaticHead(nullptr);
	WearMetaHumanParts(nullptr);
	// A part on the owner under a fresh name (the last look's parts may still hold theirs until garbage collection).
	auto Place = [this](UClass* Class, const TCHAR* Name, USceneComponent* Parent) -> USceneComponent*
	{
		USceneComponent* Part = NewObject<USceneComponent>(GetOwner(), Class, MakeUniqueObjectName(GetOwner(), Class, Name), RF_Transient);
		Part->SetupAttachment(Parent);
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Part))
		{
			Primitive->SetCustomDepthStencilValue(CustomDepthStencilValue); // the outline, as on the body
			Primitive->SetRenderCustomDepth(bRenderCustomDepth);
		}
		MetaHumanParts.Add(Part);
		return Part;
	};
	if (const FShinobiCreatorFace* FacePick = Catalog->FindFace(Look.Sex, Look.Face))
	{
		USkeletalMeshComponent* Face = CastChecked<USkeletalMeshComponent>(Place(USkeletalMeshComponent::StaticClass(), TEXT("Face"), this));
		Face->SkinCacheUsage.Init(ESkinCacheUsage::Enabled, 8); // grooms follow it through the skin cache (WearMetaHumanParts)
		Face->SetSkeletalMeshAsset(FacePick->Mesh.LoadSynchronous());
		Face->SetDisablePostProcessBlueprint(true);
		Face->SetAnimInstanceClass(UShinobiFaceShapeAnimInstance::StaticClass()); // body pose, RigLogic, face sliders
		Face->RegisterComponent();
		Worn = Look;
		UpdateFace(Look);
		UpdateBody(Look);
		for (const TSoftObjectPtr<UGroomBindingAsset>& Own : FacePick->OwnGrooms)
		{
			if (UGroomBindingAsset* Binding = Own.LoadSynchronous())
			{
				UGroomComponent* Strands = CastChecked<UGroomComponent>(Place(UGroomComponent::StaticClass(), *Binding->GetGroom()->GetName(), Face));
				Strands->SetGroomAsset(Binding->GetGroom(), Binding);
				Strands->PrecachePSOs(); // (SetGroomAsset doesn't; a packaged build ensures, and stalls on the first one)
				Strands->RegisterComponent();
			}
		}
		const TPair<EShinobiGroomSlot, uint8> Grooms[] = {{EShinobiGroomSlot::Hair, Look.Hair}, {EShinobiGroomSlot::Beard, Look.Beard}, {EShinobiGroomSlot::Mustache, Look.Mustache}};
		for (const TPair<EShinobiGroomSlot, uint8>& Pick : Grooms)
		{
			const FShinobiCreatorGroom* Groom = Pick.Value != FShinobiAppearance::None ? Catalog->FindGroom(Look.Sex, Pick.Key, Pick.Value) : nullptr;
			if (!Groom)
			{
				continue;
			}
			UGroomComponent* Strands = CastChecked<UGroomComponent>(Place(UGroomComponent::StaticClass(), *Groom->Id.ToString(), Face));
			// The groom the binding was made for (a name can appear in several faces' folders as separate assets).
			UGroomBindingAsset* Binding = Catalog->LoadBinding(FacePick->Id, Groom->Id);
			UGroomAsset* Asset = Binding && Binding->GetGroom() ? Binding->GetGroom() : Groom->Groom.LoadSynchronous();
			Strands->SetGroomAsset(Asset, Binding);
			// The crowd grooms' own LODs decide strands or cards (forcing cards hid Hair_S_Messy, whose cards are switched off).
			Strands->PrecachePSOs();
			Strands->RegisterComponent();
		}
	}
	const TPair<EShinobiWardrobeSlot, uint8> Pieces[] = {{EShinobiWardrobeSlot::Top, Look.Top}, {EShinobiWardrobeSlot::Bottom, Look.Bottom}, {EShinobiWardrobeSlot::Shoes, Look.Shoes}};
	for (const TPair<EShinobiWardrobeSlot, uint8>& Pick : Pieces)
	{
		const FShinobiCreatorStyle* Style = Pick.Value != FShinobiAppearance::None ? Catalog->FindStyle(Look.Sex, Pick.Key, Pick.Value) : nullptr;
		USkeletalMesh* Mesh = Style && Style->ByBuild.IsValidIndex(uint8(Look.Build)) ? Style->ByBuild[uint8(Look.Build)].LoadSynchronous() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		USkeletalMeshComponent* Piece = CastChecked<USkeletalMeshComponent>(Place(USkeletalMeshComponent::StaticClass(), *Style->Id.ToString(), this));
		Piece->SetSkeletalMeshAsset(Mesh);
		Piece->SetLeaderPoseComponent(this);
		Piece->RegisterComponent();
	}
	return true;
}

namespace
{
	/**
	 * A body slider's bones and how far each scales at value 1 (MetaHuman bones run along X, so Y and Z are girth). Limb
	 * girth goes through the twist and corrective helpers (leaves); a chain bone passes the inverse to its children
	 * (SetBoneShape), so only the skin it carries changes. Every _l bone has its _r twin. Height is separate: the
	 * whole character scales (body, worn clothes and face) and so does its capsule.
	 * ponytail: hand-tuned amounts (phase 0 spike); clamp more if clothes clip at the extremes.
	 */
	struct FBodyPart
	{
		int32 Slider;
		const TCHAR* Bone;
		FVector Scale; // at value 1: 1 + Scale per axis
	};

	const FName BodySliderNames[] = {
		TEXT("Height"), TEXT("Shoulders"), TEXT("Chest"), TEXT("Waist"), TEXT("Hips"), TEXT("Neck"),
		TEXT("UpperArms"), TEXT("Forearms"), TEXT("Thighs"), TEXT("Calves"),
	};
	constexpr float HeightReach = 0.08f; // +-8%

	const FBodyPart BodyParts[] = {
		{1, TEXT("clavicle_l"), FVector(0.18, 0.0, 0.0)},
		{2, TEXT("spine_04"), FVector(0.0, 0.2, 0.24)},
		{2, TEXT("spine_05"), FVector(0.0, 0.2, 0.24)},
		{2, TEXT("clavicle_pec_l"), FVector(0.0, 0.45, 0.45)},
		{2, TEXT("spine_04_latissimus_l"), FVector(0.0, 0.45, 0.45)},
		{3, TEXT("spine_01"), FVector(0.0, 0.35, 0.35)},
		{3, TEXT("spine_02"), FVector(0.0, 0.35, 0.35)},
		{3, TEXT("spine_03"), FVector(0.0, 0.2, 0.2)},
		{4, TEXT("pelvis"), FVector(0.0, 0.12, 0.12)},
		{5, TEXT("neck_01"), FVector(0.0, 0.3, 0.3)},
		{5, TEXT("neck_02"), FVector(0.0, 0.25, 0.25)},
		{6, TEXT("upperarm_twist_01_l"), FVector(0.0, 0.4, 0.4)},
		{6, TEXT("upperarm_twist_02_l"), FVector(0.0, 0.4, 0.4)},
		{6, TEXT("upperarm_correctiveRoot_l"), FVector(0.0, 0.4, 0.4)},
		{7, TEXT("lowerarm_twist_01_l"), FVector(0.0, 0.35, 0.35)},
		{7, TEXT("lowerarm_twist_02_l"), FVector(0.0, 0.35, 0.35)},
		{7, TEXT("lowerarm_correctiveRoot_l"), FVector(0.0, 0.35, 0.35)},
		{8, TEXT("thigh_twist_01_l"), FVector(0.0, 0.22, 0.22)},
		{8, TEXT("thigh_twist_02_l"), FVector(0.0, 0.22, 0.22)},
		{8, TEXT("thigh_correctiveRoot_l"), FVector(0.0, 0.22, 0.22)},
		{9, TEXT("calf_twist_01_l"), FVector(0.0, 0.22, 0.22)},
		{9, TEXT("calf_twist_02_l"), FVector(0.0, 0.22, 0.22)},
		{9, TEXT("calf_correctiveRoot_l"), FVector(0.0, 0.22, 0.22)},
	};
}

TArray<FName> UShinobiBodyComponent::GetBodySliderNames()
{
	return TArray<FName>(BodySliderNames, UE_ARRAY_COUNT(BodySliderNames));
}

void UShinobiBodyComponent::UpdateBody(const FShinobiAppearance& Look)
{
	Worn.BodyShape = Look.BodyShape;
	auto Value = [&Look](int32 Slider) { return Look.BodyShape.IsValidIndex(Slider) ? Look.BodyShape[Slider] / 127.f : 0.f; };
	TMap<FName, FVector> Scales;
	for (const FBodyPart& Part : BodyParts)
	{
		const FString Bone = Part.Bone;
		for (const FString& Side : {Bone, Bone.EndsWith(TEXT("_l")) ? Bone.LeftChop(2) + TEXT("_r") : FString()})
		{
			if (!Side.IsEmpty())
			{
				Scales.FindOrAdd(FName(*Side), FVector::OneVector) += Part.Scale * Value(Part.Slider);
			}
		}
	}
	BoneShapes.Reset();
	for (const TPair<FName, FVector>& Scale : Scales)
	{
		SetBoneShape(Scale.Key, Scale.Value);
	}
	// Height: this body (with everything attached to it) about its feet, and the character's capsule to match, the
	// driver mesh moved so the feet stay on the capsule's floor.
	const float Height = 1.f + HeightReach * Value(0);
	SetRelativeScale3D(FVector(Height));
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const ACharacter* Defaults = Character->GetClass()->GetDefaultObject<ACharacter>();
		const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Height;
		Character->GetCapsuleComponent()->SetCapsuleHalfHeight(HalfHeight);
		FVector Driver = Character->GetMesh()->GetRelativeLocation();
		Driver.Z = -HalfHeight;
		Character->GetMesh()->SetRelativeLocation(Driver);
	}
}

void UShinobiBodyComponent::UpdateFace(const FShinobiAppearance& Look)
{
	USkeletalMeshComponent* Face = nullptr;
	for (UActorComponent* Part : MetaHumanParts)
	{
		if (Part && Part->GetName().StartsWith(TEXT("Face")))
		{
			Face = Cast<USkeletalMeshComponent>(Part);
		}
	}
	UShinobiFaceShapeAnimInstance* Shape = Face ? Cast<UShinobiFaceShapeAnimInstance>(Face->GetAnimInstance()) : nullptr;
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Shape || !Catalog)
	{
		return;
	}
	Worn.FaceShape = Look.FaceShape;
	const TArray<FName> Sliders = UShinobiFaceShapeAnimInstance::GetSliderNames();
	for (int32 Index = 0; Index < Sliders.Num(); ++Index)
	{
		Shape->SetFaceShape(Sliders[Index], Look.FaceShape.IsValidIndex(Index) ? Look.FaceShape[Index] / 127.f : 0.f);
	}
	// A face with baked blend morphs (Scripts/setup_creator.py blendmorphs) blends by those; others move their joints.
	Face->ClearMorphTargets();
	for (int32 Slot = 0; Slot < UShinobiFaceShapeAnimInstance::MaxBlends; ++Slot)
	{
		Worn.BlendFace[Slot] = Look.BlendFace[Slot];
		Worn.BlendWeight[Slot] = Look.BlendWeight[Slot];
		const FShinobiCreatorFace* Target = Look.BlendFace[Slot] != FShinobiAppearance::None ? Catalog->FindFace(Worn.Sex, Look.BlendFace[Slot]) : nullptr;
		const float Weight = Look.BlendWeight[Slot] / 255.f;
		const FName Morph = Target ? FName(*(TEXT("Blend_") + Target->Id.ToString())) : NAME_None;
		const bool bMorph = Target && Face->GetSkeletalMeshAsset() && Face->GetSkeletalMeshAsset()->FindMorphTarget(Morph);
		if (bMorph)
		{
			Face->SetMorphTarget(Morph, Weight);
		}
		Shape->SetFaceBlend(Slot, Target && !bMorph ? Target->Mesh.LoadSynchronous() : nullptr, Weight);
	}
}

void UShinobiBodyComponent::WearStaticHead(USkeletalMesh* Head)
{
	if (Head)
	{
		WearMetaHumanParts(nullptr);
	}
	if (!Head || !GetOwner())
	{
		if (StaticHead)
		{
			StaticHead->DestroyComponent();
			StaticHead = nullptr;
		}
		return;
	}
	if (!StaticHead)
	{
		StaticHead = NewObject<USkeletalMeshComponent>(GetOwner(), TEXT("StaticHead"), RF_Transient);
		StaticHead->SetupAttachment(this);
		StaticHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StaticHead->RegisterComponent();
	}
	StaticHead->SetSkeletalMeshAsset(Head);
	StaticHead->SetLeaderPoseComponent(this);
}

void UShinobiBodyComponent::SetBoneShape(FName Bone, const FVector& Scale)
{
	if (Scale.Equals(FVector::OneVector))
	{
		BoneShapes.Remove(Bone);
	}
	else
	{
		BoneShapes.Add(Bone, Scale);
	}
}

bool UShinobiBodyComponent::SetFaceShape(FName Slider, float Value)
{
	USkeletalMeshComponent* Face = nullptr;
	for (UActorComponent* Part : MetaHumanParts)
	{
		if (Part && Part->GetName().StartsWith(TEXT("Face")))
		{
			Face = Cast<USkeletalMeshComponent>(Part);
		}
	}
	if (!Face)
	{
		return false;
	}
	if (!Cast<UShinobiFaceShapeAnimInstance>(Face->GetAnimInstance()))
	{
		Face->SetDisablePostProcessBlueprint(true);
		Face->SetAnimInstanceClass(UShinobiFaceShapeAnimInstance::StaticClass());
	}
	UShinobiFaceShapeAnimInstance* Shape = Cast<UShinobiFaceShapeAnimInstance>(Face->GetAnimInstance());
	return Shape && Shape->SetFaceShape(Slider, Value);
}

void UShinobiBodyComponent::SetBodyMorph(FName Morph, float Weight)
{
	SetMorphTarget(Morph, Weight);
	for (const TWeakObjectPtr<USkinnedMeshComponent>& Follower : GetFollowerPoseComponents())
	{
		if (USkeletalMeshComponent* Follows = Cast<USkeletalMeshComponent>(Follower.Get()))
		{
			Follows->SetMorphTarget(Morph, Weight);
		}
	}
}

void UShinobiBodyComponent::WearMetaHumanParts(const TCHAR* ClassPath)
{
	for (UActorComponent* Part : MetaHumanParts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	MetaHumanParts.Reset();
	const UBlueprintGeneratedClass* Class = ClassPath ? LoadObject<UBlueprintGeneratedClass>(nullptr, ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Class || !Class->SimpleConstructionScript || !GetOwner())
	{
		UE_CLOG(ClassPath != nullptr, LogShinobiCoop, Warning, TEXT("%s: no MetaHuman blueprint %s"), *GetNameSafe(GetOwner()), ClassPath);
		return;
	}
	static const FName Body(TEXT("Body")), GroomAsset(TEXT("GroomAsset"));
	UBlueprintGeneratedClass* MutableClass = const_cast<UBlueprintGeneratedClass*>(Class);
	// A copy of the blueprint's component, under its blueprint name (LODSync finds its components by name), not registered yet.
	auto Copy = [this](const UActorComponent* Template, FName Var) -> UActorComponent*
	{
		const FName Name = FindObjectFast<UObject>(GetOwner(), Var) ? MakeUniqueObjectName(GetOwner(), Template->GetClass(), Var) : Var;
		UActorComponent* Part = NewObject<UActorComponent>(GetOwner(), Template->GetClass(), Name, RF_Transient, const_cast<UActorComponent*>(Template));
		MetaHumanParts.Add(Part);
		return Part;
	};
	USkinnedMeshComponent* StrandsFace = nullptr; // what a ShinobiStrands groom is bound to (see below)
	// The blueprint's tree (Root > Body > Face > grooms): this stands in for Root and Body.
	TFunction<void(const USCS_Node*, USceneComponent*)> Wear = [&](const USCS_Node* Node, USceneComponent* Parent)
	{
		const FName Var = Node->GetVariableName();
		const USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(MutableClass));
		USceneComponent* Placed = Parent;
		if (Var == Body)
		{
			Placed = this;
		}
		else if (Template)
		{
			// Grooms left empty (the slots the quality level bakes into the face texture: stubble, lashes, fuzz) are skipped.
			const FObjectProperty* Groom = FindFProperty<FObjectProperty>(Template->GetClass(), GroomAsset);
			if (!Groom || Groom->GetObjectPropertyValue_InContainer(Template))
			{
				Placed = CastChecked<USceneComponent>(Copy(Template, Var));
				Placed->SetupAttachment(Parent, Node->AttachToName);
				// Hair and brows draw their cards even up close: their strands render thin and washed out in our outdoor light
				// (the brows came out light brown), the cards dark and full. Stubble keeps its strands (it has no cards), and so
				// does a groom tagged ShinobiStrands (a Fab hair with untextured cards, Scripts/setup_metahuman.py fab_hair).
				if (Placed->ComponentHasTag(TEXT("ShinobiStrands")))
				{
					StrandsFace = Cast<USkinnedMeshComponent>(Parent);
				}
				else if (Var == TEXT("Hair") || Var == TEXT("Eyebrows"))
				{
					if (FBoolProperty* UseCards = FindFProperty<FBoolProperty>(Placed->GetClass(), TEXT("bUseCards")))
					{
						UseCards->SetPropertyValue_InContainer(Placed, true);
					}
				}
				// Grooms follow a skinned mesh through the GPU skin cache, which this project only gives meshes that ask
				// (r.SkinCache.DefaultBehavior=0): the face asks for every LOD.
				if (USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Placed))
				{
					Skinned->SkinCacheUsage.Init(ESkinCacheUsage::Enabled, 8);
				}
				// The outline is drawn where custom depth ends: the face and hair write the body's, so no seam shows at the neck.
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Placed))
				{
					Primitive->SetCustomDepthStencilValue(CustomDepthStencilValue);
					Primitive->SetRenderCustomDepth(bRenderCustomDepth);
					Primitive->PrecachePSOs(); // (a groom's isn't requested by its setup)
				}
				Placed->RegisterComponent();
				UE_LOG(LogShinobiCoop, Log, TEXT("%s wears MetaHuman part %s (%s) on %s"), *GetNameSafe(GetOwner()), *Placed->GetName(),
					*Template->GetClass()->GetName(), *GetNameSafe(Parent));
			}
		}
		for (const USCS_Node* Child : Node->GetChildNodes())
		{
			Wear(Child, Placed);
		}
	};
	const USCS_Node* LODSyncNode = nullptr;
	for (const USCS_Node* Root : Class->SimpleConstructionScript->GetRootNodes())
	{
		if (Cast<ULODSyncComponent>(Root->GetActualComponentTemplate(MutableClass)))
		{
			LODSyncNode = Root;
		}
		for (const USCS_Node* Child : Root->GetChildNodes()) // the actor's root: this stands in
		{
			Wear(Child, this);
		}
	}
	// LODSync last, once the parts it names exist: the grooms' cards LODs follow the face's (without it a groom can sit
	// on a LOD it has no geometry for). Its "Body" is this.
	if (LODSyncNode)
	{
		ULODSyncComponent* Sync = CastChecked<ULODSyncComponent>(Copy(LODSyncNode->GetActualComponentTemplate(MutableClass), LODSyncNode->GetVariableName()));
		for (FComponentSync& Entry : Sync->ComponentsToSync)
		{
			Entry.Name = Entry.Name == Body ? GetFName() : Entry.Name;
			// A Fab strands hair is bound to the face's LOD0 only: its binding matches the head skin by material, and the
			// face's lower LODs skin the head with other materials, so the hair vanished whenever the face dropped a LOD.
			if (StrandsFace && Entry.Name == FName(*StrandsFace->GetFName().GetPlainNameString()))
			{
				Entry.SyncOption = ESyncOption::Disabled;
			}
		}
		if (StrandsFace)
		{
			StrandsFace->SetForcedLOD(1); // (1 = LOD0)
		}
		if (FLODMappingData* Mapping = Sync->CustomLODMapping.Find(Body))
		{
			const FLODMappingData Moved = *Mapping;
			Sync->CustomLODMapping.Remove(Body);
			Sync->CustomLODMapping.Add(GetFName(), Moved);
		}
		Sync->RegisterComponent();
	}
}

bool UShinobiBodyComponent::WearCharacter(const FString& Name)
{
	const FString Dir = FString::Printf(TEXT("/Game/Shinobi/Characters/%s/"), *Name);
	return Wear(*FString::Printf(TEXT("%sSK_%s.SK_%s"), *Dir, *Name, *Name),
		*FString::Printf(TEXT("%sRTG_MannyTo%s.RTG_MannyTo%s"), *Dir, *Name, *Name));
}

void UShinobiBodyComponent::HideDriver()
{
	if (USkeletalMeshComponent* Driver = Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		Driver->SetVisibility(false);
		Driver->SetCastShadow(false);
		Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}
