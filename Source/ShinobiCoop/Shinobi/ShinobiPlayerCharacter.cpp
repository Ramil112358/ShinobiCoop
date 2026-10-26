#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Misc/PackageName.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/ShinobiLook.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Hero/ShinobiPhysicalReactionComponent.h"
#include "Shinobi/VFX/ShinobiSfx.h"
#include "TimerManager.h"
#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_Block.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/Abilities/GA_FireMeteor.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_IceMeteor.h"
#include "Shinobi/Abilities/GA_Kamui.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Revive.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Substitution.h"
#include "Shinobi/Abilities/GA_Dragon.h"
#include "Shinobi/Abilities/GA_Summons.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/Abilities/GA_Teleport.h"
#include "Shinobi/Abilities/GA_WaterDragon.h"
#include "Shinobi/Abilities/GA_WaterWall.h"
#include "Shinobi/Abilities/ShinobiJutsuAbility.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"
#include "Shinobi/VFX/ShinobiSusanooComponent.h"
#include "Shinobi/Traversal/ShinobiTraversalComponent.h"
#include "MotionWarpingComponent.h"
#include "ClothingSimulationInteractor.h"
#include "ClothingAssetBase.h"
#include "ChaosCloth/ChaosClothingSimulationInteractor.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	TAutoConsoleVariable<bool> CVarHero(
		TEXT("Shinobi.Hero"),
		true,
		TEXT("Players use the hero character (sub-project #2) when its assets exist; 0 = the plain Manny. Read when a player spawns."));

	TAutoConsoleVariable<FString> CVarOutfitCopySuffix(
		TEXT("Shinobi.OutfitCopySuffix"),
		TEXT(""),
		TEXT("Appended to the body's name in outfit copy asset names (tests point it at copies that don't exist)."));

	const TCHAR* const WhiteMaskPath = TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture");
}

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = {
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Blink::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(), UGA_Block::StaticClass(),
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
		UGA_Substitution::StaticClass(), UGA_Revive::StaticClass(),
		UGA_AlmightyPush::StaticClass(), UGA_Kamui::StaticClass(), UGA_KamuiExit::StaticClass(),
		UGA_Teleport::StaticClass(), UGA_TeleportJump::StaticClass(),
		UGA_EarthWall::StaticClass(), UGA_WaterWall::StaticClass(), UGA_FireMeteor::StaticClass(), UGA_IceMeteor::StaticClass(), UGA_WaterDragon::StaticClass(), UGA_Susanoo::StaticClass(),
		UGA_SummonPusho::StaticClass(), UGA_SummonOwl::StaticClass(), UGA_SummonMisha::StaticClass(), UGA_SummonPabu::StaticClass(),
		UGA_SummonDragon::StaticClass(), UGA_SummonCrimsonDragon::StaticClass(), UGA_DragonDismount::StaticClass(), UGA_CrimsonDragonDismount::StaticClass(), UGA_DragonDismountKey::StaticClass(), UGA_DragonBreath::StaticClass(),
	};
	StartupEffects.Add(UGE_SubstitutionRegen::StaticClass());

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	JumpMaxCount = 2; // double jump

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->JumpZVelocity = 600.f;
	Movement->AirControl = 0.35f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = FootArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;


	UseMannequinMesh();

	HeroMesh = CreateDefaultSubobject<UShinobiBodyComponent>(TEXT("HeroMesh"));
	HeroMesh->SetupAttachment(GetMesh());

	OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OutfitMesh"));
	OutfitMesh->SetupAttachment(HeroMesh);
	OutfitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OutfitMesh->SetVisibility(false);
	OutfitMesh->SetTeleportDistanceThreshold(150.f); // a blink, Substitution or respawn snaps the cloth along, not stretched

	NameTag = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);

	SurfaceFx = CreateDefaultSubobject<UShinobiSurfaceFxComponent>(TEXT("SurfaceFx"));
	SurfaceFx->SetupAttachment(RootComponent); // turns with the capsule onto walls

	Traversal = CreateDefaultSubobject<UShinobiTraversalComponent>(TEXT("Traversal"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// The guard's ribcage: a body-hugging Susanoo (Scripts/setup_susanoo_ribs.py) that snaps up and down quickly.
	Guard = CreateDefaultSubobject<UShinobiSusanooComponent>(TEXT("Guard"));
	Guard->StateTag = ShinobiTags::State_Blocking;
	Guard->Model = FSoftObjectPath(TEXT("/Game/Shinobi/Jutsu/SM_SusanooRibs.SM_SusanooRibs"));
	Guard->FadeInSeconds = 0.15f;
	Guard->FadeOutSeconds = 0.25f;
}

int32 AShinobiPlayerCharacter::FindRosterIndex(const FString& Name)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Roster); ++Index)
	{
		if (Name.Equals(Roster[Index].Name, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AShinobiPlayerCharacter::SetCharacterIndex(uint8 Index)
{
	CharacterIndex = Index;
	ApplyCharacter();
}

void AShinobiPlayerCharacter::SetAppearance(const FShinobiAppearance& Look)
{
	Appearance = Look;
	Appearance.Sanitize();
	CharacterIndex = CustomCharacter;
	ApplyCharacter();
}

int32 AShinobiPlayerCharacter::FindOutfitIndex(const FString& Name)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Outfits); ++Index)
	{
		if (Name.Equals(Outfits[Index].Name, ESearchCase::IgnoreCase))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AShinobiPlayerCharacter::SetOutfitIndex(uint8 Index)
{
	OutfitIndex = Index;
	ApplyOutfit();
}

namespace
{
	/** Where the Hips bone sits in the mesh's reference pose (component space); zero for a skeleton without one. */
	FVector RefHipsLocation(const USkeletalMesh* Mesh)
	{
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
		{
			if (Ref.GetBoneName(Bone).ToString().EndsWith(TEXT("Hips")))
			{
				return FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation();
			}
		}
		return FVector::ZeroVector;
	}
}

FString AShinobiPlayerCharacter::OutfitMeshPath(int32 Outfit, const FString& Body)
{
	const FString Name = FString::Printf(TEXT("SK_%s_%s%s"), Outfits[Outfit].Name, *Body, *CVarOutfitCopySuffix.GetValueOnGameThread());
	return FString::Printf(TEXT("/Game/Shinobi/Outfits/%s/%s.%s"), Outfits[Outfit].Name, *Name, *Name);
}

FString AShinobiPlayerCharacter::OutfitHidePath(int32 Outfit, const FString& Body)
{
	const FString Name = FString::Printf(TEXT("T_%s_%s%s_Hide"), Outfits[Outfit].Name, *Body, *CVarOutfitCopySuffix.GetValueOnGameThread());
	return FString::Printf(TEXT("/Game/Shinobi/Outfits/%s/%s.%s"), Outfits[Outfit].Name, *Name, *Name);
}

bool AShinobiPlayerCharacter::IsOutfitBuilt(int32 Outfit, int32 Body)
{
	if (Outfit <= 0)
	{
		return true;
	}
	if (Outfit >= UE_ARRAY_COUNT(Outfits) || Body < 0 || Body >= UE_ARRAY_COUNT(Roster))
	{
		return false;
	}
	return FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(OutfitMeshPath(Outfit, Body)));
}

bool AShinobiPlayerCharacter::DressOutfit(USkeletalMeshComponent& BodyMesh, USkeletalMeshComponent& Outfit, int32 OutfitIndex, const FString& BodyName)
{
	USkeletalMesh* Cloth = nullptr;
	UTexture* Mask = nullptr;
	if (OutfitIndex > 0 && OutfitIndex < UE_ARRAY_COUNT(Outfits))
	{
		// Quiet: an outfit that isn't imported yet is expected.
		Cloth = LoadObject<USkeletalMesh>(nullptr, *OutfitMeshPath(OutfitIndex, BodyName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		Mask = Cloth ? LoadObject<UTexture>(nullptr, *OutfitHidePath(OutfitIndex, BodyName), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		if (!Cloth)
		{
			static TSet<FString> Warned; // once per outfit and body
			const FString Pair = FString::Printf(TEXT("%s/%s"), Outfits[OutfitIndex].Name, *BodyName);
			if (!Warned.Contains(Pair))
			{
				Warned.Add(Pair);
				UE_LOG(LogShinobiCoop, Warning, TEXT("No %s copy fitted to %s (Scripts/character/build_outfits.ps1): wearing nothing"),
					Outfits[OutfitIndex].Name, *BodyName);
			}
		}
	}
	// The body hides what the copy covers (its M_Character opacity), and shows everything again without one.
	if (!Mask)
	{
		Mask = LoadObject<UTexture>(nullptr, WhiteMaskPath);
	}
	BodyMesh.EmptyOverrideMaterials(); // (a new body brings its own materials)
	for (int32 Slot = 0; Slot < BodyMesh.GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = BodyMesh.CreateDynamicMaterialInstance(Slot))
		{
			Material->SetTextureParameterValue(TEXT("HideMask"), Mask);
		}
	}
	const USkeletalMesh* Body = BodyMesh.GetSkeletalMeshAsset();
	Outfit.SetSkeletalMeshAsset(Cloth);
	Outfit.SetVisibility(Cloth != nullptr);
	if (!Cloth || !Body)
	{
		Outfit.SetLeaderPoseComponent(nullptr);
		return false;
	}
	// The outfit's bones take the body's component-space pose, so a body whose skeleton sits elsewhere in its FBX than the
	// outfit's would carry the outfit off by the difference: shift it back by the two hips' reference-pose offset.
	Outfit.SetRelativeLocation(RefHipsLocation(Cloth) - RefHipsLocation(Body));
	Outfit.SetLeaderPoseComponent(&BodyMesh);
	Outfit.ForceClothNextUpdateTeleportAndReset();
	return true;
}

void AShinobiPlayerCharacter::ApplyOutfit()
{
	if (!HeroMesh || !OutfitMesh)
	{
		return;
	}
	const FString& Frame = HeroMesh->GetFittingName();
	if (!DressOutfit(*HeroMesh, *OutfitMesh, OutfitIndex, Frame.IsEmpty() ? FString(Roster[WornBodyIndex].Name) : Frame))
	{
		return;
	}
	bOutfitGravityOverridden = false; // a new cloth simulation starts on world gravity
	RefreshOutline();
	// Scripts/smoke_m4.ps1 style check: both machines log what they see.
	UE_LOG(LogShinobiCoop, Log, TEXT("%s wears %s"), *GetName(), Outfits[OutfitIndex].Name);
}

void AShinobiPlayerCharacter::UpdateOutfitCloth()
{
	if (!OutfitMesh || !OutfitMesh->IsVisible())
	{
		return;
	}
	if (UClothingSimulationInteractor* Interactor = OutfitMesh->GetClothingSimulationInteractor())
	{
		// Wall-running and water-walking turn the character's gravity: the cloth hangs along it, not world down (each
		// cloth's own config: the simulation-wide override does nothing in 5.8).
		const FVector Down = GetCharacterMovement()->GetGravityDirection();
		const bool bOverride = !Down.Equals(FVector::DownVector, 0.01);
		if (bOverride || bOutfitGravityOverridden)
		{
			for (const UClothingAssetBase* Cloth : OutfitMesh->GetSkeletalMeshAsset()->GetMeshClothingAssets())
			{
				if (UChaosClothingInteractor* Chaos = Cloth ? Cast<UChaosClothingInteractor>(Interactor->GetClothingInteractor(Cloth->GetName())) : nullptr)
				{
					Chaos->SetGravity(1.f, bOverride, Down * FMath::Abs(GetCharacterMovement()->GetGravityZ()));
				}
			}
		}
		bOutfitGravityOverridden = bOverride;
	}
	bool bAsleep = false;
	if (FApp::CanEverRender())
	{
		const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
		const bool bFar = Viewer && Viewer->PlayerCameraManager
			&& FVector::Dist(Viewer->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) > OutfitClothRangeCm;
		bAsleep = bFar || !OutfitMesh->WasRecentlyRendered(1.f);
	}
	if (bAsleep != OutfitMesh->IsClothingSimulationSuspended())
	{
		if (bAsleep)
		{
			OutfitMesh->SuspendClothingSimulation();
		}
		else
		{
			OutfitMesh->ResumeClothingSimulation();
		}
	}
}

bool AShinobiPlayerCharacter::ApplyCharacter()
{
	if (!HeroMesh)
	{
		return false;
	}
	if (CharacterIndex == CustomCharacter)
	{
		if (HeroMesh->WearAppearance(Appearance))
		{
			UE_LOG(LogShinobiCoop, Log, TEXT("%s plays a made character (%s)"), *GetName(), *Appearance.ToCode());
			ApplyOutfit();
			return true;
		}
		UE_LOG(LogShinobiCoop, Warning, TEXT("%s: no creator catalog or frame (Scripts/setup_creator.py): shown as the hero"), *GetName());
	}
	for (const int32 Index : {int32(CharacterIndex), 0}) // the pick, else the hero
	{
		if (Index < UE_ARRAY_COUNT(Roster) && HeroMesh->Wear(Roster[Index].Mesh, Roster[Index].Retargeter, Roster[Index].MetaHuman))
		{
			WornBodyIndex = uint8(Index);
			// Scripts/smoke_m4.ps1 reads this on both machines to check the picks replicate.
			UE_LOG(LogShinobiCoop, Log, TEXT("%s plays %s%s"), *GetName(),
				CharacterIndex < UE_ARRAY_COUNT(Roster) ? Roster[CharacterIndex].Name : TEXT("an unknown pick"),
				Index == CharacterIndex ? TEXT("") : TEXT(" (not imported: shown as the hero)"));
			ApplyOutfit(); // the new body's skeleton: re-bind the outfit to it
			return true;
		}
	}
	return false;
}

void AShinobiPlayerCharacter::ApplyOutlineFor(const AShinobiCharacterBase* Viewer)
{
	Super::ApplyOutlineFor(Viewer);
	ShinobiLook::EnableOutline(OutfitMesh, OutlineStencilFor(Viewer)); // the cloak is its own mesh
}

void AShinobiPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShinobiPlayerCharacter, CharacterIndex);
	DOREPLIFETIME(AShinobiPlayerCharacter, Appearance);
	DOREPLIFETIME(AShinobiPlayerCharacter, OutfitIndex);
	DOREPLIFETIME_CONDITION(AShinobiPlayerCharacter, Loadout, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AShinobiPlayerCharacter, LastCombatTime, COND_OwnerOnly);
}

void AShinobiPlayerCharacter::ApplyHero()
{
	if (!CVarHero.GetValueOnGameThread() || !ApplyCharacter())
	{
		OutfitMesh->DestroyComponent();
		OutfitMesh = nullptr;
		HeroMesh->DestroyComponent();
		HeroMesh = nullptr;
		return;
	}
	// The Manny keeps running every gameplay animation, unseen; the hero copies its pose.
	HeroMesh->HideDriver();
}

bool AShinobiPlayerCharacter::CanMove() const
{
	static const FGameplayTagContainer Blockers = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ShinobiTags::State_ChakraCharging);
		Tags.AddTag(ShinobiTags::State_Shocked);
		Tags.AddTag(ShinobiTags::State_Staggered);
		Tags.AddTag(ShinobiTags::State_Reviving);
		Tags.AddTag(ShinobiTags::State_Downed);
		Tags.AddTag(ShinobiTags::State_Dead);
		Tags.AddTag(ShinobiTags::State_PvpHold);
		return Tags;
	}();
	return !AbilitySystem->HasAnyMatchingGameplayTags(Blockers);
}

void AShinobiPlayerCharacter::ApplyLoadout(const ShinobiLoadout::FSlots& Slots)
{
	if (!HasAuthority() || Slots.Num() != ShinobiLoadout::NumSlots)
	{
		return;
	}
	Loadout = Slots;
	StampLoadoutInputIDs();
	LogLoadout();
}

void AShinobiPlayerCharacter::OnRep_Loadout()
{
	StampLoadoutInputIDs();
	LogLoadout();
	if (PendingSave.Num() > 0 && Loadout == PendingSave)
	{
		ShinobiLoadout::Save(Loadout);
		PendingSave.Reset();
	}
}

void AShinobiPlayerCharacter::LogLoadout() const
{
	FString Line;
	for (int32 Slot = 0; Slot < Loadout.Num(); ++Slot)
	{
		const UClass* Jutsu = Loadout[Slot].Get();
		const FGameplayAbilitySpec* Spec = Jutsu ? AbilitySystem->FindAbilitySpecFromClass(const_cast<UClass*>(Jutsu)) : nullptr;
		// ok: this machine's spec answers the slot's key (a client's specs may not have arrived yet: "nospec")
		const TCHAR* State = !Jutsu ? TEXT("") : !Spec ? TEXT("(nospec)") : Spec->InputID == static_cast<int32>(ShinobiLoadout::SlotInputID(Slot)) ? TEXT("(ok)") : TEXT("(BAD)");
		Line += FString::Printf(TEXT(" slot%d=%s%s"), Slot + 1, Jutsu ? *Jutsu->GetName() : TEXT("-"), State);
	}
	UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiLoadout %s (%s):%s"), *GetName(), HasAuthority() ? TEXT("server") : TEXT("client"), *Line);
}

void AShinobiPlayerCharacter::StampLoadoutInputIDs()
{
	const auto SlotOf = [this](const UClass* Jutsu) -> int32
	{
		const int32 Slot = Jutsu ? Loadout.IndexOfByPredicate([Jutsu](const TSubclassOf<UShinobiJutsuAbility>& InSlot) { return InSlot.Get() == Jutsu; }) : INDEX_NONE;
		return Slot == INDEX_NONE ? INDEX_NONE : static_cast<int32>(ShinobiLoadout::SlotInputID(Slot));
	};
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		const UShinobiGameplayAbility* Ability = Cast<UShinobiGameplayAbility>(Spec.Ability);
		if (!Ability)
		{
			continue;
		}
		int32 InputID;
		if (Cast<UShinobiJutsuAbility>(Ability))
		{
			InputID = SlotOf(Ability->GetClass());
		}
		else if (Ability->FollowsSlotOf)
		{
			InputID = SlotOf(Ability->FollowsSlotOf.Get());
		}
		else
		{
			continue; // melee, dodge, the dismount key...: fixed inputs
		}
		if (Spec.InputID != InputID)
		{
			Spec.InputID = InputID;
			if (HasAuthority())
			{
				AbilitySystem->MarkAbilitySpecDirty(Spec);
			}
		}
	}
}

void AShinobiPlayerCharacter::RequestLoadout(const ShinobiLoadout::FSlots& Slots, bool bSave)
{
	if (HasAuthority())
	{
		if (TryChangeLoadout(Slots) && bSave)
		{
			ShinobiLoadout::Save(Loadout);
		}
		return;
	}
	// Saved once the server's answer comes back as this loadout (OnRep_Loadout); a refusal changes nothing, saved bar included.
	PendingSave = bSave ? Slots : ShinobiLoadout::FSlots();
	ServerRequestLoadout(Slots);
}

void AShinobiPlayerCharacter::ServerRequestLoadout_Implementation(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Slots)
{
	if (ShinobiLoadout::IsValid(Slots, GetKnownJutsu())) // a client's value: check it
	{
		TryChangeLoadout(Slots);
	}
}

bool AShinobiPlayerCharacter::TryChangeLoadout(const ShinobiLoadout::FSlots& Slots)
{
	if (!HasAuthority() || Slots.Num() != ShinobiLoadout::NumSlots)
	{
		return false;
	}
	if (Slots == Loadout)
	{
		return true;
	}
	const FText Reason = GetLoadoutLockReason();
	if (!Reason.IsEmpty() || !ShinobiLoadout::CanChange(Loadout, Slots, [this](int32 Slot) { return IsSlotLocked(Slot); }))
	{
		UE_LOG(LogShinobiCoop, Display, TEXT("ShinobiLoadout %s refused: %s"), *GetName(), Reason.IsEmpty() ? TEXT("a locked slot") : *Reason.ToString());
		return false;
	}
	ApplyLoadout(Slots);
	return true;
}

FText AShinobiPlayerCharacter::GetLoadoutLockReason() const
{
	if (IsCastingJutsu())
	{
		return INVTEXT("Casting");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		return INVTEXT("Mounted");
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Downed))
	{
		return INVTEXT("Downed");
	}
	if (IsInCombat())
	{
		return INVTEXT("In combat");
	}
	const AShinobiGameState* State = GetWorld() ? GetWorld()->GetGameState<AShinobiGameState>() : nullptr;
	if (State && State->PvpPhase == EShinobiPvpPhase::Fighting)
	{
		return INVTEXT("Round in progress");
	}
	return FText::GetEmpty();
}

float AShinobiPlayerCharacter::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;
	return State ? static_cast<float>(State->GetServerWorldTimeSeconds()) : (World ? World->GetTimeSeconds() : 0.f);
}

void AShinobiPlayerCharacter::NoteCombat()
{
	if (HasAuthority())
	{
		LastCombatTime = ServerNow();
	}
}

float AShinobiPlayerCharacter::GetCombatSecondsLeft() const
{
	return FMath::Max(0.f, LastCombatTime + CombatSeconds - ServerNow());
}

bool AShinobiPlayerCharacter::IsSlotLocked(int32 SlotIndex) const
{
	const UClass* Jutsu = Loadout.IsValidIndex(SlotIndex) ? Loadout[SlotIndex].Get() : nullptr;
	const FGameplayAbilitySpec* Spec = Jutsu ? AbilitySystem->FindAbilitySpecFromClass(const_cast<UClass*>(Jutsu)) : nullptr;
	const UShinobiJutsuAbility* Ability = Spec ? Cast<UShinobiJutsuAbility>(Spec->Ability) : nullptr;
	return Ability && Ability->IsSlotLocked(*AbilitySystem, *Spec);
}

TArray<TSubclassOf<UShinobiJutsuAbility>> AShinobiPlayerCharacter::GetKnownJutsu() const
{
	TArray<TSubclassOf<UShinobiJutsuAbility>> Known;
	for (const TSubclassOf<UShinobiGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UShinobiJutsuAbility::StaticClass()))
		{
			Known.Add(TSubclassOf<UShinobiJutsuAbility>(AbilityClass.Get()));
		}
	}
	return Known;
}

void AShinobiPlayerCharacter::FlipPadJutsuPage(int32 Direction)
{
	constexpr int32 PadPages = ShinobiLoadout::NumSlots / 4;
	PadJutsuPage = ((PadJutsuPage - 1 + Direction) % PadPages + PadPages) % PadPages + 1;
}

void AShinobiPlayerCharacter::PadJutsuPressed(int32 Face)
{
	if (Face < 0 || Face >= 4)
	{
		return;
	}
	PadPressedSlot[Face] = (PadJutsuPage - 1) * 4 + Face;
	AbilityInputPressed(ShinobiLoadout::SlotInputID(PadPressedSlot[Face]));
}

void AShinobiPlayerCharacter::PadJutsuReleased(int32 Face)
{
	if (Face < 0 || Face >= 4 || PadPressedSlot[Face] == INDEX_NONE)
	{
		return;
	}
	const int32 Slot = PadPressedSlot[Face]; // a page flip mid-hold must not release another slot (charged jutsu)
	PadPressedSlot[Face] = INDEX_NONE;
	AbilityInputReleased(ShinobiLoadout::SlotInputID(Slot));
}

bool AShinobiPlayerCharacter::IsCastingJutsu() const
{
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.IsActive() && Cast<UShinobiJutsuAbility>(Spec.Ability))
		{
			return true;
		}
	}
	return false;
}

bool AShinobiPlayerCharacter::CancelJutsuTargetPlacement()
{
	for (FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (!Spec.IsActive())
		{
			continue;
		}
		if (UShinobiJutsuAbility* Jutsu = Cast<UShinobiJutsuAbility>(Spec.GetPrimaryInstance()); Jutsu && Jutsu->CancelTargetPlacement())
		{
			return true;
		}
	}
	return false;
}

void AShinobiPlayerCharacter::BeginPlay()
{
	ApplyHero();
	Super::BeginPlay();
	if (HasAuthority())
	{
		ApplyLoadout(ShinobiLoadout::Default()); // the owner sends its saved one once its input is set up
	}
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Downed).AddUObject(this, &AShinobiPlayerCharacter::OnDownedTagChanged);
	// Lightning Blink: the body is gone while it streaks (the ability's tag reaches every machine).
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Blinking).AddWeakLambda(this, [this](const FGameplayTag, int32 Count)
	{
		SetActorHiddenInGame(Count > 0);
	});
}

void AShinobiPlayerCharacter::FellOutOfWorld(const UDamageType& DamageType)
{
	if (!HasAuthority())
	{
		return;
	}
	const AShinobiGameState* State = GetWorld()->GetGameState<AShinobiGameState>();
	const bool bPvp = State && State->IsPvp();
	if (bPvp && State->PvpPhase != EShinobiPvpPhase::Warmup)
	{
		Super::FellOutOfWorld(DamageType); // gone: out for the round (dead to GatherStatus); the next reset respawns the player
		return;
	}
	// Back to a start: the first one, or in a PvP warmup one of this side's (any start when the map has none tagged).
	const FName OwnTag = bPvp && IsPvpTeam(GetTeam())
		? (GetTeam() == EShinobiTeam::TeamA ? AShinobiPvpGameMode::TeamStartTagA : AShinobiPvpGameMode::TeamStartTagB) : NAME_None;
	APlayerStart* Spot = nullptr;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Spot = Spot ? Spot : *It;
		if (OwnTag.IsNone() || It->ActorHasTag(OwnTag))
		{
			Spot = *It;
			break;
		}
	}
	if (Spot)
	{
		GetCharacterMovement()->StopMovementImmediately();
		TeleportTo(Spot->GetActorLocation(), Spot->GetActorRotation());
		return;
	}
	Super::FellOutOfWorld(DamageType);
}

void AShinobiPlayerCharacter::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	if (!IsAlive())
	{
		return;
	}
	AbilitySystem->CancelAllAbilities();
	AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGE_Downed>(), 1.f, AbilitySystem->MakeEffectContext());
}

void AShinobiPlayerCharacter::Revive()
{
	if (!AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Downed))
	{
		return;
	}
	// Health first: the downed tag's removal checks it to tell a revive from a bleed-out.
	AbilitySystem->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth() * ReviveHealthFraction);
	AbilitySystem->RemoveActiveGameplayEffectBySourceEffect(UGE_Downed::StaticClass(), nullptr);
}

float AShinobiPlayerCharacter::GetBleedOutRemaining() const
{
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ShinobiTags::State_Downed));
	const TArray<float> Remaining = AbilitySystem->GetActiveEffectsTimeRemaining(Query);
	return Remaining.Num() > 0 ? Remaining[0] : 0.f;
}

void AShinobiPlayerCharacter::OnDownedTagChanged(const FGameplayTag Tag, int32 Count)
{
	static const TSoftObjectPtr<UAnimMontage> DownedMontage(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Downed.AM_Downed")));
	UAnimMontage* Anim = DownedMontage.LoadSynchronous();
	if (Count > 0)
	{
		if (PhysicalReactions && PhysicalReactions->Down())
		{
			ShinobiSfx::Play(this, EShinobiSfx::Thud, GetActorLocation()); // AM_Downed's sound (UShinobiFeedbackComponent)
		}
		else if (Anim)
		{
			PlayAnimMontage(Anim); // AM_Downed doesn't auto blend out: it holds its last (hunched) frame
		}
		return;
	}
	if (Anim)
	{
		StopAnimMontage(Anim);
	}
	if (HasAuthority() && Attributes->GetHealth() <= 0.f && !AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Dead))
	{
		AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGE_Dead>(), 1.f, AbilitySystem->MakeEffectContext());
	}
	// Revived: get up. A bleed-out stays down (State.Dead, applied just above on the server). Next tick, so a client has
	// the dead tag from the same update before it decides.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (PhysicalReactions && !AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Dead))
		{
			PhysicalReactions->Revive();
		}
	}));
}

void AShinobiPlayerCharacter::ToggleLockOn()
{
	if (LockTarget.IsValid() || !Controller)
	{
		LockTarget = nullptr;
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(LockOnHalfAngle));
	float BestCos = MinCos;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		const float Cos = FVector::DotProduct(ViewRotation.Vector(), (It->GetActorLocation() - ViewLocation).GetSafeNormal());
		if (IsHostileTo(*It) && It->IsAlive() && Cos >= BestCos && FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= LockOnRange)
		{
			BestCos = Cos;
			LockTarget = *It;
		}
	}
}

void AShinobiPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Riding a dragon: the camera pulls back, and Space / Control climb and dive (the owner's input; a remote rider's flight arrives in its moves).
	// Wearing the 12 m Perfect Susanoo, it pulls back further so the whole figure is in view.
	const bool bMounted = AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted);
	const bool bSusanoo = AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Susanoo);
	if (bMounted || bSusanoo || bBoomPulledBack)
	{
		// Only driven around a ride or the armour, so anything else that sets the boom (the world tour's close-ups) is left alone.
		const float TargetArmLength = CameraZoomDistance + (bMounted ? MountedArmLength - FootArmLength : 0.f) + (bSusanoo ? SusanooArmLength - FootArmLength : 0.f);
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, 2.f);
		CameraBoom->TargetOffset.Z = FMath::FInterpTo(CameraBoom->TargetOffset.Z, bSusanoo ? SusanooLookHeight : 0.f, DeltaSeconds, 2.f); // look up at its chest
		bBoomPulledBack = bMounted || bSusanoo || !FMath::IsNearlyEqual(CameraBoom->TargetArmLength, CameraZoomDistance, 1.f) || CameraBoom->TargetOffset.Z > 1.f;
	}
	if (bMounted != bSteeringByCamera)
	{
		// Riding, the character (and so the dragon) faces where the camera looks, so the fire is aimed by turning; on foot it turns with its movement.
		bSteeringByCamera = bMounted;
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->bOrientRotationToMovement = !bMounted;
		Movement->bUseControllerDesiredRotation = bMounted;
		Movement->RotationRate = FRotator(0.f, bMounted ? 360.f : 720.f, 0.f);
	}
	if (!bMounted)
	{
		bFlightClimb = bFlightDive = false; // keys held when the ride ended must not carry into the next one
	}
	if (bMounted && (IsLocallyControlled() || !GetController()) && CanMove())
	{
		AddMovementInput(FVector::UpVector, (bFlightClimb ? 1.f : 0.f) - (bFlightDive ? 1.f : 0.f));
	}
	UpdateLoopPose(SeatedMontage, bSeatedLoadTried, TEXT("/Game/Shinobi/Anims/AM_Mount_Sit.AM_Mount_Sit"), ShinobiTags::State_Mounted, 0.3f);
	UpdateLoopPose(GuardMontage, bGuardLoadTried, TEXT("/Game/Shinobi/Anims/AM_Block_Guard.AM_Block_Guard"), ShinobiTags::State_Blocking, 0.2f);
	if (GetCharacterMovement()->IsFalling())
	{
		FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
	}
	UpdateOutfitCloth();
	// Possession and the player state can arrive after BeginPlay, so keep the tag current here.
	NameTag->SetVisibility(!IsLocallyControlled());
	if (const APlayerState* State = GetPlayerState())
	{
		NameTag->SetNameMode(State->GetPlayerName());
	}
	const AShinobiCharacterBase* Target = LockTarget.Get();
	if (!Target)
	{
		return;
	}
	if (!Controller || !Target->IsAlive() || FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > LockOnRange)
	{
		LockTarget = nullptr;
		return;
	}
	if (bJutsuTargetPlacementActive)
	{
		return;
	}
	// Aim the camera (so every aimed jutsu) at the target's chest; the server aims from the replicated camera.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FRotator Desired = (Target->GetActorLocation() + FVector(0.f, 0.f, 30.f) - ViewLocation).Rotation();
	Desired.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch), -40.f, 20.f);
	Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaSeconds, 10.f));
}

void AShinobiPlayerCharacter::UpdateLoopPose(TObjectPtr<UAnimMontage>& Montage, bool& bLoadTried, const TCHAR* Path, const FGameplayTag& Tag, float BlendOutSeconds)
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (!Montage)
	{
		if (bLoadTried)
		{
			return;
		}
		bLoadTried = true; // once: a missing asset must not cost a failed load every frame
		Montage = LoadObject<UAnimMontage>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Montage)
		{
			return;
		}
	}
	if (AbilitySystem->HasMatchingGameplayTag(Tag))
	{
		if (!Anim->IsAnyMontagePlaying()) // another montage (a jutsu) takes the pose for its moment, then the loop comes back
		{
			Anim->Montage_Play(Montage);
			Anim->Montage_SetNextSection(TEXT("Default"), TEXT("Default"), Montage); // a loop
		}
	}
	else if (Anim->Montage_IsPlaying(Montage))
	{
		Anim->Montage_Stop(BlendOutSeconds, Montage);
	}
}

void AShinobiPlayerCharacter::UnPossessed()
{
	// A lost controller (unpossess, disconnect) never sends the release for held inputs, and chakra charge never
	// ends on its own, so end the hold-to-use abilities here. Must run before Super: APawn::UnPossessed clears
	// the Owner, after which the ClientCancelAbility RPC can't reach the owning client. Mostly server-side; on a
	// client it can also run while the pawn is being destroyed (harmless).
	SetCameraFreeLook(false);
	FGameplayTagContainer HeldAbilities;
	HeldAbilities.AddTag(ShinobiTags::Ability_Sprint);
	HeldAbilities.AddTag(ShinobiTags::Ability_ChakraCharge);
	AbilitySystem->CancelAbilities(&HeldAbilities);
	bFlightClimb = bFlightDive = false; // their releases never arrive
	Super::UnPossessed();
}

void AShinobiPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	const UShinobiInputConfig& InputConfig = UShinobiInputConfig::Get();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputConfig.Context, ShinobiInputPriority);
			// A possession that ended while RB was held never got RB's release, so start without the jutsu context.
			Subsystem->RemoveMappingContext(InputConfig.JutsuContext);
		}
	}

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(InputConfig.Move, ETriggerEvent::Triggered, this, &AShinobiPlayerCharacter::Move);
	Input->BindAction(InputConfig.Look, ETriggerEvent::Triggered, this, &AShinobiPlayerCharacter::Look);
	Input->BindAction(InputConfig.CameraFreeLook, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::SetCameraFreeLook, true);
	Input->BindAction(InputConfig.CameraFreeLook, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::SetCameraFreeLook, false);
	Input->BindAction(InputConfig.CameraFreeLook, ETriggerEvent::Canceled, this, &AShinobiPlayerCharacter::SetCameraFreeLook, false);
	Input->BindAction(InputConfig.CameraZoom, ETriggerEvent::Triggered, this, &AShinobiPlayerCharacter::ZoomCamera);
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::JumpPressed);
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::JumpReleased);
	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : InputConfig.AbilityActions)
	{
		Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::AbilityInputPressed, Pair.Key);
		Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::AbilityInputReleased, Pair.Key);
	}
	for (int32 Face = 0; Face < InputConfig.PadJutsuActions.Num(); ++Face)
	{
		Input->BindAction(InputConfig.PadJutsuActions[Face], ETriggerEvent::Started, this, &AShinobiPlayerCharacter::PadJutsuPressed, Face);
		Input->BindAction(InputConfig.PadJutsuActions[Face], ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::PadJutsuReleased, Face);
	}
	Input->BindAction(InputConfig.JutsuPageNext, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::FlipPadJutsuPage, 1);
	Input->BindAction(InputConfig.JutsuPagePrev, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::FlipPadJutsuPage, -1);
	Input->BindAction(InputConfig.LockOn, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::ToggleLockOn);
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::SetJutsuModifier, true);
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::SetJutsuModifier, false);

	if (IsLocallyControlled())
	{
		ShinobiLoadout::FSlots Saved = ShinobiLoadout::LoadSaved(GetKnownJutsu()); // this machine's saved bar
		if (FParse::Param(FCommandLine::Get(), TEXT("ShinobiLoadoutSmoke")))
		{
			Saved = ShinobiLoadout::WithLegacyPage(ShinobiLoadout::Default(), 3); // smoke_m4: a bar the host must take from this client
		}
		RequestLoadout(Saved, false); // already this machine's save (or the smoke test's bar, which must not become it)
	}
}

void AShinobiPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || !CanMove())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix Yaw(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	FVector Intent = Yaw.GetUnitAxis(EAxis::X) * Axis.Y + Yaw.GetUnitAxis(EAxis::Y) * Axis.X;
	if (const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(GetCharacterMovement()); Movement && Movement->IsWallTraversing())
	{
		Intent = Movement->WallInputDirection(Intent); // the camera stays upright; the wall turns it into up/down/sideways
	}
	AddMovementInput(Intent);
}

void AShinobiPlayerCharacter::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	if (UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(GetCharacterMovement()))
	{
		Movement->DetachFromWall();
	}
	Super::LaunchCharacter(LaunchVelocity, bXYOverride, bZOverride);
}

void AShinobiPlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	static const TSoftObjectPtr<UAnimMontage> RollMontage(FSoftObjectPath(TEXT("/Game/Shinobi/Anims/AM_Land_Roll.AM_Land_Roll")));
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(GetCharacterMovement());
	if (Movement->IsFalling())
	{
		FallApexZ = GetActorLocation().Z;
		if (UAnimMontage* Roll = RollMontage.Get())
		{
			Anim->Montage_Stop(0.1f, Roll); // jumped out of the roll
		}
		return;
	}
	// ponytail: the roll is in place (build_land_roll.py strips its travel); the landing momentum carries it. A forward
	// push (root motion force, as GA_Dodge) if standing-still rolls look wrong.
	if (PrevMovementMode == MOVE_Falling && Movement->IsMovingOnGround() && !Movement->IsWallTraversing() && CanMove()
		&& FallApexZ - GetActorLocation().Z >= RollFallHeight && !Anim->IsAnyMontagePlaying())
	{
		if (UAnimMontage* Roll = RollMontage.LoadSynchronous())
		{
			Anim->Montage_Play(Roll);
		}
	}
}

void AShinobiPlayerCharacter::Look(const FInputActionValue& Value)
{
	ApplyLookInput(Value.Get<FVector2D>());
}

void AShinobiPlayerCharacter::ApplyLookInput(const FVector2D& Axis)
{
	if (bCameraFreeLookActive)
	{
		ApplyCameraFreeLookInput(Axis);
		return;
	}
	if (bJutsuTargetPlacementActive)
	{
		MoveJutsuTargetCursor(Axis);
		return;
	}
	if (LockTarget.IsValid())
	{
		return; // the lock steers the camera
	}
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AShinobiPlayerCharacter::SetCameraFreeLook(bool bHeld)
{
	if (bHeld == bCameraFreeLookActive)
	{
		return;
	}

	if (bHeld)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (!PC || !IsLocallyControlled())
		{
			return;
		}

		CameraFreeLookRotation = (PC->GetControlRotation() - GetActorRotation()).GetNormalized();
		CameraBoom->bUsePawnControlRotation = false;
		CameraBoom->SetRelativeRotation(CameraFreeLookRotation);
		bCameraFreeLookActive = true;
		return;
	}

	CameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
	CameraBoom->bUsePawnControlRotation = true;
	bCameraFreeLookActive = false;
}

void AShinobiPlayerCharacter::ApplyCameraFreeLookInput(const FVector2D& Axis)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	CameraFreeLookRotation.Yaw += Axis.X * PC->InputYawScale_DEPRECATED;
	CameraFreeLookRotation.Pitch = FMath::Clamp(
		CameraFreeLookRotation.Pitch + Axis.Y * PC->InputPitchScale_DEPRECATED,
		CameraFreeLookMinPitch,
		CameraFreeLookMaxPitch);
	CameraFreeLookRotation.Normalize();
	CameraBoom->SetRelativeRotation(CameraFreeLookRotation);
}

void AShinobiPlayerCharacter::MoveJutsuTargetCursor(const FVector2D& Axis)
{
	JutsuTargetScreenPosition += Axis * 0.006f;
	JutsuTargetScreenPosition.X = FMath::Clamp(JutsuTargetScreenPosition.X, 0.f, 1.f);
	JutsuTargetScreenPosition.Y = FMath::Clamp(JutsuTargetScreenPosition.Y, 0.f, 1.f);
}

void AShinobiPlayerCharacter::BeginJutsuTargetPlacement()
{
	bJutsuTargetPlacementActive = true;
	bHasServerJutsuAimDirection = false;
	JutsuTargetScreenPosition = FVector2D(0.5f, 0.5f);
}

void AShinobiPlayerCharacter::EndJutsuTargetPlacement()
{
	bJutsuTargetPlacementActive = false;
	bHasServerJutsuAimDirection = false;
	JutsuTargetScreenPosition = FVector2D(0.5f, 0.5f);
}

FVector AShinobiPlayerCharacter::GetJutsuTargetAimDirection() const
{
	if (HasAuthority() && bHasServerJutsuAimDirection)
	{
		return ServerJutsuAimDirection;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	const FRotator ViewRotation = PC ? PC->GetControlRotation() : GetActorRotation();
	float AspectRatio = 16.f / 9.f;
	if (PC)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		PC->GetViewportSize(ViewportWidth, ViewportHeight);
		if (ViewportWidth > 0 && ViewportHeight > 0)
		{
			AspectRatio = static_cast<float>(ViewportWidth) / ViewportHeight;
		}
	}
	const float FieldOfView = PC && PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetFOVAngle()
		: FollowCamera->FieldOfView;
	const float HalfFovTangent = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float HorizontalOffset = (JutsuTargetScreenPosition.X * 2.f - 1.f) * HalfFovTangent;
	const float VerticalOffset = (1.f - JutsuTargetScreenPosition.Y * 2.f) * HalfFovTangent / AspectRatio;
	const FRotationMatrix ViewBasis(ViewRotation);
	return (ViewBasis.GetUnitAxis(EAxis::X)
		+ ViewBasis.GetUnitAxis(EAxis::Y) * HorizontalOffset
		+ ViewBasis.GetUnitAxis(EAxis::Z) * VerticalOffset).GetSafeNormal();
}

void AShinobiPlayerCharacter::ServerSetJutsuTargetAimDirection_Implementation(FVector Direction)
{
	if (!bJutsuTargetPlacementActive || Direction.IsNearlyZero())
	{
		return;
	}
	ServerJutsuAimDirection = Direction.GetSafeNormal();
	bHasServerJutsuAimDirection = true;
}

void AShinobiPlayerCharacter::ZoomCamera(const FInputActionValue& Value)
{
	const float ZoomAxis = Value.Get<float>();
	if (FMath::IsNearlyZero(ZoomAxis))
	{
		return;
	}
	// While the big map is open the wheel zooms the map, not the camera.
	if (AShinobiPlayerController* PC = Cast<AShinobiPlayerController>(Controller); PC && PC->IsBigMapOpen())
	{
		PC->ZoomMap(ZoomAxis > 0.f ? 1 : -1, false);
		return;
	}

	CameraZoomDistance = FMath::Clamp(
		CameraZoomDistance - ZoomAxis * CameraZoomStep,
		MinCameraArmLength,
		MaxCameraArmLength);

	if (!AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		CameraBoom->TargetArmLength = CameraZoomDistance;
	}
}

bool AShinobiPlayerCharacter::CanJumpOrTraverse() const
{
	return CanMove() && !AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Blocking);
}

void AShinobiPlayerCharacter::JumpPressed()
{
	if (!CanJumpOrTraverse())
	{
		return;
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		bFlightClimb = true; // on a dragon Space climbs
		return;
	}
	if (Traversal->IsTraversing() || Traversal->TryTraversal())
	{
		return; // over or onto what is ahead instead of jumping
	}
	Jump();
}

void AShinobiPlayerCharacter::JumpReleased()
{
	bFlightClimb = false;
	StopJumping();
}

void AShinobiPlayerCharacter::AbilityInputPressed(EShinobiAbilityInputID InputID)
{
	if (RouteTargetPlacementInput(InputID, true))
	{
		return;
	}
	bFlightDive |= InputID == EShinobiAbilityInputID::Dodge && AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted); // Control dives
	// A second dodge press inside a dodge's window blinks instead (UGA_Blink checks the window, stamina and its cooldown).
	if (InputID == EShinobiAbilityInputID::Dodge && AbilitySystem->TryActivateAbilityByClass(UGA_Blink::StaticClass()))
	{
		return;
	}
	if (bJutsuTargetPlacementActive && ShinobiLoadout::SlotIndexOf(InputID) != INDEX_NONE && !HasAuthority())
	{
		ServerSetJutsuTargetAimDirection(GetJutsuTargetAimDirection());
	}
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(InputID));
}

void AShinobiPlayerCharacter::AbilityInputReleased(EShinobiAbilityInputID InputID)
{
	if (RouteTargetPlacementInput(InputID, false))
	{
		return;
	}
	if (InputID == EShinobiAbilityInputID::Dodge)
	{
		bFlightDive = false;
	}
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(InputID));
}

bool AShinobiPlayerCharacter::RouteTargetPlacementInput(EShinobiAbilityInputID InputID, bool bPressed)
{
	if (InputID != EShinobiAbilityInputID::Melee)
	{
		return false;
	}
	if (!bPressed)
	{
		if (TargetPlacementInputID == INDEX_NONE)
		{
			return false;
		}
		AbilitySystem->AbilityLocalInputReleased(TargetPlacementInputID);
		TargetPlacementInputID = INDEX_NONE;
		return true;
	}
	if (!bJutsuTargetPlacementActive || TargetPlacementInputID != INDEX_NONE)
	{
		return false;
	}

	int32 PlacementInputID = INDEX_NONE;
	{
		FScopedAbilityListLock AbilityListLock(*AbilitySystem);
		for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
		{
			if (Spec.IsActive() && ShinobiLoadout::SlotIndexOf(static_cast<EShinobiAbilityInputID>(Spec.InputID)) != INDEX_NONE)
			{
				PlacementInputID = Spec.InputID;
				break;
			}
		}
	}
	if (PlacementInputID == INDEX_NONE)
	{
		return false;
	}

	TargetPlacementInputID = PlacementInputID;
	if (!HasAuthority())
	{
		ServerSetJutsuTargetAimDirection(GetJutsuTargetAimDirection());
	}
	AbilitySystem->AbilityLocalInputPressed(PlacementInputID);
	return true;
}

void AShinobiPlayerCharacter::SetJutsuModifier(bool bHeld)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem)
	{
		return;
	}
	const UInputMappingContext* JutsuContext = UShinobiInputConfig::Get().JutsuContext;
	if (bHeld)
	{
		Subsystem->AddMappingContext(JutsuContext, ShinobiJutsuInputPriority);
	}
	else
	{
		Subsystem->RemoveMappingContext(JutsuContext);
	}
}
