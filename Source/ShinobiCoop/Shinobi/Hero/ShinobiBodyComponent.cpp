#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiCoop.h"
#include "Components/LODSyncComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	if (MutableLook)
	{
		MutableLook->Cancel(); // a made character's build must not land on this body
	}
	// Retargeter first: a new mesh re-initialises the anim instance, which reads it.
	Retargeter = Found;
	SetSkeletalMeshAsset(Body);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	UpdateBody(FShinobiAppearance()); // a roster body is worn as made: no creator shapes or height
	FittingName.Reset();
	EmptyOverrideMaterials(); // the last made body's skin tint
	WearStaticHead(nullptr);
	WearMetaHumanParts(MetaHumanClassPath);
	return true;
}

bool UShinobiBodyComponent::IsMutableBuildPending() const
{
	return MutableLook && MutableLook->IsPending();
}

namespace
{
	/** The City Sample crowd's retargeter (made for the AnimGen Londoners): the Mutable bodies share metahuman_base_skel. */
	UIKRetargeter* LoadCrowdRetargeter()
	{
		return LoadObject<UIKRetargeter>(nullptr, TEXT("/Game/Shinobi/AnimGen/RTG_MannyToCrowd.RTG_MannyToCrowd"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

bool UShinobiBodyComponent::WearAppearance(const FShinobiAppearance& Look)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Catalog || !LoadCrowdRetargeter() || !GetOwner())
	{
		return false;
	}
	// The body and its wardrobe come from one Mutable build (CO_Shinobi). Nothing changes until it lands (DressBuilt), so
	// a roster body keeps its own retargeter and head meanwhile, and stays if the build can't be made.
	if (!MutableLook)
	{
		MutableLook = NewObject<UShinobiMutableLook>(this);
		MutableLook->OnBuilt.BindWeakLambda(this, [this](USkeletalMesh* Built) { DressBuilt(Built); });
	}
	PendingLook = Look;
	return MutableLook->Request(Look);
}

void UShinobiBodyComponent::DressBuilt(USkeletalMesh* Built)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UIKRetargeter* Found = LoadCrowdRetargeter();
	if (!Built || !Catalog || !Found || !GetOwner())
	{
		return; // what is worn stays
	}
	if (!IsRegistered() || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return; // the wearer left (its world is gone) while this built
	}
	const FShinobiAppearance& Look = PendingLook;
	const FShinobiCreatorFace* FacePick = Catalog->FindFace(Look.Sex, Look.Face);
	// Retargeter first: a new mesh re-initialises the anim instance, which reads it. The Own build is a roster MetaHuman's
	// body: its own retargeter.
	UIKRetargeter* OwnRetargeter = Look.Build == EShinobiBuild::Own && FacePick ? FacePick->Retargeter.LoadSynchronous() : nullptr;
	Retargeter = OwnRetargeter ? OwnRetargeter : Found;
	SetSkeletalMeshAsset(Built);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	FittingName = UShinobiCreatorCatalog::FittingName(Look.Sex, FMath::Min(Look.Build, EShinobiBuild::Over));
	WearStaticHead(nullptr);
	WearMetaHumanParts(nullptr);
	EmptyOverrideMaterials(); // the last look's skin tint and colours
	ColourPieces(Look);
	if (const FShinobiCreatorHead* Head = Catalog->FindHead(Look.Head))
	{
		// A roster head: its own mesh, no MetaHuman face or grooms, and the body's skin tinted to its neck.
		WearStaticHead(Head->Mesh.LoadSynchronous());
		WornHeadTint = Head->SkinTint;
		ApplySkinTone(Look, WornHeadTint);
		ApplySkinLayers(Look);
		Worn = Look;
		UpdateBody(Look);
		UE_LOG(LogShinobiCoop, Log, TEXT("%s wears its Mutable look (%s)"), *GetNameSafe(GetOwner()), *Look.ToCode());
		OnBodyBuilt.Broadcast();
		return;
	}
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
	if (FacePick && !FacePick->MetaHuman.IsNull())
	{
		// A roster MetaHuman's face: its blueprint's parts, as on its roster body (its Fab hair needs their settings).
		WearMetaHumanParts(*FacePick->MetaHuman.ToString());
		Worn = Look;
		UpdateBody(Look);
	}
	else if (FacePick)
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
	WornHeadTint = FLinearColor::White;
	ApplySkinTone(Look, WornHeadTint);
	ApplySkinLayers(Look);
	UE_LOG(LogShinobiCoop, Log, TEXT("%s wears its Mutable look (%s)"), *GetNameSafe(GetOwner()), *Look.ToCode());
	OnBodyBuilt.Broadcast();
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
	if (IsMutableBuildPending())
	{
		PendingLook.BodyShape = Look.BodyShape;
	}
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
	if (IsMutableBuildPending())
	{
		PendingLook.FaceShape = Look.FaceShape;
		FMemory::Memcpy(PendingLook.BlendWeight, Look.BlendWeight, sizeof(Look.BlendWeight));
	}
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

FLinearColor UShinobiBodyComponent::SkinToneMultiplier(uint8 Tone)
{
	return Tone <= 128 ? FMath::Lerp(ShinobiMutable::SkinToneDark, FLinearColor::White, Tone / 128.f)
		: FMath::Lerp(FLinearColor::White, ShinobiMutable::SkinToneLight, (Tone - 128) / 127.f);
}

void UShinobiBodyComponent::UpdateSkin(const FShinobiAppearance& Look)
{
	// (and onto a look still building, so its landing doesn't bring the old skin back)
	for (FShinobiAppearance* Kept : { &Worn, IsMutableBuildPending() ? &PendingLook : nullptr })
	{
		if (Kept)
		{
			Kept->SkinTone = Look.SkinTone;
			Kept->MakeUp = Look.MakeUp;
			Kept->Freckles = Look.Freckles;
			Kept->Tattoos = Look.Tattoos;
			FMemory::Memcpy(Kept->SlotColour, Look.SlotColour, sizeof(Look.SlotColour));
		}
	}
	ApplySkinTone(Look, WornHeadTint);
	ApplySkinLayers(Look);
	ColourPieces(Look);
}

void UShinobiBodyComponent::ApplySkinTone(const FShinobiAppearance& Look, const FLinearColor& HeadTint)
{
	const FLinearColor Tone = SkinToneMultiplier(Look.SkinTone);
	auto Set = [](UMeshComponent* Mesh, const TCHAR* Parameter, const FLinearColor& Value)
	{
		for (int32 Index = 0; Mesh && Index < Mesh->GetNumMaterials(); ++Index)
		{
			FLinearColor Unused;
			const UMaterialInterface* Material = Mesh->GetMaterial(Index);
			if (Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Unused))
			{
				Mesh->CreateDynamicMaterialInstance(Index)->SetVectorParameterValue(Parameter, Value);
			}
		}
	};
	Set(this, ShinobiMutable::SkinTintParameter, HeadTint * Tone);
	Set(StaticHead, ShinobiMutable::StaticHeadTintParameter, Tone);
	for (UActorComponent* Part : MetaHumanParts)
	{
		if (Part && Part->GetName().StartsWith(TEXT("Face")))
		{
			Set(Cast<UMeshComponent>(Part), ShinobiMutable::SkinTintParameter, Tone);
		}
	}
}

void UShinobiBodyComponent::ApplySkinLayers(const FShinobiAppearance& Look)
{
	const UDataTable* Designs = LoadObject<UDataTable>(nullptr, ShinobiMutable::TattoosPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const UDataTable* Regions = LoadObject<UDataTable>(nullptr, ShinobiMutable::TattooRegionsPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const TArray<FName> DesignRows = Designs ? Designs->GetRowNames() : TArray<FName>();
	const TArray<FName> RegionRows = Regions ? Regions->GetRowNames() : TArray<FName>();
	for (int32 Index = 0; Index < GetNumMaterials(); ++Index)
	{
		float Unused;
		const UMaterialInterface* Material = GetMaterial(Index);
		if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Tattoo 1 Opacity")), Unused))
		{
			continue; // not skin (or not M_ShinobiSkin)
		}
		UMaterialInstanceDynamic* Skin = CreateDynamicMaterialInstance(Index);
		for (int32 Slot = 0; Slot < ShinobiMutable::MaxTattoos; ++Slot)
		{
			const FString Name = FString::Printf(TEXT("Tattoo %d"), Slot + 1);
			const FShinobiTattoo* Tattoo = Look.Tattoos.IsValidIndex(Slot) ? &Look.Tattoos[Slot] : nullptr;
			const FShinobiTattooDesign* Design = Tattoo && DesignRows.IsValidIndex(Tattoo->Design)
				? Designs->FindRow<FShinobiTattooDesign>(DesignRows[Tattoo->Design], TEXT("ApplySkinLayers")) : nullptr;
			const FShinobiTattooRegion* Region = Tattoo && RegionRows.IsValidIndex(Tattoo->Region)
				? Regions->FindRow<FShinobiTattooRegion>(RegionRows[Tattoo->Region], TEXT("ApplySkinLayers")) : nullptr;
			UTexture2D* Texture = Design && Region ? Design->Texture.LoadSynchronous() : nullptr;
			Skin->SetScalarParameterValue(*(Name + TEXT(" Opacity")), Texture ? 1.f : 0.f);
			if (!Texture)
			{
				continue;
			}
			// Nudged up to a region's size either way, turned a full turn over the byte, sized 0.5x to 1.5x.
			const FVector2D Centre = Region->Centre + FVector2D(Tattoo->OffsetU, Tattoo->OffsetV) / 127.0 * Region->Scale;
			Skin->SetTextureParameterValue(*Name, Texture);
			Skin->SetVectorParameterValue(*(Name + TEXT(" Offset")), FLinearColor(Centre.X, Centre.Y, 0.f, 0.f));
			Skin->SetScalarParameterValue(*(Name + TEXT(" Rotation")), Region->Rotation + Tattoo->Rotation / 255.f);
			Skin->SetScalarParameterValue(*(Name + TEXT(" Scale")), Region->Scale * (0.5f + Tattoo->Scale / 255.f));
		}
	}
	// Make-up and freckles: a MetaHuman face's skin (static heads keep their own painted faces).
	const UDataTable* MakeUps = LoadObject<UDataTable>(nullptr, ShinobiMutable::MakeUpPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const TArray<FName> MakeUpRows = MakeUps ? MakeUps->GetRowNames() : TArray<FName>();
	const FShinobiMakeUpRow* MakeUp = Look.MakeUp > 0 && MakeUpRows.IsValidIndex(Look.MakeUp) ? MakeUps->FindRow<FShinobiMakeUpRow>(MakeUpRows[Look.MakeUp], TEXT("ApplySkinLayers")) : nullptr;
	UTexture2D* Freckles = Look.Freckles > 0 ? LoadObject<UTexture2D>(nullptr, *ShinobiMutable::FrecklesPath(Look.Freckles), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	for (UActorComponent* Part : MetaHumanParts)
	{
		UMeshComponent* Face = Part && Part->GetName().StartsWith(TEXT("Face")) ? Cast<UMeshComponent>(Part) : nullptr;
		for (int32 Index = 0; Face && Index < Face->GetNumMaterials(); ++Index)
		{
			float Unused;
			const UMaterialInterface* Material = Face->GetMaterial(Index);
			if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Lips Makeup Opacity")), Unused))
			{
				continue;
			}
			UMaterialInstanceDynamic* Skin = Face->CreateDynamicMaterialInstance(Index);
			auto Paint = [Skin](const TCHAR* Layer, const TSoftObjectPtr<UTexture2D>* Mask, const FLinearColor& Colour, float Opacity)
			{
				UTexture2D* Texture = Mask ? Mask->LoadSynchronous() : nullptr;
				Skin->SetScalarParameterValue(*(FString(Layer) + TEXT(" Opacity")), Texture ? Opacity : 0.f);
				if (Texture)
				{
					Skin->SetTextureParameterValue(Layer, Texture);
					Skin->SetVectorParameterValue(*(FString(Layer) + TEXT(" Colour")), Colour);
				}
			};
			Paint(TEXT("Eye Makeup"), MakeUp ? &MakeUp->Eye : nullptr, MakeUp ? MakeUp->EyeColour : FLinearColor::Black, 0.85f);
			Paint(TEXT("Lips Makeup"), MakeUp ? &MakeUp->Lips : nullptr, MakeUp ? MakeUp->LipsColour : FLinearColor::Black, 0.8f);
			Paint(TEXT("Blush Makeup"), MakeUp ? &MakeUp->Blush : nullptr, MakeUp ? MakeUp->BlushColour : FLinearColor::Black, 0.5f);
			Skin->SetScalarParameterValue(TEXT("Freckles Opacity"), Freckles ? 0.6f : 0.f);
			if (Freckles)
			{
				Skin->SetTextureParameterValue(TEXT("Freckles"), Freckles);
				Skin->SetVectorParameterValue(TEXT("Freckles Colour"), ShinobiMutable::FrecklesColour);
			}
		}
	}
}

void UShinobiBodyComponent::ColourPieces(const FShinobiAppearance& Look)
{
	const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const TArray<FName> Colours = Palette ? Palette->GetRowNames() : TArray<FName>();
	const bool bFemale = Look.Sex == EShinobiSex::Female;
	for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
	{
		const EShinobiWardrobeSlot Slot = EShinobiWardrobeSlot(Index);
		const uint8 Colour = Look.SlotColour[Index];
		const bool bColoured = Colour > 0 && Colours.IsValidIndex(Colour);
		const FShinobiWardrobeRow* Row = UShinobiMutableLook::FindRow(Slot, Look.Slot(Slot));
		const USkeletalMesh* Piece = Row ? (bFemale ? Row->FemaleMesh : Row->MaleMesh).LoadSynchronous() : nullptr;
		const USkeletalMesh* Built = GetSkeletalMeshAsset();
		if (!Piece || !Built)
		{
			continue;
		}
		const FLinearColor Value = bColoured ? Palette->FindRow<FShinobiPaletteRow>(Colours[Colour], TEXT("ColourPieces"))->Colour : FLinearColor::White;
		const TArray<TSoftObjectPtr<UMaterialInterface>>& Tinted = bFemale ? Row->FemaleTinted : Row->MaleTinted;
		for (int32 Material = 0; Material < FMath::Min(Tinted.Num(), Piece->GetMaterials().Num()); ++Material)
		{
			UMaterialInterface* Copy = Tinted[Material].LoadSynchronous();
			UMaterialInterface* Own = Piece->GetMaterials()[Material].MaterialInterface;
			// The built body keeps the piece's materials: those sections take the colour (or, Original, their own again).
			for (int32 Section = 0; Copy && Own && Section < Built->GetMaterials().Num(); ++Section)
			{
				if (Built->GetMaterials()[Section].MaterialInterface != Own)
				{
					continue;
				}
				if (!bColoured)
				{
					SetMaterial(Section, Own);
					continue;
				}
				UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(GetMaterial(Section));
				if (!Dynamic || Dynamic->Parent != Copy)
				{
					Dynamic = UMaterialInstanceDynamic::Create(Copy, this);
					SetMaterial(Section, Dynamic);
				}
				for (const FName Parameter : Row->ColourParameters)
				{
					Dynamic->SetVectorParameterValue(Parameter, Value);
				}
			}
		}
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
		StaticHead->SetCustomDepthStencilValue(CustomDepthStencilValue); // the outline, as on the body
		StaticHead->SetRenderCustomDepth(bRenderCustomDepth);
		StaticHead->RegisterComponent();
	}
	StaticHead->EmptyOverrideMaterials(); // (the last head's tinted instances)
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
	// A copy of the blueprint's component, under its blueprint name where free (the last look's parts hold theirs until
	// garbage collection), not registered yet. Named: what each blueprint name became (LODSync finds components by name).
	TMap<FName, FName> Named;
	auto Copy = [this, &Named](const UActorComponent* Template, FName Var) -> UActorComponent*
	{
		const FName Name = FindObjectFast<UObject>(GetOwner(), Var) ? MakeUniqueObjectName(GetOwner(), Template->GetClass(), Var) : Var;
		UActorComponent* Part = NewObject<UActorComponent>(GetOwner(), Template->GetClass(), Name, RF_Transient, const_cast<UActorComponent*>(Template));
		MetaHumanParts.Add(Part);
		Named.Add(Var, Name);
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
					// A bundled groom's LODs alternate strands and cards (cards at LODs 1 and 3 only): with strands drawn, the
					// distance LODs of cards drew nothing and the hair vanished from a few metres.
					if (UGroomComponent* StrandsGroom = Cast<UGroomComponent>(Placed))
					{
						StrandsGroom->SetForcedLOD(0);
					}
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
			const FName* Placed = Named.Find(Entry.Name);
			Entry.Name = Entry.Name == Body ? GetFName() : Placed ? *Placed : Entry.Name;
			// A Fab strands hair is bound to the face's LOD0 only: its binding matches the head skin by material, and the
			// face's lower LODs skin the head with other materials, so the hair vanished whenever the face dropped a LOD.
			if (StrandsFace && Entry.Name == StrandsFace->GetFName())
			{
				Entry.SyncOption = ESyncOption::Disabled;
			}
		}
		if (StrandsFace)
		{
			StrandsFace->SetForcedLOD(1); // (1 = LOD0)
		}
		// (the custom LOD mappings, e.g. the grooms' cards LODs, are keyed by the same names)
		TMap<FName, FLODMappingData> Mappings;
		for (const TPair<FName, FLODMappingData>& Mapping : Sync->CustomLODMapping)
		{
			const FName* Placed = Named.Find(Mapping.Key);
			Mappings.Add(Mapping.Key == Body ? GetFName() : Placed ? *Placed : Mapping.Key, Mapping.Value);
		}
		Sync->CustomLODMapping = MoveTemp(Mappings);
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
