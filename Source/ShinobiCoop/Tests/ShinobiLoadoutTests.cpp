#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/ConfigCacheIni.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Kamui.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/Abilities/GA_Teleport.h"
#include "Shinobi/Abilities/GA_WaterDragon.h"
#include "Shinobi/Abilities/GA_WaterWall.h"

namespace
{
	using ShinobiLoadout::FSlots;

	TArray<TSubclassOf<UShinobiJutsuAbility>> KnownForTests()
	{
		TArray<TSubclassOf<UShinobiJutsuAbility>> Known(ShinobiLoadout::Default());
		Known.Add(UGA_WaterDragon::StaticClass());
		Known.Add(UGA_WaterWall::StaticClass());
		return Known;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutSlotIdsTest, "ShinobiCoop.Loadout.SlotIds", ShinobiTestFlags)
bool FShinobiLoadoutSlotIdsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Slot 0 is Jutsu1"), ShinobiLoadout::SlotInputID(0) == EShinobiAbilityInputID::Jutsu1);
	TestTrue(TEXT("Slot 7 is Jutsu8"), ShinobiLoadout::SlotInputID(7) == EShinobiAbilityInputID::Jutsu8);
	TestTrue(TEXT("Slot 8 is out of range"), ShinobiLoadout::SlotInputID(8) == EShinobiAbilityInputID::None);
	TestEqual(TEXT("Jutsu5 is slot 4"), ShinobiLoadout::SlotIndexOf(EShinobiAbilityInputID::Jutsu5), 4);
	TestEqual(TEXT("Melee is no slot"), ShinobiLoadout::SlotIndexOf(EShinobiAbilityInputID::Melee), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutDefaultTest, "ShinobiCoop.Loadout.DefaultIsTheSpecOrder", ShinobiTestFlags)
bool FShinobiLoadoutDefaultTest::RunTest(const FString& Parameters)
{
	const FSlots Expected = {
		UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
		UGA_Teleport::StaticClass(), UGA_Kamui::StaticClass(), UGA_AlmightyPush::StaticClass(), UGA_Susanoo::StaticClass(),
	};
	TestTrue(TEXT("Default loadout matches the spec"), ShinobiLoadout::Default() == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutValidationTest, "ShinobiCoop.Loadout.Validation", ShinobiTestFlags)
bool FShinobiLoadoutValidationTest::RunTest(const FString& Parameters)
{
	const TArray<TSubclassOf<UShinobiJutsuAbility>> Known = KnownForTests();
	TestTrue(TEXT("Default is valid"), ShinobiLoadout::IsValid(ShinobiLoadout::Default(), Known));
	FSlots Short = ShinobiLoadout::Default();
	Short.Pop();
	TestFalse(TEXT("7 slots is invalid"), ShinobiLoadout::IsValid(Short, Known));
	FSlots Dupe = ShinobiLoadout::Default();
	Dupe[1] = UGA_Fireball::StaticClass();
	TestFalse(TEXT("A duplicate is invalid"), ShinobiLoadout::IsValid(Dupe, Known));
	FSlots Unknown = ShinobiLoadout::Default();
	Unknown[1] = UGA_WaterDragon::StaticClass();
	TestFalse(TEXT("A jutsu the player doesn't have is invalid"), ShinobiLoadout::IsValid(Unknown, ShinobiLoadout::Default()));
	FSlots WithEmpty = ShinobiLoadout::Default();
	WithEmpty[3] = nullptr;
	TestTrue(TEXT("An empty slot is valid"), ShinobiLoadout::IsValid(WithEmpty, Known));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutEditOpsTest, "ShinobiCoop.Loadout.EditOps", ShinobiTestFlags)
bool FShinobiLoadoutEditOpsTest::RunTest(const FString& Parameters)
{
	const FSlots Base = ShinobiLoadout::Default();
	const FSlots Placed = ShinobiLoadout::Place(Base, UGA_WaterDragon::StaticClass(), 2);
	TestTrue(TEXT("Water Dragon lands in slot 2"), Placed[2] == UGA_WaterDragon::StaticClass());
	TestFalse(TEXT("Rasengan leaves the loadout"), Placed.Contains(UGA_Rasengan::StaticClass()));
	const FSlots Moved = ShinobiLoadout::Place(Base, UGA_Kamui::StaticClass(), 0);
	TestTrue(TEXT("Kamui moves to slot 0"), Moved[0] == UGA_Kamui::StaticClass());
	TestTrue(TEXT("Fireball takes Kamui's old slot"), Moved[5] == UGA_Fireball::StaticClass());
	const FSlots Swapped = ShinobiLoadout::Swap(Base, 0, 7);
	TestTrue(TEXT("Swap: slot 0 is Susanoo"), Swapped[0] == UGA_Susanoo::StaticClass());
	TestTrue(TEXT("Swap: slot 7 is Fireball"), Swapped[7] == UGA_Fireball::StaticClass());
	TestTrue(TEXT("Clear empties the slot"), ShinobiLoadout::Clear(Base, 3)[3] == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutCanChangeTest, "ShinobiCoop.Loadout.CanChange", ShinobiTestFlags)
bool FShinobiLoadoutCanChangeTest::RunTest(const FString& Parameters)
{
	const FSlots Base = ShinobiLoadout::Default();
	TestTrue(TEXT("No change is always allowed"), ShinobiLoadout::CanChange(Base, Base, [](int32) { return true; }));
	const FSlots NewSlot2 = ShinobiLoadout::Place(Base, UGA_WaterDragon::StaticClass(), 2);
	TestFalse(TEXT("A locked slot can't change"), ShinobiLoadout::CanChange(Base, NewSlot2, [](int32 Slot) { return Slot == 2; }));
	TestTrue(TEXT("Another slot's lock doesn't matter"), ShinobiLoadout::CanChange(Base, NewSlot2, [](int32 Slot) { return Slot == 5; }));
	TestFalse(TEXT("A locked jutsu can't be swapped away"), ShinobiLoadout::CanChange(Base, ShinobiLoadout::Swap(Base, 2, 5), [](int32 Slot) { return Slot == 5; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutSaveLoadTest, "ShinobiCoop.Loadout.SaveLoadRoundTrip", ShinobiTestFlags)
bool FShinobiLoadoutSaveLoadTest::RunTest(const FString& Parameters)
{
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiLoadout");
	TArray<FString> OldSlots;
	const bool bHadOld = GConfig->GetArray(Section, TEXT("Slots"), OldSlots, GGameUserSettingsIni) > 0;
	const TArray<TSubclassOf<UShinobiJutsuAbility>> Known = KnownForTests();

	const FSlots Saved = ShinobiLoadout::Clear(ShinobiLoadout::Place(ShinobiLoadout::Default(), UGA_WaterDragon::StaticClass(), 2), 6);
	ShinobiLoadout::Save(Saved);
	TestTrue(TEXT("Load returns what was saved"), ShinobiLoadout::LoadSaved(Known) == Saved);

	TArray<FString> Garbage;
	Garbage.Init(TEXT("/Script/ShinobiCoop.GA_NoSuchJutsu"), ShinobiLoadout::NumSlots);
	GConfig->SetArray(Section, TEXT("Slots"), Garbage, GGameUserSettingsIni);
	TestTrue(TEXT("A corrupt save falls back to the default"), ShinobiLoadout::LoadSaved(Known) == ShinobiLoadout::Default());
	GConfig->SetArray(Section, TEXT("Slots"), { TEXT("") }, GGameUserSettingsIni);
	TestTrue(TEXT("A wrong-length save falls back to the default"), ShinobiLoadout::LoadSaved(Known) == ShinobiLoadout::Default());

	if (bHadOld)
	{
		GConfig->SetArray(Section, TEXT("Slots"), OldSlots, GGameUserSettingsIni);
	}
	else
	{
		GConfig->RemoveKey(Section, TEXT("Slots"), GGameUserSettingsIni);
	}
	return true;
}

#endif
