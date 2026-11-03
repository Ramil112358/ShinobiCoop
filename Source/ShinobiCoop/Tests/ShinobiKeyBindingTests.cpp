#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiInputConfig.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"

namespace
{
	using ShinobiKeyBindings::EBindable;
	using ShinobiKeyBindings::FBinding;
	using ShinobiKeyBindings::ERebind;

	/** Keeps the player's real shortcuts: snapshots the ini section and puts it back (and re-applies) at the end of a test. */
	struct FSavedBindings
	{
		const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiKeyBindings");
		TArray<FString> Lines;
		bool bHad = false;

		FSavedBindings()
		{
			bHad = GConfig->GetSection(Section, Lines, GGameUserSettingsIni);
			ShinobiKeyBindings::ResetToDefaults();
		}

		~FSavedBindings()
		{
			GConfig->EmptySection(Section, GGameUserSettingsIni);
			for (const FString& Line : Lines)
			{
				FString Key, Value;
				if (Line.Split(TEXT("="), &Key, &Value))
				{
					GConfig->SetString(Section, *Key, *Value, GGameUserSettingsIni);
				}
			}
			UShinobiInputConfig::Get().ApplyKeyBindings();
		}
	};

	const FEnhancedActionKeyMapping* FindKey(const UInputAction* Action, const FKey& Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : UShinobiInputConfig::Get().Context->GetMappings())
		{
			if (Mapping.Action == Action && Mapping.Key == Key)
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	const UInputAction* SlotAction(int32 SlotIndex)
	{
		return UShinobiInputConfig::Get().AbilityActions[ShinobiLoadout::SlotInputID(SlotIndex)];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingDefaultsTest, "ShinobiCoop.KeyBindings.Defaults", ShinobiTestFlags)
bool FShinobiKeyBindingDefaultsTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	TestTrue(TEXT("Slot 5 on R"), ShinobiKeyBindings::Get(EBindable::Slot5).Key == EKeys::R);
	TestTrue(TEXT("Slot 8 on Caps Lock"), ShinobiKeyBindings::Get(EBindable::Slot8).Key == EKeys::CapsLock);
	TestTrue(TEXT("Skills screen on K"), ShinobiKeyBindings::Get(EBindable::SkillsScreen).Key == EKeys::K);
	TestTrue(TEXT("Melee on left mouse"), ShinobiKeyBindings::Get(EBindable::Melee).Key == EKeys::LeftMouseButton);
	TestTrue(TEXT("Map on M"), ShinobiKeyBindings::Get(EBindable::Map).Key == EKeys::M);
	TestFalse(TEXT("No default uses Alt"), ShinobiKeyBindings::Get(EBindable::Slot1).bAlt);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingSwapTest, "ShinobiCoop.KeyBindings.SwapOnConflict", ShinobiTestFlags)
bool FShinobiKeyBindingSwapTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	EBindable SwappedWith = EBindable::Num;
	TestTrue(TEXT("G is taken: a swap"), ShinobiKeyBindings::Rebind(EBindable::Slot8, { EKeys::G }, &SwappedWith) == ERebind::Swapped);
	TestTrue(TEXT("with slot 7"), SwappedWith == EBindable::Slot7);
	TestTrue(TEXT("Slot 7 gets Caps Lock"), ShinobiKeyBindings::Get(EBindable::Slot7).Key == EKeys::CapsLock);
	TestTrue(TEXT("Slot 8 gets G"), ShinobiKeyBindings::Get(EBindable::Slot8).Key == EKeys::G);
	TestTrue(TEXT("A free key is a plain rebind"), ShinobiKeyBindings::Rebind(EBindable::Slot1, { EKeys::Q }) == ERebind::Done);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingBlockedTest, "ShinobiCoop.KeyBindings.Blocked", ShinobiTestFlags)
bool FShinobiKeyBindingBlockedTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	const FBinding Blocked[] = { { EKeys::Escape }, { EKeys::F4, true }, { EKeys::Tab, true }, { EKeys::W }, { EKeys::RightMouseButton },
		{ EKeys::LeftCommand }, { EKeys::Gamepad_FaceButton_Bottom }, { EKeys::LeftAlt } };
	for (const FBinding& Binding : Blocked)
	{
		TestTrue(*FString::Printf(TEXT("%s refused"), *ShinobiKeyBindings::KeyLabel(Binding).ToString()),
			ShinobiKeyBindings::Rebind(EBindable::Slot1, Binding) == ERebind::Blocked);
	}
	TestTrue(TEXT("Slot 1 keeps 1"), ShinobiKeyBindings::Get(EBindable::Slot1) == ShinobiKeyBindings::Default(EBindable::Slot1));
	TestFalse(TEXT("Plain F4 is fine"), ShinobiKeyBindings::IsBlocked({ EKeys::F4 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingAltResetTest, "ShinobiCoop.KeyBindings.AltAndReset", ShinobiTestFlags)
bool FShinobiKeyBindingAltResetTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	ShinobiKeyBindings::Rebind(EBindable::Slot1, { EKeys::One, true });
	TestEqual(TEXT("Alt label"), ShinobiKeyBindings::KeyLabel(ShinobiKeyBindings::Get(EBindable::Slot1)).ToString(), FString(TEXT("Alt+1")));
	TestEqual(TEXT("Short Caps label"), ShinobiKeyBindings::KeyLabel({ EKeys::CapsLock }, true).ToString(), FString(TEXT("Caps")));
	ShinobiKeyBindings::ResetToDefaults();
	TestTrue(TEXT("Reset restores slot 1"), ShinobiKeyBindings::Get(EBindable::Slot1) == ShinobiKeyBindings::Default(EBindable::Slot1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingChordTest, "ShinobiCoop.Input.AltBindingsChordAndBlock", ShinobiTestFlags)
bool FShinobiKeyBindingChordTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	ShinobiKeyBindings::Rebind(EBindable::Slot2, { EKeys::One, true });
	const FEnhancedActionKeyMapping* Alt1 = FindKey(SlotAction(1), EKeys::One);
	if (!TestNotNull(TEXT("Slot 2 is on 1 (with Alt)"), Alt1))
	{
		return false;
	}
	const UInputTriggerChordAction* Chord = nullptr;
	for (const TObjectPtr<UInputTrigger>& Trigger : Alt1->Triggers)
	{
		Chord = Chord ? Chord : Cast<UInputTriggerChordAction>(Trigger);
	}
	TestTrue(TEXT("chorded on the Alt modifier"), Chord && Chord->ChordAction == UShinobiInputConfig::Get().AltModifier);
	const FEnhancedActionKeyMapping* Plain1 = FindKey(SlotAction(0), EKeys::One);
	TestTrue(TEXT("Slot 1 keeps plain 1 with no chord (Enhanced Input blocks it while Alt is held)"), Plain1 && Plain1->Triggers.Num() == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiKeyBindingLiveTest, "ShinobiCoop.KeyBindings.LiveRemap", ShinobiTestFlags)
bool FShinobiKeyBindingLiveTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	ShinobiKeyBindings::Rebind(EBindable::Slot1, { EKeys::ThumbMouseButton });
	TestNotNull(TEXT("Slot 1 now on Mouse 4"), FindKey(SlotAction(0), EKeys::ThumbMouseButton));
	TestNull(TEXT("and no longer on 1"), FindKey(SlotAction(0), EKeys::One));
	ShinobiKeyBindings::Rebind(EBindable::Melee, { EKeys::MiddleMouseButton });
	const UInputAction* Melee = UShinobiInputConfig::Get().AbilityActions[EShinobiAbilityInputID::Melee];
	TestNotNull(TEXT("Melee on the middle button"), FindKey(Melee, EKeys::MiddleMouseButton));
	TestNotNull(TEXT("Melee keeps its gamepad X"), FindKey(Melee, EKeys::Gamepad_FaceButton_Left));
	return true;
}

namespace
{
	using ShinobiKeyBindings::FPadBinding;

	/** The mapping of Which's action on Button in the live context, or null. */
	const FEnhancedActionKeyMapping* PadMappingOf(EBindable Which, const FKey& Button)
	{
		return FindKey(UShinobiInputConfig::Get().ActionForBindable(static_cast<uint8>(Which)), Button);
	}

	const UInputAction* ChordOf(const FEnhancedActionKeyMapping* Mapping)
	{
		for (const TObjectPtr<UInputTrigger>& Trigger : Mapping ? Mapping->Triggers : TArray<TObjectPtr<UInputTrigger>>())
		{
			if (const UInputTriggerChordAction* Chord = Cast<UInputTriggerChordAction>(Trigger))
			{
				return Chord->ChordAction;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPadDefaultsTest, "ShinobiCoop.KeyBindings.PadDefaults", ShinobiTestFlags)
bool FShinobiPadDefaultsTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	TestTrue(TEXT("Slot 1 on RB + A"), ShinobiKeyBindings::GetPad(EBindable::Slot1) == FPadBinding{ EKeys::Gamepad_RightShoulder, EKeys::Gamepad_FaceButton_Bottom });
	TestTrue(TEXT("Slot 8 on RB + D-pad left"), ShinobiKeyBindings::GetPad(EBindable::Slot8) == FPadBinding{ EKeys::Gamepad_RightShoulder, EKeys::Gamepad_DPad_Left });
	TestTrue(TEXT("Jump on A"), ShinobiKeyBindings::GetPad(EBindable::Jump) == FPadBinding{ FKey(), EKeys::Gamepad_FaceButton_Bottom });
	TestTrue(TEXT("Chakra charge on LB"), ShinobiKeyBindings::GetPad(EBindable::ChakraCharge) == FPadBinding{ FKey(), EKeys::Gamepad_LeftShoulder });
	TestTrue(TEXT("Block on LT"), ShinobiKeyBindings::GetPad(EBindable::Block) == FPadBinding{ FKey(), EKeys::Gamepad_LeftTrigger });
	TestTrue(TEXT("Skills screen on D-pad left"), ShinobiKeyBindings::GetPad(EBindable::SkillsScreen) == FPadBinding{ FKey(), EKeys::Gamepad_DPad_Left });
	TestFalse(TEXT("Map zoom out has no button"), ShinobiKeyBindings::GetPad(EBindable::MapZoomOut).IsBound());
	TestTrue(TEXT("Map zoom in on ="), ShinobiKeyBindings::Get(EBindable::MapZoomIn).Key == EKeys::Equals);
	TestTrue(TEXT("Map zoom out on -"), ShinobiKeyBindings::Get(EBindable::MapZoomOut).Key == EKeys::Hyphen);
	for (int32 Index = 0; Index < static_cast<int32>(EBindable::Num); ++Index)
	{
		const FPadBinding Pad = ShinobiKeyBindings::GetPad(static_cast<EBindable>(Index));
		TestFalse(TEXT("RB / RT never act alone"), ShinobiKeyBindings::IsPadHold(Pad.Button));
	}
	TestEqual(TEXT("HUD label"), ShinobiKeyBindings::PadLabel({ EKeys::Gamepad_RightShoulder, EKeys::Gamepad_FaceButton_Bottom }, true).ToString(), FString(TEXT("RB+A")));
	TestEqual(TEXT("Menu label"), ShinobiKeyBindings::PadLabel({ EKeys::Gamepad_RightTrigger, EKeys::Gamepad_DPad_Up }).ToString(), FString(TEXT("RT + D-pad Up")));
	TestEqual(TEXT("A lone button"), ShinobiKeyBindings::PadLabel({ FKey(), EKeys::Gamepad_FaceButton_Top }).ToString(), FString(TEXT("Y")));
	TestEqual(TEXT("Nothing"), ShinobiKeyBindings::PadLabel(FPadBinding()).ToString(), FString(TEXT("None")));
	TestTrue(TEXT("Nothing on the HUD"), ShinobiKeyBindings::PadLabel(FPadBinding(), true).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPadRebindTest, "ShinobiCoop.KeyBindings.PadRebind", ShinobiTestFlags)
bool FShinobiPadRebindTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	// A combo onto an action that had a lone button.
	TestTrue(TEXT("RT + X for Dodge is free"), ShinobiKeyBindings::RebindPad(EBindable::Dodge, { EKeys::Gamepad_RightTrigger, EKeys::Gamepad_FaceButton_Left }) == ERebind::Done);
	const FEnhancedActionKeyMapping* DodgeX = PadMappingOf(EBindable::Dodge, EKeys::Gamepad_FaceButton_Left);
	TestTrue(TEXT("Dodge on X, chorded on RT"), DodgeX && ChordOf(DodgeX) == UShinobiInputConfig::Get().PadHoldRT);
	TestNull(TEXT("and no longer on B"), PadMappingOf(EBindable::Dodge, EKeys::Gamepad_FaceButton_Right));
	const FEnhancedActionKeyMapping* MeleeX = PadMappingOf(EBindable::Melee, EKeys::Gamepad_FaceButton_Left);
	TestTrue(TEXT("Melee keeps plain X (Enhanced Input blocks it while RT is held)"), MeleeX && ChordOf(MeleeX) == nullptr);

	// A lone button onto a jutsu slot, taken from an action: they swap.
	EBindable SwappedWith = EBindable::Num;
	TestTrue(TEXT("Y is Revive's: a swap"), ShinobiKeyBindings::RebindPad(EBindable::Slot1, { FKey(), EKeys::Gamepad_FaceButton_Top }, &SwappedWith) == ERebind::Swapped);
	TestTrue(TEXT("with Revive"), SwappedWith == EBindable::Revive);
	TestTrue(TEXT("Revive gets RB + A"), ShinobiKeyBindings::GetPad(EBindable::Revive) == FPadBinding{ EKeys::Gamepad_RightShoulder, EKeys::Gamepad_FaceButton_Bottom });
	const FEnhancedActionKeyMapping* SlotY = PadMappingOf(EBindable::Slot1, EKeys::Gamepad_FaceButton_Top);
	TestTrue(TEXT("Slot 1 on plain Y"), SlotY && ChordOf(SlotY) == nullptr);

	// Taking an unassigned action's place leaves the other with nothing.
	TestTrue(TEXT("LB for Map zoom out: Chakra charge's"), ShinobiKeyBindings::RebindPad(EBindable::MapZoomOut, { FKey(), EKeys::Gamepad_LeftShoulder }, &SwappedWith) == ERebind::Swapped);
	TestFalse(TEXT("Chakra charge now has no button"), ShinobiKeyBindings::GetPad(EBindable::ChakraCharge).IsBound());
	TestEqual(TEXT("and no mapping but its key"), UShinobiInputConfig::Get().Context->GetMappings().FilterByPredicate([](const FEnhancedActionKeyMapping& Mapping)
	{
		return Mapping.Action == UShinobiInputConfig::Get().ActionForBindable(static_cast<uint8>(EBindable::ChakraCharge));
	}).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPadBlockedTest, "ShinobiCoop.KeyBindings.PadBlocked", ShinobiTestFlags)
bool FShinobiPadBlockedTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	const FPadBinding Blocked[] = {
		{ FKey(), EKeys::Gamepad_RightShoulder }, { FKey(), EKeys::Gamepad_RightTrigger }, // hold-only
		{ EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightTrigger }, // hold + hold
		{ FKey(), EKeys::Gamepad_Special_Right }, // Menu always pauses
		{ EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_FaceButton_Bottom }, // only RB / RT are held first
		{ FKey(), EKeys::Gamepad_LeftX }, { FKey(), EKeys::Q },
	};
	for (const FPadBinding& Binding : Blocked)
	{
		TestTrue(*FString::Printf(TEXT("%s refused"), *ShinobiKeyBindings::PadLabel(Binding).ToString()),
			ShinobiKeyBindings::RebindPad(EBindable::Jump, Binding) == ERebind::Blocked);
	}
	TestTrue(TEXT("Jump keeps A"), ShinobiKeyBindings::GetPad(EBindable::Jump) == ShinobiKeyBindings::PadDefault(EBindable::Jump));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPadResetTest, "ShinobiCoop.KeyBindings.ResetPerDevice", ShinobiTestFlags)
bool FShinobiPadResetTest::RunTest(const FString& Parameters)
{
	FSavedBindings Saved;
	ShinobiKeyBindings::Rebind(EBindable::Jump, { EKeys::J });
	ShinobiKeyBindings::RebindPad(EBindable::Jump, { EKeys::Gamepad_RightTrigger, EKeys::Gamepad_FaceButton_Bottom });
	ShinobiKeyBindings::ResetPad();
	TestTrue(TEXT("controller reset: Jump back on A"), ShinobiKeyBindings::GetPad(EBindable::Jump) == ShinobiKeyBindings::PadDefault(EBindable::Jump));
	TestTrue(TEXT("but the keyboard keeps J"), ShinobiKeyBindings::Get(EBindable::Jump).Key == EKeys::J);
	ShinobiKeyBindings::RebindPad(EBindable::Jump, { EKeys::Gamepad_RightTrigger, EKeys::Gamepad_FaceButton_Bottom });
	ShinobiKeyBindings::ResetKeyboard();
	TestTrue(TEXT("keyboard reset: Jump back on Space"), ShinobiKeyBindings::Get(EBindable::Jump).Key == EKeys::SpaceBar);
	TestTrue(TEXT("but the controller keeps RT + A"), ShinobiKeyBindings::GetPad(EBindable::Jump) == FPadBinding{ EKeys::Gamepad_RightTrigger, EKeys::Gamepad_FaceButton_Bottom });
	return true;
}

#endif
