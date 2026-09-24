#include "Shinobi/ShinobiCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"

AShinobiCharacterBase::AShinobiCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShinobiMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	Attributes = CreateDefaultSubobject<UShinobiAttributeSet>(TEXT("Attributes"));

	StartupEffects = { UGE_ChakraRegen::StaticClass(), UGE_StaminaRegen::StaticClass() };
}

UAbilitySystemComponent* AShinobiCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AShinobiCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
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

void AShinobiCharacterBase::UseMannequinMesh()
{
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> MannyAnim(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
	if (MannyMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(MannyMesh.Object);
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
