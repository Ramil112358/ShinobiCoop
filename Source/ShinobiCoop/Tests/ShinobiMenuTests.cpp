#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/ConfigCacheIni.h"
#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMainMenuWidget.h"
#include "Shinobi/UI/ShinobiRosterPage.h"
#include "Shinobi/UI/ShinobiSkillsWidget.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/UI/ShinobiSettings.h"

// The main menu (docs/superpowers/specs/2026-10-20-main-menu-polish-design.md): its data, the stage and the tabs.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuHostOrderTest, "ShinobiCoop.UI.MenuHostOrder", ShinobiTestFlags)
bool FShinobiMenuHostOrderTest::RunTest(const FString& Parameters)
{
	const TArray<int32> Order = UShinobiSessionSubsystem::GetHostOrder();
	TestEqual(TEXT("co-op, then arenas, then test maps"), Order, TArray<int32>{ 0, 1, 2, 5, 6, 7, 8, 3, 4 });
	TestEqual(TEXT("every map once"), Order.Num(), UShinobiSessionSubsystem::GetMaps().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuModesTest, "ShinobiCoop.UI.MenuModes", ShinobiTestFlags)
bool FShinobiMenuModesTest::RunTest(const FString& Parameters)
{
	const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
	TestEqual(TEXT("co-op map"), UShinobiSessionSubsystem::GetModes(Maps[1]), TArray<EShinobiPvpFormat>{ EShinobiPvpFormat::None });
	TestEqual(TEXT("arena"), UShinobiSessionSubsystem::GetModes(Maps[5]), TArray<EShinobiPvpFormat>{ EShinobiPvpFormat::OneVsOne, EShinobiPvpFormat::TwoVsTwo });
	TestEqual(TEXT("co-op players"), UShinobiSessionSubsystem::GetMaxPlayers(Maps[1], EShinobiPvpFormat::None), 4);
	TestEqual(TEXT("1v1 players"), UShinobiSessionSubsystem::GetMaxPlayers(Maps[5], EShinobiPvpFormat::OneVsOne), 2);
	TestEqual(TEXT("2v2 players"), UShinobiSessionSubsystem::GetMaxPlayers(Maps[5], EShinobiPvpFormat::TwoVsTwo), 4);
	TestEqual(TEXT("find by name"), UShinobiSessionSubsystem::FindMapByName(TEXT("London")), 1);
	TestEqual(TEXT("unknown name"), UShinobiSessionSubsystem::FindMapByName(TEXT("Atlantis")), int32(INDEX_NONE));
	for (const FShinobiMap& Map : Maps)
	{
		TestTrue(FString::Printf(TEXT("%s has a blurb"), Map.DisplayName), FCString::Strlen(Map.Blurb) > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuArtPathsTest, "ShinobiCoop.UI.MenuArtPaths", ShinobiTestFlags)
bool FShinobiMenuArtPathsTest::RunTest(const FString& Parameters)
{
	const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
	TestEqual(TEXT("London id"), ShinobiMenuArt::MapId(Maps[1]), FString(TEXT("London")));
	TestEqual(TEXT("arena id"), ShinobiMenuArt::MapId(Maps[6]), FString(TEXT("Arena_Camden")));
	TestEqual(TEXT("test level id"), ShinobiMenuArt::MapId(Maps[3]), FString(TEXT("ThirdPerson")));
	TestEqual(TEXT("map image"), ShinobiMenuArt::MapImagePath(Maps[1]), FString(TEXT("/Game/Shinobi/UI/Menu/T_Map_London.T_Map_London")));
	TestEqual(TEXT("portrait"), ShinobiMenuArt::PortraitPath(0), FString(TEXT("/Game/Shinobi/UI/Menu/T_Portrait_Hero.T_Portrait_Hero")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuArtMissingTest, "ShinobiCoop.UI.MenuArtMissingIsNull", ShinobiTestFlags)
bool FShinobiMenuArtMissingTest::RunTest(const FString& Parameters)
{
	TestNull(TEXT("an uncaptured portrait"), ShinobiMenuArt::LoadArt(TEXT("/Game/Shinobi/UI/Menu/T_Portrait_Nobody.T_Portrait_Nobody")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuSettingsSettersTest, "ShinobiCoop.UI.MenuSettingsSetters", ShinobiTestFlags)
bool FShinobiMenuSettingsSettersTest::RunTest(const FString& Parameters)
{
	const FString OldCharacter = ShinobiSettings::GetSavedCharacter();
	const FString OldOutfit = ShinobiSettings::GetSavedOutfit();

	ShinobiSettings::SetSavedCharacter(AShinobiPlayerCharacter::Roster[1].Name);
	TestEqual(TEXT("character saved"), ShinobiSettings::GetSavedCharacter(), FString(AShinobiPlayerCharacter::Roster[1].Name));
	ShinobiSettings::SetSavedCharacter(TEXT("Nobody"));
	TestEqual(TEXT("unknown character ignored"), ShinobiSettings::GetSavedCharacter(), FString(AShinobiPlayerCharacter::Roster[1].Name));

	ShinobiSettings::SetSavedOutfit(AShinobiPlayerCharacter::Outfits[1].Name);
	TestEqual(TEXT("outfit saved"), ShinobiSettings::GetSavedOutfit(), FString(AShinobiPlayerCharacter::Outfits[1].Name));
	ShinobiSettings::SetSavedOutfit(TEXT("Nothing"));
	TestEqual(TEXT("unknown outfit ignored"), ShinobiSettings::GetSavedOutfit(), FString(AShinobiPlayerCharacter::Outfits[1].Name));

	ShinobiSettings::SetSavedCharacter(OldCharacter);
	ShinobiSettings::SetSavedOutfit(OldOutfit);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuSettingsRowsTest, "ShinobiCoop.UI.MenuSettingsRows", ShinobiTestFlags)
bool FShinobiMenuSettingsRowsTest::RunTest(const FString& Parameters)
{
	using ShinobiSettings::ERow;
	const TArray<ERow> Menu = ShinobiSettings::RowsFor(true);
	TestFalse(TEXT("main menu: no Character row"), Menu.Contains(ERow::Character));
	TestFalse(TEXT("main menu: no Outfit row"), Menu.Contains(ERow::Outfit));
	TestEqual(TEXT("main menu rows"), Menu.Num(), int32(ERow::Num) - 2);
	TestEqual(TEXT("pause menu rows"), ShinobiSettings::RowsFor(false).Num(), int32(ERow::Num));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuJutsuGroupsTest, "ShinobiCoop.UI.MenuJutsuGroups", ShinobiTestFlags)
bool FShinobiMenuJutsuGroupsTest::RunTest(const FString& Parameters)
{
	using ShinobiLoadout::EGroup;
	TestTrue(TEXT("Fireball"), ShinobiLoadout::GroupOf(UGA_Fireball::StaticClass()) == EGroup::Combat);
	TestTrue(TEXT("Earth Wall"), ShinobiLoadout::GroupOf(UGA_EarthWall::StaticClass()) == EGroup::Elements);
	TestTrue(TEXT("Susanoo"), ShinobiLoadout::GroupOf(UGA_Susanoo::StaticClass()) == EGroup::SummonsAndDragons);
	TestEqual(TEXT("group name"), ShinobiLoadout::GroupName(EGroup::SummonsAndDragons).ToString(), FString(TEXT("SUMMONS & DRAGONS")));
	TestTrue(TEXT("the pawn knows jutsu"), GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu().Num() > 0);
	return true;
}

namespace
{
	bool MenuTestBodyImported(int32 Body)
	{
		return LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Roster[Body].Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageShowsPickTest, "ShinobiCoop.UI.MenuStageShowsPick", ShinobiTestFlags)
bool FShinobiMenuStageShowsPickTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	if (MenuTestBodyImported(1))
	{
		Stage->Show(1, 0);
		TestEqual(TEXT("shows the pick"), Stage->GetShownBody(), 1);
		TestEqual(TEXT("wears its mesh"), Stage->GetBody()->GetSkeletalMeshAsset()->GetPathName(), FString(AShinobiPlayerCharacter::Roster[1].Mesh));
	}
	Stage->Show(99, 0);
	TestEqual(TEXT("an unknown pick falls back to the hero"), Stage->GetShownBody(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageMissingOutfitTest, "ShinobiCoop.UI.MenuStageMissingOutfitShowsBody", ShinobiTestFlags)
bool FShinobiMenuStageMissingOutfitTest::RunTest(const FString& Parameters)
{
	for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
	{
		for (int32 Outfit = 1; Outfit < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Outfit)
		{
			if (MenuTestBodyImported(Body) && !AShinobiPlayerCharacter::IsOutfitBuilt(Outfit, Body))
			{
				FShinobiTestWorld W;
				AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
				Stage->Show(Body, Outfit);
				TestFalse(TEXT("no outfit shown"), Stage->GetOutfit()->IsVisible());
				TestTrue(TEXT("body shown"), Stage->GetBody()->IsVisible());
				return true;
			}
		}
	}
	AddInfo(TEXT("every outfit is built for every imported body: nothing to check"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageWearsOutfitTest, "ShinobiCoop.UI.MenuStageWearsOutfit", ShinobiTestFlags)
bool FShinobiMenuStageWearsOutfitTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("no outfit is always built"), AShinobiPlayerCharacter::IsOutfitBuilt(0, 0));
	for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
	{
		for (int32 Outfit = 1; Outfit < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Outfit)
		{
			if (MenuTestBodyImported(Body) && AShinobiPlayerCharacter::IsOutfitBuilt(Outfit, Body))
			{
				FShinobiTestWorld W;
				AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
				Stage->Show(Body, Outfit);
				const USkeletalMesh* Worn = Stage->GetOutfit()->GetSkeletalMeshAsset();
				TestEqual(TEXT("the fitted copy"), Worn ? Worn->GetPathName() : FString(), AShinobiPlayerCharacter::OutfitMeshPath(Outfit, Body));
				TestTrue(TEXT("follows the body"), Stage->GetOutfit()->LeaderPoseComponent.Get() == Stage->GetBody());
				TestTrue(TEXT("shown"), Stage->GetOutfit()->IsVisible());
				return true;
			}
		}
	}
	AddWarning(TEXT("no outfit copy is built: run Scripts/character/build_outfits.ps1"));
	return true;
}

namespace
{
	/** Up through parents and owning user widgets. */
	bool MenuTestIsInside(const UWidget* Widget, const UWidget* Container)
	{
		while (Widget)
		{
			if (Widget == Container)
			{
				return true;
			}
			const UWidget* Parent = Widget->GetParent();
			if (!Parent)
			{
				const UUserWidget* Owner = Widget->GetTypedOuter<UUserWidget>();
				Parent = Owner != Widget ? Owner : nullptr;
			}
			Widget = Parent;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuTabFocusTest, "ShinobiCoop.UI.MainMenuTabFocus", ShinobiTestFlags)
bool FShinobiMainMenuTabFocusTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	for (const EShinobiMenuTab Tab : { EShinobiMenuTab::Play, EShinobiMenuTab::Shinobi, EShinobiMenuTab::Jutsu, EShinobiMenuTab::Settings })
	{
		Menu->SelectTab(Tab);
		TestTrue(TEXT("tab selected"), Menu->GetTab() == Tab);
		const UWidget* Target = Menu->GetFocusTarget(Tab);
		TestNotNull(TEXT("something to focus"), Target);
		TestTrue(TEXT("focus target is on that tab's page"), MenuTestIsInside(Target, Menu->GetTabPage(Tab)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuHostRowsTest, "ShinobiCoop.UI.MainMenuHostRowsCoverMaps", ShinobiTestFlags)
bool FShinobiMainMenuHostRowsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	TestEqual(TEXT("a row per map"), Menu->GetHostRowCount(), UShinobiSessionSubsystem::GetMaps().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuRosterPickTest, "ShinobiCoop.UI.MenuRosterPickSaves", ShinobiTestFlags)
bool FShinobiMenuRosterPickTest::RunTest(const FString& Parameters)
{
	const FString OldCharacter = ShinobiSettings::GetSavedCharacter();
	const FString OldOutfit = ShinobiSettings::GetSavedOutfit();
	FShinobiTestWorld W;
	UShinobiRosterPage* Page = CreateWidget<UShinobiRosterPage>(W.World, UShinobiRosterPage::StaticClass());
	Page->TakeWidget();

	Page->Pick(1);
	TestEqual(TEXT("pick saved"), ShinobiSettings::GetSavedCharacter(), FString(AShinobiPlayerCharacter::Roster[1].Name));
	ShinobiSettings::SetSavedOutfit(AShinobiPlayerCharacter::Outfits[0].Name);
	Page->StepOutfit(+1);
	TestEqual(TEXT("next outfit"), ShinobiSettings::GetSavedOutfit(), FString(AShinobiPlayerCharacter::Outfits[1].Name));
	Page->StepOutfit(-1);
	Page->StepOutfit(-1);
	TestEqual(TEXT("wraps to the last outfit"), ShinobiSettings::GetSavedOutfit(),
		FString(AShinobiPlayerCharacter::Outfits[UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits) - 1].Name));
	TestNotNull(TEXT("a card to focus"), Page->GetFocusTarget());

	ShinobiSettings::SetSavedCharacter(OldCharacter);
	ShinobiSettings::SetSavedOutfit(OldOutfit);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuLoadoutTest, "ShinobiCoop.UI.MenuLoadoutSavesValid", ShinobiTestFlags)
bool FShinobiMenuLoadoutTest::RunTest(const FString& Parameters)
{
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiLoadout");
	TArray<FString> OldSlots;
	const bool bHadSlots = GConfig->GetArray(Section, TEXT("Slots"), OldSlots, GGameUserSettingsIni) > 0;
	GConfig->SetArray(Section, TEXT("Slots"), TArray<FString>{ TEXT("a"), TEXT("b"), TEXT("c") }, GGameUserSettingsIni); // an old, broken save

	const TArray<TSubclassOf<UShinobiJutsuAbility>> Known = GetDefault<AShinobiPlayerCharacter>()->GetKnownJutsu();
	FShinobiTestWorld W;
	UShinobiSkillsWidget* Jutsu = CreateWidget<UShinobiSkillsWidget>(W.World, UShinobiSkillsWidget::StaticClass());
	Jutsu->bMenuMode = true;
	Jutsu->TakeWidget();
	TestTrue(TEXT("a broken save shows the default bar"), Jutsu->GetShownLoadout() == ShinobiLoadout::Default());

	const int32 Spare = Known.IndexOfByPredicate([](const TSubclassOf<UShinobiJutsuAbility>& J) { return !ShinobiLoadout::Default().Contains(J); });
	if (TestTrue(TEXT("a jutsu not on the default bar"), Spare != INDEX_NONE))
	{
		Jutsu->OnCellClicked(*Jutsu->GetCell(false, Spare));
		Jutsu->OnCellClicked(*Jutsu->GetCell(true, 7));
		const ShinobiLoadout::FSlots Saved = ShinobiLoadout::LoadSaved(Known);
		TestTrue(TEXT("saved in slot 8"), Saved[7] == Known[Spare]);
		TestTrue(TEXT("a valid bar"), ShinobiLoadout::IsValid(Saved, Known));
		TestTrue(TEXT("shown"), Jutsu->GetShownLoadout() == Saved);
	}

	if (bHadSlots)
	{
		GConfig->SetArray(Section, TEXT("Slots"), OldSlots, GGameUserSettingsIni);
	}
	else
	{
		GConfig->RemoveKey(Section, TEXT("Slots"), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuArtAllCapturedTest, "ShinobiCoop.UI.MenuArtAllCaptured", ShinobiTestFlags)
bool FShinobiMenuArtAllCapturedTest::RunTest(const FString& Parameters)
{
	for (const FShinobiMap& Map : UShinobiSessionSubsystem::GetMaps())
	{
		TestNotNull(FString::Printf(TEXT("%s has a picture (Scripts/menu_shots.ps1 -Maps)"), Map.DisplayName), ShinobiMenuArt::LoadArt(ShinobiMenuArt::MapImagePath(Map)));
	}
	// A character added later has no portrait until it's shot: its card shows its initial, so only warn.
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Index)
	{
		if (!ShinobiMenuArt::LoadArt(ShinobiMenuArt::PortraitPath(Index)))
		{
			AddWarning(FString::Printf(TEXT("%s has no portrait: Scripts/menu_shots.ps1 -Portraits"), AShinobiPlayerCharacter::Roster[Index].Name));
		}
	}
	return true;
}

#endif
