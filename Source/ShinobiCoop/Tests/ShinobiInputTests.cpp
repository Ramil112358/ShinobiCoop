#include "Tests/ShinobiTestWorld.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

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

	// Every rebindable action: its keyboard/mouse binding and, when it has one, its controller binding, with RB / RT combos
	// chorded on the matching hold action (whatever this machine has saved: Build() applies it).
	const auto CountFor = [&Mappings](const UInputAction* Action)
	{
		int32 Count = 0;
		for (const FEnhancedActionKeyMapping& Mapping : Mappings)
		{
			Count += Mapping.Action == Action ? 1 : 0;
		}
		return Count;
	};
	const auto ChordOf = [](const FEnhancedActionKeyMapping& Mapping) -> const UInputAction*
	{
		for (const TObjectPtr<UInputTrigger>& Trigger : Mapping.Triggers)
		{
			if (const UInputTriggerChordAction* Chord = Cast<UInputTriggerChordAction>(Trigger))
			{
				return Chord->ChordAction;
			}
		}
		return nullptr;
	};
	for (int32 Index = 0; Index < static_cast<int32>(ShinobiKeyBindings::EBindable::Num); ++Index)
	{
		const ShinobiKeyBindings::EBindable Which = static_cast<ShinobiKeyBindings::EBindable>(Index);
		const FString Name = ShinobiKeyBindings::Label(Which).ToString();
		const UInputAction* Action = Config->ActionForBindable(static_cast<uint8>(Index));
		if (!TestNotNull(*FString::Printf(TEXT("%s has an action"), *Name), Action))
		{
			continue;
		}
		const ShinobiKeyBindings::FPadBinding Pad = ShinobiKeyBindings::GetPad(Which);
		TestEqual(*FString::Printf(TEXT("%s: one key, plus one button if it has one"), *Name), CountFor(Action), Pad.IsBound() ? 2 : 1);
		TestNotNull(*FString::Printf(TEXT("%s on its key"), *Name), FindMapping(Mappings, Action, ShinobiKeyBindings::Get(Which).Key));
		if (Pad.IsBound())
		{
			const FEnhancedActionKeyMapping* PadMapping = FindMapping(Mappings, Action, Pad.Button);
			if (TestNotNull(*FString::Printf(TEXT("%s on its button"), *Name), PadMapping))
			{
				const UInputAction* Hold = !Pad.Hold.IsValid() ? nullptr : Pad.Hold == EKeys::Gamepad_RightTrigger ? Config->PadHoldRT.Get() : Config->PadHoldRB.Get();
				TestTrue(*FString::Printf(TEXT("%s: chorded on its hold button, or on nothing"), *Name), ChordOf(*PadMapping) == Hold);
			}
		}
	}
	TestNotNull(TEXT("RB holds for combos"), FindMapping(Mappings, Config->PadHoldRB, EKeys::Gamepad_RightShoulder));
	TestNotNull(TEXT("RT holds for combos"), FindMapping(Mappings, Config->PadHoldRT, EKeys::Gamepad_RightTrigger));
	TestNotNull(TEXT("Esc pauses"), FindMapping(Mappings, Config->Pause, EKeys::Escape));
	TestNotNull(TEXT("Menu pauses"), FindMapping(Mappings, Config->Pause, EKeys::Gamepad_Special_Right));
	TestNotNull(TEXT("Left Alt is the modifier"), FindMapping(Mappings, Config->AltModifier, EKeys::LeftAlt));
	TestNotNull(TEXT("Right Alt is the modifier"), FindMapping(Mappings, Config->AltModifier, EKeys::RightAlt));

	TestNotNull(TEXT("Mouse wheel controls camera zoom"), FindMapping(Mappings, Config->CameraZoom, EKeys::MouseWheelAxis));
	TestNotNull(TEXT("Right mouse holds camera free-look"), FindMapping(Mappings, Config->CameraFreeLook, EKeys::RightMouseButton));

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
