#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dragon.h"
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

	int32 InputOf(AShinobiPlayerCharacter* P, TSubclassOf<UGameplayAbility> Ability)
	{
		const FGameplayAbilitySpec* Spec = P->GetAbilitySystemComponent()->FindAbilitySpecFromClass(Ability);
		return Spec ? Spec->InputID : -999;
	}

	int32 SlotInput(int32 SlotIndex) { return static_cast<int32>(ShinobiLoadout::SlotInputID(SlotIndex)); }

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutDefaultSlotsFireTest, "ShinobiCoop.Loadout.DefaultSlotsFire", ShinobiTestFlags)
bool FShinobiLoadoutDefaultSlotsFireTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestTrue(TEXT("A new player has the default loadout"), P->GetLoadout() == ShinobiLoadout::Default());
	TestEqual(TEXT("Fireball fires from slot 1"), InputOf(P, UGA_Fireball::StaticClass()), SlotInput(0));
	TestEqual(TEXT("Susanoo fires from slot 8"), InputOf(P, UGA_Susanoo::StaticClass()), SlotInput(7));
	TestEqual(TEXT("Water Dragon is in no slot"), InputOf(P, UGA_WaterDragon::StaticClass()), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutUnslottedTest, "ShinobiCoop.Loadout.UnslottedJutsuCannotActivate", ShinobiTestFlags)
bool FShinobiLoadoutUnslottedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	UAbilitySystemComponent* ASC = P->GetAbilitySystemComponent();
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UGA_WaterDragon::StaticClass());
	if (!TestNotNull(TEXT("Water Dragon is granted"), Spec))
	{
		return false;
	}
	TestFalse(TEXT("An unslotted jutsu can't be activated, even by handle"), ASC->TryActivateAbility(Spec->Handle));
	ShinobiTest::Tap(P, ShinobiTest::Equip(P, UGA_WaterDragon::StaticClass(), 2));
	TestTrue(TEXT("Slotted into 3, key 3 casts it"), ShinobiTest::IsActive(P, UGA_WaterDragon::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutFollowUpsTest, "ShinobiCoop.Loadout.FollowUpsTrackTheirParentSlot", ShinobiTestFlags)
bool FShinobiLoadoutFollowUpsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestEqual(TEXT("Kamui exit starts on Kamui's slot 6"), InputOf(P, UGA_KamuiExit::StaticClass()), SlotInput(5));
	TestEqual(TEXT("The Teleport jump on Teleport's slot 5"), InputOf(P, UGA_TeleportJump::StaticClass()), SlotInput(4));
	P->ApplyLoadout(ShinobiLoadout::Swap(P->GetLoadout(), 5, 1));
	TestEqual(TEXT("Kamui moved to slot 2: so does its exit"), InputOf(P, UGA_KamuiExit::StaticClass()), SlotInput(1));
	P->ApplyLoadout(ShinobiLoadout::Clear(P->GetLoadout(), 1));
	TestEqual(TEXT("Kamui unslotted: no key for its exit"), InputOf(P, UGA_KamuiExit::StaticClass()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("The dismount key keeps X"), InputOf(P, UGA_DragonDismountKey::StaticClass()), static_cast<int32>(EShinobiAbilityInputID::Dismount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutRestampTest, "ShinobiCoop.Loadout.OnRepRestampsInputIDs", ShinobiTestFlags)
bool FShinobiLoadoutRestampTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	for (FGameplayAbilitySpec& Spec : P->GetAbilitySystemComponent()->GetActivatableAbilities())
	{
		if (Cast<UShinobiJutsuAbility>(Spec.Ability))
		{
			Spec.InputID = 42; // stale, as a client's specs can be before the loadout arrives
		}
	}
	P->StampLoadoutInputIDs(); // what OnRep_Loadout runs
	TestEqual(TEXT("Fireball back on slot 1"), InputOf(P, UGA_Fireball::StaticClass()), SlotInput(0));
	TestEqual(TEXT("Water Wall back to no slot"), InputOf(P, UGA_WaterWall::StaticClass()), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutCooldownLockTest, "ShinobiCoop.Loadout.CooldownLocksSlot", ShinobiTestFlags)
bool FShinobiLoadoutCooldownLockTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	W.Tick(1.f); // the seals end, the ball flies, the cooldown runs
	TestTrue(TEXT("Fireball cooling down"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Fireball));
	TestTrue(TEXT("Slot 1 locked"), P->IsSlotLocked(0));
	TestFalse(TEXT("Can't replace a jutsu on cooldown"), P->TryChangeLoadout(ShinobiLoadout::Place(P->GetLoadout(), UGA_WaterWall::StaticClass(), 0)));
	TestTrue(TEXT("Still Fireball"), P->GetLoadout()[0] == UGA_Fireball::StaticClass());
	TestTrue(TEXT("Another slot can change"), P->TryChangeLoadout(ShinobiLoadout::Place(P->GetLoadout(), UGA_WaterWall::StaticClass(), 1)));
	TestTrue(TEXT("Water Wall in slot 2"), P->GetLoadout()[1] == UGA_WaterWall::StaticClass());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutMountedLockTest, "ShinobiCoop.Loadout.MountedLocksAll", ShinobiTestFlags)
bool FShinobiLoadoutMountedLockTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestTrue(TEXT("Unlocked on foot"), P->GetLoadoutLockReason().IsEmpty());
	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	TestFalse(TEXT("Locked while mounted"), P->GetLoadoutLockReason().IsEmpty());
	TestFalse(TEXT("No swap while mounted"), P->TryChangeLoadout(ShinobiLoadout::Swap(P->GetLoadout(), 2, 3)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadoutDragonSlotTest, "ShinobiCoop.Loadout.DragonSlotDismounts", ShinobiTestFlags)
bool FShinobiLoadoutDragonSlotTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::Equip(P, UGA_SummonDragon::StaticClass(), 5);
	const EShinobiAbilityInputID Crimson = ShinobiTest::Equip(P, UGA_SummonCrimsonDragon::StaticClass(), 6);
	ShinobiTest::Tap(P, Crimson);
	W.Tick(1.3f);
	if (!TestTrue(TEXT("Riding the Crimson Dragon"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted)))
	{
		return false;
	}
	ShinobiTest::Tap(P, ShinobiLoadout::SlotInputID(5));
	TestTrue(TEXT("Tairn's slot does nothing while on the Crimson Dragon"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	ShinobiTest::Tap(P, Crimson);
	TestFalse(TEXT("The Crimson Dragon's slot climbs down"), ShinobiTest::HasTag(P, ShinobiTags::State_Mounted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCombatDealtTest, "ShinobiCoop.Loadout.DamageStartsCombat", ShinobiTestFlags)
bool FShinobiCombatDealtTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	TestFalse(TEXT("Not in combat at the start"), P->IsInCombat());
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Enemy, 10.f);
	TestTrue(TEXT("Hitting an enemy puts the player in combat"), P->IsInCombat());
	TestFalse(TEXT("which locks the loadout"), P->GetLoadoutLockReason().IsEmpty());
	W.Tick(4.5f);
	TestTrue(TEXT("Still in combat 4.5 s later"), P->IsInCombat());
	W.Tick(0.7f);
	TestFalse(TEXT("Out of combat 5 s after the last hit"), P->IsInCombat());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCombatTakenTest, "ShinobiCoop.Loadout.TakingDamageStartsCombat", ShinobiTestFlags)
bool FShinobiCombatTakenTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 10.f);
	TestTrue(TEXT("Being hit puts the player in combat"), P->IsInCombat());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCombatSummonTest, "ShinobiCoop.Loadout.SummonHitsCount", ShinobiTestFlags)
bool FShinobiCombatSummonTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(800.f, 0.f, 0.f));
	const TArray<AShinobiSummon*> Summons = AShinobiSummon::Summon(P, EShinobiSummonKind::Pusho, FVector(200.f, 0.f, 0.f));
	if (!TestTrue(TEXT("Pusho came"), Summons.Num() > 0 && Summons[0] != nullptr))
	{
		return false;
	}
	ShinobiCombat::ApplyDamage(Summons[0]->GetAbilitySystemComponent(), Enemy, 10.f);
	TestTrue(TEXT("The summon's hit puts its summoner in combat"), P->IsInCombat());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCombatTeammateTest, "ShinobiCoop.Loadout.TeammateDamageDoesNot", ShinobiTestFlags)
bool FShinobiCombatTeammateTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* A = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* B = W.Spawn<AShinobiPlayerCharacter>(FVector(300.f, 0.f, 0.f));
	ShinobiCombat::ApplyDamage(A->GetAbilitySystemComponent(), B, 10.f);
	ShinobiCombat::ApplyDamage(A->GetAbilitySystemComponent(), A, 10.f);
	TestFalse(TEXT("A teammate's or your own damage is not combat (A)"), A->IsInCombat());
	TestFalse(TEXT("A teammate's or your own damage is not combat (B)"), B->IsInCombat());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCombatLocksTest, "ShinobiCoop.Loadout.CombatLocksAll", ShinobiTestFlags)
bool FShinobiCombatLocksTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 10.f);
	TestFalse(TEXT("No swap in combat"), P->TryChangeLoadout(ShinobiLoadout::Swap(P->GetLoadout(), 2, 3)));
	TestTrue(TEXT("The banner has seconds to show"), P->GetCombatSecondsLeft() > 4.f && P->GetCombatSecondsLeft() <= 5.f);
	return true;
}

#endif
