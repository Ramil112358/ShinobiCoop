#include "Shinobi/ShinobiPlayerCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Substitution.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = {
		UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass(),
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
		UGA_Substitution::StaticClass(),
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
	CameraBoom->TargetArmLength = 450.f;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	ChakraAura = CreateDefaultSubobject<UShinobiChakraAuraComponent>(TEXT("ChakraAura"));
	ChakraAura->SetupAttachment(RootComponent);

	UseMannequinMesh();
}

bool AShinobiPlayerCharacter::CanMove() const
{
	static const FGameplayTagContainer Blockers = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ShinobiTags::State_ChakraCharging);
		Tags.AddTag(ShinobiTags::State_Shocked);
		Tags.AddTag(ShinobiTags::State_Staggered);
		return Tags;
	}();
	return !AbilitySystem->HasAnyMatchingGameplayTags(Blockers);
}

void AShinobiPlayerCharacter::UnPossessed()
{
	// A lost controller (unpossess, disconnect) never sends the release for held inputs, and chakra charge never
	// ends on its own, so end the hold-to-use abilities here. Must run before Super: APawn::UnPossessed clears
	// the Owner, after which the ClientCancelAbility RPC can't reach the owning client. Mostly server-side; on a
	// client it can also run while the pawn is being destroyed (harmless).
	FGameplayTagContainer HeldAbilities;
	HeldAbilities.AddTag(ShinobiTags::Ability_Sprint);
	HeldAbilities.AddTag(ShinobiTags::Ability_ChakraCharge);
	AbilitySystem->CancelAbilities(&HeldAbilities);
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
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::JumpPressed);
	Input->BindAction(InputConfig.Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : InputConfig.AbilityActions)
	{
		Input->BindAction(Pair.Value, ETriggerEvent::Started, this, &AShinobiPlayerCharacter::AbilityInputPressed, Pair.Key);
		Input->BindAction(Pair.Value, ETriggerEvent::Completed, this, &AShinobiPlayerCharacter::AbilityInputReleased, Pair.Key);
	}
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
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void AShinobiPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AShinobiPlayerCharacter::JumpPressed()
{
	if (!CanMove())
	{
		return;
	}
	Jump();
}

void AShinobiPlayerCharacter::AbilityInputPressed(EShinobiAbilityInputID InputID)
{
	AbilitySystem->AbilityLocalInputPressed(static_cast<int32>(InputID));
}

void AShinobiPlayerCharacter::AbilityInputReleased(EShinobiAbilityInputID InputID)
{
	AbilitySystem->AbilityLocalInputReleased(static_cast<int32>(InputID));
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
