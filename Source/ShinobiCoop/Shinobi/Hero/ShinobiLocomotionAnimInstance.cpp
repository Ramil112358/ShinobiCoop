#include "Shinobi/Hero/ShinobiLocomotionAnimInstance.h"
#include "GameFramework/Character.h"
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
			const UEnum* Enum = nullptr;
			if (const FByteProperty* Byte = CastField<FByteProperty>(*It))
			{
				Enum = Byte->Enum;
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(*It))
			{
				Enum = EnumProperty->GetEnum();
			}
			if (Enum)
			{
				TMap<FString, int64>& Values = EnumValues.Add(Name);
				for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // last entry is the generated _MAX
				{
					Values.Add(Enum->GetDisplayNameTextByIndex(Index).ToString(), Enum->GetValueByIndex(Index));
				}
			}
		}
	}

	template <typename T>
	void SetStruct(const TMap<FName, const FProperty*>& Fields, void* Container, FName Field, const T& Value)
	{
		const FStructProperty* Property = CastField<FStructProperty>(Fields.FindRef(Field));
		if (Property && Property->Struct == TBaseStructure<T>::Get())
		{
			*Property->ContainerPtrToValuePtr<T>(Container) = Value;
		}
	}

	void SetReal(const TMap<FName, const FProperty*>& Fields, void* Container, FName Field, double Value)
	{
		const FNumericProperty* Property = CastField<FNumericProperty>(Fields.FindRef(Field));
		if (Property && Property->IsFloatingPoint())
		{
			Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Container), Value);
		}
	}

	void SetBool(const TMap<FName, const FProperty*>& Fields, void* Container, FName Field, bool bValue)
	{
		if (const FBoolProperty* Property = CastField<FBoolProperty>(Fields.FindRef(Field)))
		{
			Property->SetPropertyValue_InContainer(Container, bValue);
		}
	}
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
	const int64* Value = Values ? Values->Find(DisplayName) : nullptr;
	if (!Value)
	{
		return;
	}
	const FProperty* Property = Fields.FindRef(Field);
	if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
	{
		Byte->SetPropertyValue_InContainer(Container, static_cast<uint8>(*Value));
	}
	else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Container), *Value);
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

	SetEnum(Properties, TEXT("MovementMode"), bOnGround ? TEXT("OnGround") : TEXT("InAir"));
	SetEnum(Properties, TEXT("Gait"), bSprint ? TEXT("Sprint") : bWalk ? TEXT("Walk") : TEXT("Run"));
	SetEnum(Properties, TEXT("Stance"), Movement->IsCrouching() ? TEXT("Crouch") : TEXT("Stand"));
	SetEnum(Properties, TEXT("RotationMode"), bStrafe ? TEXT("Strafe") : TEXT("OrientToMovement"));
	SetStruct(Fields, Properties, TEXT("ActorTransform"), Character->GetActorTransform());
	SetStruct(Fields, Properties, TEXT("Velocity"), Movement->Velocity);
	SetStruct(Fields, Properties, TEXT("InputAcceleration"), Acceleration);
	SetReal(Fields, Properties, TEXT("CurrentMaxAcceleration"), Movement->GetMaxAcceleration());
	SetReal(Fields, Properties, TEXT("CurrentMaxDeceleration"), Movement->GetMaxBrakingDeceleration());
	SetStruct(Fields, Properties, TEXT("OrientationIntent"), Acceleration.IsNearlyZero() ? Character->GetActorRotation() : Acceleration.Rotation());
	SetStruct(Fields, Properties, TEXT("AimingRotation"), Character->GetBaseAimRotation());
	SetBool(Fields, Properties, TEXT("JustLanded"), Now - LandedTime < JustLandedSeconds);
	SetStruct(Fields, Properties, TEXT("LandVelocity"), LandVelocity);
	SetStruct(Fields, Properties, TEXT("GroundNormal"), FVector(Movement->CurrentFloor.HitResult.ImpactNormal));
	SetStruct(Fields, Properties, TEXT("GroundLocation"), FVector(Movement->CurrentFloor.HitResult.ImpactPoint));

	if (InputStateProperty)
	{
		void* InputState = InputStateProperty->ContainerPtrToValuePtr<void>(Properties);
		SetBool(Fields, InputState, TEXT("WantsToSprint"), bSprint);
		SetBool(Fields, InputState, TEXT("WantsToWalk"), bWalk);
		SetBool(Fields, InputState, TEXT("WantsToStrafe"), bStrafe);
		SetBool(Fields, InputState, TEXT("WantsToCrouch"), Movement->bWantsToCrouch);
	}
}
