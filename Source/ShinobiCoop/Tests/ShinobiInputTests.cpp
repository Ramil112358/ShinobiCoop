#include "Tests/ShinobiTestWorld.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Shinobi/ShinobiInputConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FEnhancedActionKeyMapping* FindMapping(const TArray<FEnhancedActionKeyMapping>& Mappings, const UInputAction* Action, FKey Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action == Action && Mapping.Key == Key)
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	/** Runs a mapping's modifiers in order, starting from an Axis2D value of (1, 0). */
	FVector2D RunModifiers(const FEnhancedActionKeyMapping& Mapping)
	{
		FInputActionValue Value(EInputActionValueType::Axis2D, FVector(1.f, 0.f, 0.f));
		for (const TObjectPtr<UInputModifier>& Modifier : Mapping.Modifiers)
		{
			Value = Modifier->ModifyRaw(nullptr, Value, 0.f);
		}
		return Value.Get<FVector2D>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiInputMappingsTest, "ShinobiCoop.Input.MappingsAreCorrect", ShinobiTestFlags)
bool FShinobiInputMappingsTest::RunTest(const FString& Parameters)
{
	UShinobiInputConfig* Config = UShinobiInputConfig::Build(GetTransientPackage());
	const TArray<FEnhancedActionKeyMapping>& Mappings = Config->Context->GetMappings();

	const FEnhancedActionKeyMapping* WMapping = FindMapping(Mappings, Config->Move, EKeys::W);
	const FEnhancedActionKeyMapping* SMapping = FindMapping(Mappings, Config->Move, EKeys::S);
	const FEnhancedActionKeyMapping* AMapping = FindMapping(Mappings, Config->Move, EKeys::A);
	const FEnhancedActionKeyMapping* DMapping = FindMapping(Mappings, Config->Move, EKeys::D);
	if (TestNotNull(TEXT("W mapping exists"), WMapping))
	{
		TestEqual(TEXT("W drives forward"), RunModifiers(*WMapping), FVector2D(0.f, 1.f));
	}
	if (TestNotNull(TEXT("S mapping exists"), SMapping))
	{
		TestEqual(TEXT("S drives backward"), RunModifiers(*SMapping), FVector2D(0.f, -1.f));
	}
	if (TestNotNull(TEXT("A mapping exists"), AMapping))
	{
		TestEqual(TEXT("A drives left"), RunModifiers(*AMapping), FVector2D(-1.f, 0.f));
	}
	if (TestNotNull(TEXT("D mapping exists"), DMapping))
	{
		TestEqual(TEXT("D drives right"), RunModifiers(*DMapping), FVector2D(1.f, 0.f));
	}

	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : Config->AbilityActions)
	{
		int32 KeyCount = 0;
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			if (Mapping.Action == Pair.Value)
			{
				++KeyCount;
			}
		}
		TestEqual(*FString::Printf(TEXT("Ability input %d is bound to exactly two keys"), static_cast<int32>(Pair.Key)), KeyCount, 2);
	}

	const FEnhancedActionKeyMapping* LeftStick = FindMapping(Mappings, Config->Move, EKeys::Gamepad_Left2D);
	if (TestNotNull(TEXT("Left stick mapping exists"), LeftStick))
	{
		TestTrue(TEXT("Left stick's first modifier is a dead zone"),
			LeftStick->Modifiers.Num() > 0 && LeftStick->Modifiers[0]->IsA<UInputModifierDeadZone>());
	}

	const FEnhancedActionKeyMapping* RightStick = FindMapping(Mappings, Config->Look, EKeys::Gamepad_Right2D);
	if (TestNotNull(TEXT("Right stick mapping exists"), RightStick))
	{
		TestTrue(TEXT("Right stick's first modifier is a dead zone"),
			RightStick->Modifiers.Num() > 0 && RightStick->Modifiers[0]->IsA<UInputModifierDeadZone>());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
