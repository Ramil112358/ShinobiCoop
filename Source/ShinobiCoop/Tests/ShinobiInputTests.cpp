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

	const TArray<FEnhancedActionKeyMapping>& JutsuMappings = Config->JutsuContext->GetMappings();
	const auto CountFor = [](const TArray<FEnhancedActionKeyMapping>& List, const UInputAction* Action)
	{
		int32 Count = 0;
		for (const FEnhancedActionKeyMapping& Mapping : List)
		{
			Count += Mapping.Action == Action ? 1 : 0;
		}
		return Count;
	};
	int32 JutsuActions = 0;
	for (const TPair<EShinobiAbilityInputID, TObjectPtr<UInputAction>>& Pair : Config->AbilityActions)
	{
		// Jutsu: a number key in IMC_Shinobi, a face button in IMC_ShinobiJutsu (active only while RB is held).
		// Everything else: keyboard + gamepad in IMC_Shinobi.
		const bool bJutsu = Pair.Key >= EShinobiAbilityInputID::Jutsu1;
		JutsuActions += bJutsu ? 1 : 0;
		const int32 ID = static_cast<int32>(Pair.Key);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_Shinobi"), ID), CountFor(Mappings, Pair.Value), bJutsu ? 1 : 2);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_ShinobiJutsu"), ID), CountFor(JutsuMappings, Pair.Value), bJutsu ? 1 : 0);
	}
	TestEqual(TEXT("Four jutsu actions"), JutsuActions, 4);
	TestNotNull(TEXT("RB holds the jutsu modifier"), FindMapping(Mappings, Config->JutsuModifier, EKeys::Gamepad_RightShoulder));
	TestNotNull(TEXT("Key 1 casts jutsu 1"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu1], EKeys::One));
	TestNotNull(TEXT("Key 4 casts jutsu 4"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu4], EKeys::Four));
	TestNotNull(TEXT("RB + A casts jutsu 1"), FindMapping(JutsuMappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu1], EKeys::Gamepad_FaceButton_Bottom));
	TestNotNull(TEXT("RB + Y casts jutsu 4"), FindMapping(JutsuMappings, Config->AbilityActions[EShinobiAbilityInputID::Jutsu4], EKeys::Gamepad_FaceButton_Top));

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
