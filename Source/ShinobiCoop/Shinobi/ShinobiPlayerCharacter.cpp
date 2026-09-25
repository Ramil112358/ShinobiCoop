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
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"

AShinobiPlayerCharacter::AShinobiPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	StartupAbilities = { UGA_Melee::StaticClass(), UGA_Dodge::StaticClass(), UGA_Sprint::StaticClass(), UGA_ChakraCharge::StaticClass() };

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

void AShinobiPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	const UShinobiInputConfig& InputConfig = UShinobiInputConfig::Get();

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputConfig.Context, ShinobiInputPriority);
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
}

void AShinobiPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller || AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))
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
	if (AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_ChakraCharging))
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
