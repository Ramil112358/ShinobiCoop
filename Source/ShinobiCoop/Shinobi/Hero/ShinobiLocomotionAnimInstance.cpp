#include "Shinobi/Hero/ShinobiLocomotionAnimInstance.h"
#include "GameFramework/Character.h"
#include "Shinobi/Hero/ShinobiBlueprintStruct.h"
#include "Shinobi/ShinobiMovementComponent.h"

namespace
{
	const FName CharacterPropertiesName(TEXT("CharacterProperties"));
	const FName InputStateName(TEXT("InputState"));

	void AddFields(const UStruct* Struct, TMap<FName, const FProperty*>& Fields, TMap<FName, TMap<FString, int64>>& EnumValues)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FName Name(*It->GetAuthoredName());
			Fields.Add(Name, *It);
			if (const UEnum* Enum = ShinobiBlueprintStruct::GetEnum(*It))
			{
				TMap<FString, int64>& Values = EnumValues.Add(Name);
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // last entry is the generated _MAX
				{
					Values.Add(Enum->GetDisplayNameTextByIndex(Index).ToString(), Enum->GetValueByIndex(Index));
				}
			}
		}
	}
}

UClass* UShinobiLocomotionAnimInstance::LoadAnimBlueprintClass()
{
	static const TSoftClassPtr<UAnimInstance> AnimBlueprint(FSoftObjectPath(TEXT("/Game/Blueprints/SandboxCharacter_CMC_ABP.SandboxCharacter_CMC_ABP_C")));
	return AnimBlueprint.LoadSynchronous();
}

void UShinobiLocomotionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	PropertiesProperty = CastField<FStructProperty>(GetClass()->FindPropertyByName(CharacterPropertiesName));
	InputStateProperty = nullptr;
	Fields.Reset();
	EnumValues.Reset();
	if (!PropertiesProperty)
	{
		return; // the class default object of this native class, or an AnimBP without the sample's variable
	}
	AddFields(PropertiesProperty->Struct, Fields, EnumValues);
	InputStateProperty = CastField<FStructProperty>(Fields.FindRef(InputStateName));
	if (InputStateProperty)
	{
		AddFields(InputStateProperty->Struct, Fields, EnumValues);
	}
}

void UShinobiLocomotionAnimInstance::SetEnum(void* Container, FName Field, const TCHAR* DisplayName) const
{
	const TMap<FString, int64>* Values = EnumValues.Find(Field);
	if (const int64* Value = Values ? Values->Find(DisplayName) : nullptr)
	{
		ShinobiBlueprintStruct::SetEnum(Fields.FindRef(Field), Container, *Value);
	}
}

void UShinobiLocomotionAnimInstance::UpdateCharacterProperties()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !PropertiesProperty)
	{
		return;
	}
	void* Properties = PropertiesProperty->ContainerPtrToValuePtr<void>(this);
	auto Field = [this](const TCHAR* Name) { return Fields.FindRef(FName(Name)); };

	const bool bOnGround = Movement->IsMovingOnGround(); // also true on a wall (wall running walks on custom gravity)
	const double Now = GetWorld()->GetTimeSeconds();
	if (bOnGround && bWasInAir)
	{
		LandedTime = Now;
		LandVelocity = LastAirVelocity;
	}
	bWasInAir = !bOnGround;
	if (!bOnGround)
	{
		LastAirVelocity = Movement->Velocity;
	}

	// Sprint isn't replicated to other machines' copies of a character, so its speed decides there too.
	const UShinobiMovementComponent* ShinobiMovement = Cast<UShinobiMovementComponent>(Movement);
	const bool bSprint = (ShinobiMovement && ShinobiMovement->IsSprinting()) || Movement->Velocity.Size() > Movement->MaxWalkSpeed + 50.f;
	const bool bWalk = !bSprint && Movement->MaxWalkSpeed <= WalkGaitMaxSpeed;
	const bool bStrafe = !Movement->bOrientRotationToMovement;
	const FVector Acceleration = Movement->GetCurrentAcceleration();

	using namespace ShinobiBlueprintStruct;
	SetEnum(Properties, TEXT("MovementMode"), bOnGround ? TEXT("OnGround") : TEXT("InAir"));
	SetEnum(Properties, TEXT("Gait"), bSprint ? TEXT("Sprint") : bWalk ? TEXT("Walk") : TEXT("Run"));
	SetEnum(Properties, TEXT("Stance"), Movement->IsCrouching() ? TEXT("Crouch") : TEXT("Stand"));
	SetEnum(Properties, TEXT("RotationMode"), bStrafe ? TEXT("Strafe") : TEXT("OrientToMovement"));
	SetStruct(Field(TEXT("ActorTransform")), Properties, Character->GetActorTransform());
	SetStruct(Field(TEXT("Velocity")), Properties, Movement->Velocity);
	SetStruct(Field(TEXT("InputAcceleration")), Properties, Acceleration);
	SetReal(Field(TEXT("CurrentMaxAcceleration")), Properties, Movement->GetMaxAcceleration());
	SetReal(Field(TEXT("CurrentMaxDeceleration")), Properties, Movement->GetMaxBrakingDeceleration());
	SetStruct(Field(TEXT("OrientationIntent")), Properties, Acceleration.IsNearlyZero() ? Character->GetActorRotation() : Acceleration.Rotation());
	SetStruct(Field(TEXT("AimingRotation")), Properties, Character->GetBaseAimRotation());
	SetBool(Field(TEXT("JustLanded")), Properties, Now - LandedTime < JustLandedSeconds);
	SetStruct(Field(TEXT("LandVelocity")), Properties, LandVelocity);
	SetStruct(Field(TEXT("GroundNormal")), Properties, FVector(Movement->CurrentFloor.HitResult.ImpactNormal));
	SetStruct(Field(TEXT("GroundLocation")), Properties, FVector(Movement->CurrentFloor.HitResult.ImpactPoint));

	if (InputStateProperty)
	{
		void* InputState = InputStateProperty->ContainerPtrToValuePtr<void>(Properties);
		SetBool(Field(TEXT("WantsToSprint")), InputState, bSprint);
		SetBool(Field(TEXT("WantsToWalk")), InputState, bWalk);
		SetBool(Field(TEXT("WantsToStrafe")), InputState, bStrafe);
		SetBool(Field(TEXT("WantsToCrouch")), InputState, Movement->bWantsToCrouch);
	}
}
