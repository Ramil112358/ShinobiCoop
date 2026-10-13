#include "Shinobi/ShinobiCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/VFX/ShinobiBodyFlameComponent.h"
#include "Shinobi/VFX/ShinobiCastFxComponent.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Shinobi/VFX/ShinobiFeedbackComponent.h"
#include "Shinobi/VFX/ShinobiPhaseFxComponent.h"
#include "Shinobi/VFX/ShinobiSusanooComponent.h"

AShinobiCharacterBase::AShinobiCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShinobiMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	Attributes = CreateDefaultSubobject<UShinobiAttributeSet>(TEXT("Attributes"));

	ChakraAura = CreateDefaultSubobject<UShinobiChakraAuraComponent>(TEXT("ChakraAura"));
	ChakraAura->SetupAttachment(RootComponent);
	CastFx = CreateDefaultSubobject<UShinobiCastFxComponent>(TEXT("CastFx"));
	CastFx->SetupAttachment(RootComponent);
	BodyFlames = CreateDefaultSubobject<UShinobiBodyFlameComponent>(TEXT("BodyFlames"));
	BodyFlames->SetupAttachment(RootComponent);
	Feedback = CreateDefaultSubobject<UShinobiFeedbackComponent>(TEXT("Feedback"));
	PhaseFx = CreateDefaultSubobject<UShinobiPhaseFxComponent>(TEXT("PhaseFx"));
	Susanoo = CreateDefaultSubobject<UShinobiSusanooComponent>(TEXT("Susanoo"));

	StartupEffects = { UGE_ChakraRegen::StaticClass(), UGE_StaminaRegen::StaticClass() };
}

UAbilitySystemComponent* AShinobiCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AShinobiCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	if (bUseLocomotionAnim)
	{
		// Motion-matched locomotion from the Game Animation Sample (Scripts/anim/). Loaded here, not by ConstructorHelpers:
		// class default objects are built while this module loads, before plugin modules the sample's assets import
		// (DrawDebugLibrary), and loading the AnimBP then breaks its nodes for the whole session.
		static const TSoftClassPtr<UAnimInstance> LocomotionAnim(FSoftObjectPath(TEXT("/Game/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP_C")));
		if (UClass* Locomotion = LocomotionAnim.LoadSynchronous())
		{
			GetMesh()->SetAnimInstanceClass(Locomotion);
		}
	}
	// Outline the visible body (the hero, when a hidden Manny drives it); the driver itself stays out of custom depth.
	RefreshOutline();
	if (GetBodyMesh() != GetMesh())
	{
		GetMesh()->SetRenderCustomDepth(false);
	}
	AbilitySystem->InitAbilityActorInfo(this, this);
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Dead).AddUObject(this, &AShinobiCharacterBase::OnDeadTagChanged);
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Phased).AddUObject(this, &AShinobiCharacterBase::OnPhasedTagChanged);
	// A downed or dead rider is thrown from the dragon.
	for (const FGameplayTag Out : { ShinobiTags::State_Downed.GetTag(), ShinobiTags::State_Dead.GetTag() })
	{
		AbilitySystem->RegisterGameplayTagEvent(Out).AddWeakLambda(this, [this](const FGameplayTag, int32 Count)
		{
			if (Count > 0 && HasAuthority())
			{
				AbilitySystem->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Mounted));
			}
		});
	}
	if (HasAuthority())
	{
		Attributes->OnOutOfHealth.AddUObject(this, &AShinobiCharacterBase::HandleOutOfHealth);
		GrantStartupAbilitiesAndEffects();
	}
}

void AShinobiCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Not initial-only: the PvP mode picks a joining player's side after the pawn has already replicated.
	DOREPLIFETIME(AShinobiCharacterBase, Team);
}

void AShinobiCharacterBase::SetTeam(EShinobiTeam NewTeam)
{
	if (!HasAuthority() || Team == NewTeam)
	{
		return;
	}
	Team = NewTeam;
	OnTeamChanged();
}

void AShinobiCharacterBase::OnRep_Team()
{
	OnTeamChanged();
}

void AShinobiCharacterBase::OnTeamChanged()
{
	const AShinobiCharacterBase* Viewer = GetLocalViewer(GetWorld());
	if (Viewer == this)
	{
		RefreshAllOutlines(GetWorld()); // everyone's colour is relative to this body's side
		return;
	}
	ApplyOutlineFor(Viewer);
}

const AShinobiCharacterBase* AShinobiCharacterBase::GetLocalViewer(const UWorld* World)
{
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(World) : nullptr;
	return PC ? Cast<AShinobiCharacterBase>(PC->GetPawn()) : nullptr;
}

void AShinobiCharacterBase::RefreshOutline()
{
	ApplyOutlineFor(GetLocalViewer(GetWorld()));
}

void AShinobiCharacterBase::ApplyOutlineFor(const AShinobiCharacterBase* Viewer)
{
	// A viewer's own body counts as friendly (IsHostileTo is false for itself).
	// ponytail: without a viewer (dedicated server, nobody possessed yet) the stencil is the old static rule, since only Enemies
	// were ever red; PvP characters are first drawn with it and corrected by RefreshAllOutlines when the local pawn arrives.
	ShinobiLook::EnableOutline(GetBodyMesh(), OutlineStencilFor(Viewer));
}

uint8 AShinobiCharacterBase::OutlineStencilFor(const AShinobiCharacterBase* Viewer) const
{
	const bool bEnemy = Viewer ? IsHostileTo(Viewer) : Team == EShinobiTeam::Enemies;
	return bEnemy ? ShinobiLook::StencilEnemy : ShinobiLook::StencilFriendly;
}

void AShinobiCharacterBase::RefreshAllOutlines(UWorld* World)
{
	if (!World)
	{
		return;
	}
	const AShinobiCharacterBase* Viewer = GetLocalViewer(World);
	for (TActorIterator<AShinobiCharacterBase> It(World); It; ++It)
	{
		It->ApplyOutlineFor(Viewer);
	}
}

void AShinobiCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AbilitySystem->RefreshAbilityActorInfo();
}

void AShinobiCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystem->RefreshAbilityActorInfo();
}

void AShinobiCharacterBase::HandleOutOfHealth()
{
	UE_LOG(LogShinobiCoop, Log, TEXT("%s is out of health"), *GetName());
}

bool AShinobiCharacterBase::IsAlive() const
{
	return !AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Downed) && !AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Dead);
}

bool AShinobiCharacterBase::IsPhased() const
{
	return AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_Phased);
}

void AShinobiCharacterBase::OnPhasedTagChanged(const FGameplayTag Tag, int32 Count)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Count > 0)
	{
		PrePhasePawnResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		return;
	}
	Capsule->SetCollisionResponseToChannel(ECC_Pawn, PrePhasePawnResponse);
	if (HasAuthority())
	{
		FVector Spot = GetActorLocation();
		if (GetWorld()->FindTeleportSpot(this, Spot, GetActorRotation()))
		{
			SetActorLocation(Spot); // out of the enemy it ended inside; clients get the correction
		}
	}
}

void AShinobiCharacterBase::OnDeadTagChanged(const FGameplayTag Tag, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetBodyMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetBodyMesh()->SetSimulatePhysics(true);
}

FName AShinobiCharacterBase::GetRightHandBone() const
{
	static const FName MannyHand(TEXT("hand_r"));
	static const FName MixamoHand(TEXT("RightHand"));
	return GetBodyMesh()->GetBoneIndex(MannyHand) != INDEX_NONE ? MannyHand : MixamoHand;
}

FName AShinobiCharacterBase::GetHeadBone() const
{
	static const FName MannyHead(TEXT("head"));
	static const FName MixamoHead(TEXT("Head"));
	return GetBodyMesh()->GetBoneIndex(MannyHead) != INDEX_NONE ? MannyHead : MixamoHead;
}

void AShinobiCharacterBase::UseMannequinMesh(bool bQuinn)
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> QuinnMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnim(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
	if (bQuinn ? QuinnMesh.Succeeded() : MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(bQuinn ? QuinnMesh.Object : MannyMesh.Object);
	}
	if (MannyAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(MannyAnim.Class); // replaced by the motion-matching AnimBP in BeginPlay
	}
	bUseLocomotionAnim = true;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
}

void AShinobiCharacterBase::GrantStartupAbilitiesAndEffects()
{
	if (bStartupGranted)
	{
		return;
	}
	bStartupGranted = true;

	for (const TSubclassOf<UShinobiGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!ensureMsgf(AbilityClass, TEXT("%s has an empty StartupAbilities entry"), *GetName()))
		{
			continue;
		}
		const UShinobiGameplayAbility* Ability = AbilityClass->GetDefaultObject<UShinobiGameplayAbility>();
		// Input ID 0 is a valid ASC input slot; None must map to INDEX_NONE so a stray
		// AbilityLocalInputPressed(0) doesn't activate every unbound ability.
		const int32 InputID = (Ability->InputID == EShinobiAbilityInputID::None) ? INDEX_NONE : static_cast<int32>(Ability->InputID);
		AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, InputID, this));
	}

	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (!ensureMsgf(EffectClass, TEXT("%s has an empty StartupEffects entry"), *GetName()))
		{
			continue;
		}
		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(EffectClass, 1.f, AbilitySystem->MakeEffectContext());
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}
