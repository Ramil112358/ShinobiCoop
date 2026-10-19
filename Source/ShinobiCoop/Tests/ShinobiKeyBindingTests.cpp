#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif
