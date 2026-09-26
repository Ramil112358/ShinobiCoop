#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/Enemies/ShinobiAIController.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"

AShinobiEnemyCharacter::AShinobiEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Enemies;
	AIControllerClass = AShinobiAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Face the AI's focus (the target), not the direction of travel, so ranged enemies can back off while aiming.
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = 450.f;
	Movement->bUseRVOAvoidance = true; // don't stack up on the same player

	HealthText = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);
}

void AShinobiEnemyCharacter::BeginPlay()
{
	if (HasAuthority())
	{
		AbilitySystem->SetNumericAttributeBase(UShinobiAttributeSet::GetMaxHealthAttribute(), BaseHealth * HealthScale);
		AbilitySystem->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), BaseHealth * HealthScale);
	}
	if (!IsNetMode(NM_DedicatedServer))
	{
		for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = GetMesh()->CreateDynamicMaterialInstance(Slot))
			{
				Material->SetVectorParameterValue(TEXT("Paint Tint"), BodyTint);
			}
		}
	}
	Super::BeginPlay();
}

bool AShinobiEnemyCharacter::IsBusy() const
{
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.IsActive())
		{
			return true;
		}
	}
	return false;
}

void AShinobiEnemyCharacter::HandleOutOfHealth()
{
	Super::HandleOutOfHealth();
	if (!IsAlive())
	{
		return;
	}
	AbilitySystem->CancelAllAbilities();
	AbilitySystem->ApplyGameplayEffectToSelf(GetDefault<UGE_Dead>(), 1.f, AbilitySystem->MakeEffectContext());
	SetLifeSpan(DeathLifeSpan);
	OnDied.Broadcast(this);
}

AShinobiRogueNinja::AShinobiRogueNinja(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BaseHealth = 200.f;
	AttackDamage = 25.f;
	AttackAbility = UGA_EnemyMelee::StaticClass();
	StartupAbilities = { AttackAbility };
	UseMannequinMesh();
}

AShinobiKunaiThrower::AShinobiKunaiThrower(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BaseHealth = 120.f;
	AttackDamage = 30.f;
	AttackRange = 1500.f;
	PreferredRange = 800.f;
	AttackInterval = 2.2f;
	AttackAbility = UGA_EnemyKunai::StaticClass();
	BodyTint = FLinearColor(0.35f, 0.08f, 0.55f);
	StartupAbilities = { AttackAbility };
	GetCharacterMovement()->MaxWalkSpeed = 400.f;
	UseMannequinMesh(true);
}

AShinobiRogueJonin::AShinobiRogueJonin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BaseHealth = 2000.f;
	AttackDamage = 50.f;
	AttackRange = 200.f;
	AttackInterval = 1.4f;
	StunMultiplier = 0.33f;
	BodyTint = FLinearColor(0.05f, 0.05f, 0.06f);
	AttackAbility = UGA_EnemyMelee::StaticClass();
	SpecialAbility = UGA_EnemyFireball::StaticClass();
	StartupAbilities = { AttackAbility, SpecialAbility };
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	UseMannequinMesh();
	GetCapsuleComponent()->SetRelativeScale3D(FVector(1.3f)); // ponytail: a bigger Manny reads as "boss" until M5's models
}
