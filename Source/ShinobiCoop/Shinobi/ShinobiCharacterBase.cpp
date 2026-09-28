#include "Shinobi/ShinobiCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"
#include "Shinobi/VFX/ShinobiCastFxComponent.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Shinobi/VFX/ShinobiFeedbackComponent.h"

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
	Feedback = CreateDefaultSubobject<UShinobiFeedbackComponent>(TEXT("Feedback"));

	StartupEffects = { UGE_ChakraRegen::StaticClass(), UGE_StaminaRegen::StaticClass() };
}

UAbilitySystemComponent* AShinobiCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AShinobiCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	// Outline the visible body (the hero, when a hidden Manny drives it); the driver itself stays out of custom depth.
	ShinobiLook::EnableOutline(GetBodyMesh(), Team == EShinobiTeam::Enemies ? ShinobiLook::StencilEnemy : ShinobiLook::StencilFriendly);
	if (GetBodyMesh() != GetMesh())
	{
		GetMesh()->SetRenderCustomDepth(false);
	}
	AbilitySystem->InitAbilityActorInfo(this, this);
	AbilitySystem->RegisterGameplayTagEvent(ShinobiTags::State_Dead).AddUObject(this, &AShinobiCharacterBase::OnDeadTagChanged);
	if (HasAuthority())
	{
		Attributes->OnOutOfHealth.AddUObject(this, &AShinobiCharacterBase::HandleOutOfHealth);
		GrantStartupAbilitiesAndEffects();
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
		GetMesh()->SetAnimInstanceClass(MannyAnim.Class);
	}
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
