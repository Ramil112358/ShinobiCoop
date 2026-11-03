#include "Tests/ShinobiTestWorld.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
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
		// Jutsu slots: one keyboard key in IMC_Shinobi and one pad button in IMC_ShinobiJutsu (RB held).
		// Everything else: keyboard + gamepad in IMC_Shinobi.
		const bool bJutsu = ShinobiLoadout::SlotIndexOf(Pair.Key) != INDEX_NONE;
		JutsuActions += bJutsu ? 1 : 0;
		const int32 ID = static_cast<int32>(Pair.Key);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_Shinobi"), ID), CountFor(Mappings, Pair.Value), bJutsu ? 1 : 2);
		TestEqual(*FString::Printf(TEXT("Ability input %d: keys in IMC_ShinobiJutsu"), ID), CountFor(JutsuMappings, Pair.Value), bJutsu ? 1 : 0);
	}
	TestEqual(TEXT("Eight jutsu slot actions"), JutsuActions, 8);
	TestNotNull(TEXT("RB holds the jutsu modifier"), FindMapping(Mappings, Config->JutsuModifier, EKeys::Gamepad_RightShoulder));
	const FKey SlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::R, EKeys::T, EKeys::G, EKeys::CapsLock };
	for (int32 Slot = 0; Slot < ShinobiLoadout::NumSlots; ++Slot)
	{
		TestNotNull(*FString::Printf(TEXT("Slot %d on %s"), Slot + 1, *SlotKeys[Slot].ToString()),
			FindMapping(Mappings, Config->AbilityActions[ShinobiLoadout::SlotInputID(Slot)], SlotKeys[Slot]));
	}
	const FKey PadButtons[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top,
		EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left };
	for (int32 Slot = 0; Slot < ShinobiLoadout::NumSlots; ++Slot)
	{
		TestNotNull(*FString::Printf(TEXT("Slot %d on RB + %s"), Slot + 1, *PadButtons[Slot].ToString()),
			FindMapping(JutsuMappings, Config->AbilityActions[ShinobiLoadout::SlotInputID(Slot)], PadButtons[Slot]));
	}
	TestNotNull(TEXT("K opens the Skills screen"), FindMapping(Mappings, Config->SkillsScreen, EKeys::K));
	TestNotNull(TEXT("Left Alt is the modifier"), FindMapping(Mappings, Config->AltModifier, EKeys::LeftAlt));
	TestNotNull(TEXT("Right Alt is the modifier"), FindMapping(Mappings, Config->AltModifier, EKeys::RightAlt));
	TestNotNull(TEXT("E revives"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Revive], EKeys::E));
	TestNotNull(TEXT("F guards"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Block], EKeys::F));
	TestNotNull(TEXT("LT guards"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Block], EKeys::Gamepad_LeftTrigger));
	TestNotNull(TEXT("Y (without RB) revives"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Revive], EKeys::Gamepad_FaceButton_Top));

	TestNotNull(TEXT("Mouse wheel controls camera zoom"), FindMapping(Mappings, Config->CameraZoom, EKeys::MouseWheelAxis));
	TestNotNull(TEXT("Right mouse holds camera free-look"), FindMapping(Mappings, Config->CameraFreeLook, EKeys::RightMouseButton));
	TestNotNull(TEXT("D-pad down (without RB) dismounts"), FindMapping(Mappings, Config->AbilityActions[EShinobiAbilityInputID::Dismount], EKeys::Gamepad_DPad_Down));

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
