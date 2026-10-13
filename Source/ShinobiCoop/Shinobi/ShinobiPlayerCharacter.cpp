#include "Shinobi/ShinobiPlayerCharacter.h"
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
#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
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
#include "Shinobi/Traversal/ShinobiTraversalComponent.h"
#include "MotionWarpingComponent.h"

namespace
{
	TAutoConsoleVariable<bool> CVarHero(
		TEXT("Shinobi.Hero"),
		true,
		TEXT("Players use the hero character (sub-project #2) when its assets exist; 0 = the plain Manny. Read when a player spawns."));
}

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = {
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(),
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
		UGA_Substitution::StaticClass(), UGA_Revive::StaticClass(),
		UGA_AlmightyPush::StaticClass(), UGA_Kamui::StaticClass(), UGA_KamuiExit::StaticClass(),
		UGA_Teleport::StaticClass(), UGA_TeleportJump::StaticClass(),
		UGA_EarthWall::StaticClass(), UGA_WaterWall::StaticClass(), UGA_FireMeteor::StaticClass(), UGA_IceMeteor::StaticClass(), UGA_WaterDragon::StaticClass(), UGA_Susanoo::StaticClass(),
		UGA_SummonToad::StaticClass(), UGA_SummonSnake::StaticClass(), UGA_SummonPabu::StaticClass(),
		UGA_SummonDragon::StaticClass(), UGA_SummonCrimsonDragon::StaticClass(), UGA_DragonDismount::StaticClass(), UGA_DragonDismountKey::StaticClass(), UGA_DragonBreath::StaticClass(),
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

	NameTag = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(RootComponent);

	SurfaceFx = CreateDefaultSubobject<UShinobiSurfaceFxComponent>(TEXT("SurfaceFx"));
	SurfaceFx->SetupAttachment(RootComponent); // turns with the capsule onto walls

	Traversal = CreateDefaultSubobject<UShinobiTraversalComponent>(TEXT("Traversal"));
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
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

void AShinobiPlayerCharacter::ApplyOutfit()
{
	if (!HeroMesh || !OutfitMesh)
	{
		return;
	}
	USkeletalMesh* Cloth = nullptr;
	if (OutfitIndex < UE_ARRAY_COUNT(Outfits) && Outfits[OutfitIndex].Mesh)
	{
		// Quiet: an outfit that isn't imported yet is expected.
		Cloth = LoadObject<USkeletalMesh>(nullptr, Outfits[OutfitIndex].Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	const USkeletalMesh* Body = HeroMesh->GetSkeletalMeshAsset();
	OutfitMesh->SetSkeletalMeshAsset(Cloth);
	OutfitMesh->SetVisibility(Cloth != nullptr);
	if (!Cloth || !Body)
	{
		OutfitMesh->SetLeaderPoseComponent(nullptr);
		return;
	}
	// The outfit's bones take the body's component-space pose, so a body whose skeleton sits elsewhere in its FBX than the
	// outfit's would carry the outfit off by the difference: shift it back by the two hips' reference-pose offset.
	OutfitMesh->SetRelativeLocation(RefHipsLocation(Cloth) - RefHipsLocation(Body));
	OutfitMesh->SetLeaderPoseComponent(HeroMesh);
	RefreshOutline();
	// Scripts/smoke_m4.ps1 style check: both machines log what they see.
	UE_LOG(LogShinobiCoop, Log, TEXT("%s wears %s"), *GetName(), Outfits[OutfitIndex].Name);
}

bool AShinobiPlayerCharacter::ApplyCharacter()
{
	if (!HeroMesh)
	{
		return false;
	}
	for (const int32 Index : {int32(CharacterIndex), 0}) // the pick, else the hero
	{
		if (Index < UE_ARRAY_COUNT(Roster) && HeroMesh->Wear(Roster[Index].Mesh, Roster[Index].Retargeter))
		{
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
	DOREPLIFETIME(AShinobiPlayerCharacter, OutfitIndex);
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

int32 AShinobiPlayerCharacter::GetJutsuPage() const
{
	for (int32 Page = 1; Page <= ShinobiTags::NumJutsuPages; ++Page)
	{
		if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::JutsuPage(Page)))
		{
			return Page;
		}
	}
	return 1;
}

void AShinobiPlayerCharacter::ApplyJutsuPage(int32 Page)
{
	if (Page < 1 || Page > ShinobiTags::NumJutsuPages || AbilitySystem->HasMatchingGameplayTag(ShinobiTags::JutsuPage(Page)))
	{
		return;
	}
	for (int32 Old = 1; Old <= ShinobiTags::NumJutsuPages; ++Old)
	{
		if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::JutsuPage(Old)))
		{
			AbilitySystem->RemoveLooseGameplayTag(ShinobiTags::JutsuPage(Old));
		}
	}
	AbilitySystem->AddLooseGameplayTag(ShinobiTags::JutsuPage(Page));
}

void AShinobiPlayerCharacter::SetJutsuPage(int32 Page)
{
	if (Page < 1 || Page > ShinobiTags::NumJutsuPages || IsCastingJutsu())
	{
		return;
	}
	ApplyJutsuPage(Page);
	if (!HasAuthority())
	{
		ServerSetJutsuPage(static_cast<uint8>(Page));
	}
}

bool AShinobiPlayerCharacter::TryCycleJutsuPage(int32 Direction)
{
	if (IsCastingJutsu())
	{
		return false;
	}
	const int32 Current = GetJutsuPage();
	const int32 Count = ShinobiTags::NumJutsuPages;
	for (int32 Step = 1; Step < Count; ++Step)
	{
		const int32 Candidate = ((Current - 1 + Direction * Step) % Count + Count) % Count + 1;
		if (PageHasJutsu(Candidate))
		{
			SetJutsuPage(Candidate);
			return true;
		}
	}
	return false;
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

bool AShinobiPlayerCharacter::PageHasJutsu(int32 Page) const
{
	for (const TSubclassOf<UShinobiGameplayAbility>& AbilityClass : StartupAbilities)
	{
		const UShinobiJutsuAbility* Jutsu = AbilityClass ? Cast<UShinobiJutsuAbility>(AbilityClass->GetDefaultObject()) : nullptr;
		if (Jutsu && Jutsu->Page == Page)
		{
			return true;
		}
	}
	return false;
}

void AShinobiPlayerCharacter::ServerSetJutsuPage_Implementation(uint8 Page)
{
	if (Page < 1 || Page > ShinobiTags::NumJutsuPages)
	{
		return; // a client's value: ignore it
	}
	if (IsCastingJutsu())
	{
		ClientSetJutsuPage(static_cast<uint8>(GetJutsuPage())); // the client switched mid-cast: put it back
		return;
	}
	ApplyJutsuPage(Page);
}

void AShinobiPlayerCharacter::ClientSetJutsuPage_Implementation(uint8 Page)
{
	ApplyJutsuPage(Page);
}

void AShinobiPlayerCharacter::BeginPlay()
{
	ApplyHero();
	Super::BeginPlay();
	AbilitySystem->AddLooseGameplayTag(ShinobiTags::JutsuPage(1)); // every machine tracks its own copy
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Downed).AddUObject(this, &AShinobiPlayerCharacter::OnDownedTagChanged);
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
		if (Anim)
		{
			PlayAnimMontage(Anim); // AM_Downed doesn't auto blend out: it holds the last (lying) frame
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
	const bool bMounted = AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted);
	if (bMounted || bBoomPulledBack)
	{
		// Only driven around a ride, so anything else that sets the boom (the world tour's close-ups) is left alone.
		const float TargetArmLength = CameraZoomDistance + (bMounted ? MountedArmLength - FootArmLength : 0.f);
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, 2.f);
		bBoomPulledBack = bMounted || !FMath::IsNearlyEqual(CameraBoom->TargetArmLength, CameraZoomDistance, 1.f);
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
	UpdateSeatedPose();
	if (GetCharacterMovement()->IsFalling())
	{
		FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
	}
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

void AShinobiPlayerCharacter::UpdateSeatedPose()
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (!SeatedMontage)
	{
		if (bSeatedLoadTried)
		{
			return;
		}
		bSeatedLoadTried = true; // once: a missing asset must not cost a failed load every frame
		SeatedMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Mount_Sit.AM_Mount_Sit"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SeatedMontage)
		{
			return;
		}
	}
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		if (!Anim->IsAnyMontagePlaying()) // a jutsu's montage takes the pose for its moment, then the rider sits again
		{
			Anim->Montage_Play(SeatedMontage);
			Anim->Montage_SetNextSection(TEXT("Default"), TEXT("Default"), SeatedMontage); // a loop
		}
	}
	else if (Anim->Montage_IsPlaying(SeatedMontage))
	{
		Anim->Montage_Stop(0.3f, SeatedMontage);
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
	Input->BindAction(InputConfig.JutsuPageNext, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::CycleJutsuPage, 1);
	Input->BindAction(InputConfig.JutsuPagePrev, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::CycleJutsuPage, -1);
	Input->BindAction(InputConfig.LockOn, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::ToggleLockOn);
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::SetJutsuModifier, true);
	Input->BindAction(InputConfig.JutsuModifier, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::SetJutsuModifier, false);
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

void AShinobiPlayerCharacter::JumpPressed()
{
	if (!CanMove())
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
	if (bJutsuTargetPlacementActive && InputID >= EShinobiAbilityInputID::Jutsu1 && InputID <= EShinobiAbilityInputID::Jutsu4 && !HasAuthority())
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
			if (Spec.IsActive()
				&& Spec.InputID >= static_cast<int32>(EShinobiAbilityInputID::Jutsu1)
				&& Spec.InputID <= static_cast<int32>(EShinobiAbilityInputID::Jutsu4))
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
