#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Misc/ConfigCacheIni.h"
#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiStudioSet.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Shinobi/UI/ShinobiMainMenuWidget.h"
#include "Shinobi/UI/ShinobiDropdown.h"
#include "Shinobi/UI/ShinobiPauseMenuWidget.h"
#include "Shinobi/UI/ShinobiShortcutsWidget.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
#include "GameFramework/GameUserSettings.h"
#include "Shinobi/UI/ShinobiRosterPage.h"
#include "Shinobi/UI/ShinobiSkillsWidget.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "Shinobi/ShinobiGameState.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

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
	TestFalse(TEXT("main menu: no Face row"), Menu.Contains(ERow::Face));
	TestFalse(TEXT("main menu: no Outfit row"), Menu.Contains(ERow::Outfit));
	TestEqual(TEXT("main menu rows"), Menu.Num(), int32(ERow::Num) - 3);
	TestEqual(TEXT("pause menu rows"), ShinobiSettings::RowsFor(false).Num(), int32(ERow::Num));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSnowWeatherTest, "ShinobiCoop.UI.SnowWeatherFollowsTheHostsPick", ShinobiTestFlags)
bool FShinobiSnowWeatherTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_snow_weather.py if the weather systems are missing)
	FShinobiTestWorld W;
	AShinobiGameState* Session = W.Spawn<AShinobiGameState>();
	for (const EShinobiSnow Snow : { EShinobiSnow::Light, EShinobiSnow::Blizzard })
	{
		Session->SetSnow(EShinobiSnow::Off);
		Session->SetSnow(Snow);
		TestTrue(FString::Printf(TEXT("snow %d: the host's pick is the session's"), int32(Snow)), Session->GetSnow() == Snow);
		if (FApp::CanEverRender()) // Niagara spawns nothing in a -nullrhi test run
		{
			TestNotNull(FString::Printf(TEXT("snow %d: it snows round the camera"), int32(Snow)), Session->GetSnowEffect());
		}
	}
	Session->SetSnow(EShinobiSnow::Off);
	TestNull(TEXT("Off: no snow"), Session->GetSnowEffect());
	for (const TCHAR* System : { TEXT("/Game/Shinobi/VFX/Weather/NS_SnowLight.NS_SnowLight"), TEXT("/Game/Shinobi/VFX/Weather/NS_SnowBlizzard.NS_SnowBlizzard"),
		TEXT("/Game/Shinobi/VFX/Freeze/NS_FreezeIce.NS_FreezeIce"), TEXT("/Game/Shinobi/VFX/Freeze/NS_FreezeAura.NS_FreezeAura") })
	{
		TestNotNull(System, LoadObject<UNiagaraSystem>(nullptr, System));
	}
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
	for (const EShinobiMenuTab Tab : { EShinobiMenuTab::Play, EShinobiMenuTab::Shinobi, EShinobiMenuTab::Jutsu, EShinobiMenuTab::Settings, EShinobiMenuTab::Controls })
	{
		Menu->SelectTab(Tab);
		TestTrue(TEXT("tab selected"), Menu->GetTab() == Tab);
		const UWidget* Target = Menu->GetFocusTarget(Tab);
		TestNotNull(TEXT("something to focus"), Target);
		TestTrue(TEXT("focus target is on that tab's page"), MenuTestIsInside(Target, Menu->GetTabPage(Tab)));
	}
	// The creator opens inside SHINOBI: focus goes to it, and back to the strip after.
	Menu->OpenCreator(0);
	TestTrue(TEXT("the creator is on the SHINOBI tab"), Menu->GetTab() == EShinobiMenuTab::Shinobi && Menu->IsCreating());
	TestTrue(TEXT("focus target is in the creator"), MenuTestIsInside(Menu->GetFocusTarget(EShinobiMenuTab::Shinobi), Menu->GetCreatorPage()));
	Menu->CloseCreator();
	TestTrue(TEXT("focus target is on the strip again"), !Menu->IsCreating() && MenuTestIsInside(Menu->GetFocusTarget(EShinobiMenuTab::Shinobi), Menu->GetRosterPage()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPauseMenuTabsTest, "ShinobiCoop.UI.PauseMenuTabs", ShinobiTestFlags)
bool FShinobiPauseMenuTabsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UShinobiPauseMenuWidget* Menu = CreateWidget<UShinobiPauseMenuWidget>(W.World, UShinobiPauseMenuWidget::StaticClass());
	Menu->InitialTab = EShinobiPauseTab::Jutsu;
	Menu->TakeWidget();
	TestTrue(TEXT("opens on the tab asked for (K: JUTSU)"), Menu->GetTab() == EShinobiPauseTab::Jutsu);
	for (const EShinobiPauseTab Tab : { EShinobiPauseTab::Game, EShinobiPauseTab::Jutsu, EShinobiPauseTab::Settings, EShinobiPauseTab::Controls })
	{
		Menu->SelectTab(Tab);
		TestTrue(TEXT("tab selected"), Menu->GetTab() == Tab);
		UWidget* Page = Menu->GetTabPage(Tab);
		TestNotNull(TEXT("a page per tab"), Page);
		const UWidget* Target = Menu->GetFocusTarget(Tab);
		TestNotNull(TEXT("something to focus"), Target);
		TestTrue(TEXT("focus target is on that tab's page"), MenuTestIsInside(Target, Page));
	}
	UShinobiSettingsWidget* Settings = Cast<UShinobiSettingsWidget>(Menu->GetTabPage(EShinobiPauseTab::Settings));
	TestNotNull(TEXT("SETTINGS is the settings panel"), Settings);
	TestNotNull(TEXT("CONTROLS is the key bindings"), Cast<UShinobiShortcutsWidget>(Menu->GetTabPage(EShinobiPauseTab::Controls)));
	TestNotNull(TEXT("JUTSU is the Skills screen"), Cast<UShinobiSkillsWidget>(Menu->GetTabPage(EShinobiPauseTab::Jutsu)));
	TestFalse(TEXT("Leave isn't armed until pressed"), Menu->IsLeaveArmed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiControlsPadChipsTest, "ShinobiCoop.UI.ControlsPadChips", ShinobiTestFlags)
bool FShinobiControlsPadChipsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UShinobiShortcutsWidget* Controls = CreateWidget<UShinobiShortcutsWidget>(W.World, UShinobiShortcutsWidget::StaticClass());
	Controls->TakeWidget();
	for (int32 Slot = 0; Slot < ShinobiKeyBindings::NumPadSlots; ++Slot)
	{
		const UPanelWidget* Chip = Cast<UPanelWidget>(Controls->GetPadChip(Slot));
		const UTextBlock* Text = Chip ? Cast<UTextBlock>(Chip->GetChildAt(0)) : nullptr;
		if (TestNotNull(*FString::Printf(TEXT("slot %d has a controller chip"), Slot + 1), Text))
		{
			TestEqual(TEXT("showing RB + its button"), Text->GetText().ToString(), ShinobiKeyBindings::PadLabel(ShinobiKeyBindings::GetPad(Slot)).ToString());
		}
	}
	TestNull(TEXT("actions have no controller chip"), Controls->GetPadChip(ShinobiKeyBindings::NumPadSlots));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuTabClickTest, "ShinobiCoop.UI.MenuTabsOpenOnClick", ShinobiTestFlags)
bool FShinobiMenuTabClickTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	// Any tab, in any order: a click on its button opens it (not only Q / E stepping through them).
	UShinobiPauseMenuWidget* Pause = CreateWidget<UShinobiPauseMenuWidget>(W.World, UShinobiPauseMenuWidget::StaticClass());
	Pause->TakeWidget();
	for (const EShinobiPauseTab Tab : { EShinobiPauseTab::Controls, EShinobiPauseTab::Game, EShinobiPauseTab::Settings, EShinobiPauseTab::Jutsu })
	{
		UShinobiIndexButton* Button = Pause->GetTabButton(Tab);
		TestNotNull(TEXT("pause menu: a button per tab"), Button);
		if (Button)
		{
			Button->OnClicked.Broadcast();
			TestTrue(TEXT("pause menu: the clicked tab opens"), Pause->GetTab() == Tab);
		}
	}
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	for (const EShinobiMenuTab Tab : { EShinobiMenuTab::Controls, EShinobiMenuTab::Shinobi, EShinobiMenuTab::Settings, EShinobiMenuTab::Play, EShinobiMenuTab::Jutsu })
	{
		UShinobiIndexButton* Button = Menu->GetTabButton(Tab);
		TestNotNull(TEXT("main menu: a button per tab"), Button);
		if (Button)
		{
			Button->OnClicked.Broadcast();
			TestTrue(TEXT("main menu: the clicked tab opens"), Menu->GetTab() == Tab);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsPanelStepTest, "ShinobiCoop.UI.SettingsPanelStepsBothWays", ShinobiTestFlags)
bool FShinobiSettingsPanelStepTest::RunTest(const FString& Parameters)
{
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const bool bOldVSync = Settings->IsVSyncEnabled();
	const float OldCap = Settings->GetFrameRateLimit();
	FShinobiTestWorld W;
	UShinobiSettingsWidget* Panel = CreateWidget<UShinobiSettingsWidget>(W.World, UShinobiSettingsWidget::StaticClass());
	Panel->bMainMenu = true;
	Panel->TakeWidget();
	const TArray<ShinobiSettings::ERow> Rows = ShinobiSettings::RowsFor(true);
	const int32 Cap = Rows.IndexOfByKey(ShinobiSettings::ERow::FrameCap);
	TestTrue(TEXT("the frame cap row is shown"), Cap != INDEX_NONE);
	TestNotNull(TEXT("a first row to focus"), Panel->GetFirstEntry());
	Settings->SetFrameRateLimit(60.f);
	Panel->StepRow(Cap, 1);
	TestEqual(TEXT("on: 60 -> 120"), Settings->GetFrameRateLimit(), 120.f);
	TestEqual(TEXT("the row shows the new value"), Panel->GetShownValue(Cap).ToString(), FString(TEXT("120 FPS")));
	Panel->StepRow(Cap, -1);
	Panel->StepRow(Cap, -1);
	TestEqual(TEXT("back twice: 120 -> 30"), Settings->GetFrameRateLimit(), 30.f);
	TestEqual(TEXT("the row follows"), Panel->GetShownValue(Cap).ToString(), FString(TEXT("30 FPS")));
	TestFalse(TEXT("a help line"), Panel->GetDescriptionText().IsEmpty());
	const int32 Quality = Rows.IndexOfByKey(ShinobiSettings::ERow::Quality);
	const int32 OldQuality = Settings->GetOverallScalabilityLevel();
	Panel->PickRow(Quality, 4); // as picked from the row's dropdown
	TestEqual(TEXT("quality picked from its list"), Settings->GetOverallScalabilityLevel(), 4);
	TestEqual(TEXT("the row shows it"), Panel->GetShownValue(Quality).ToString(), ShinobiSettings::RowOptions(ShinobiSettings::ERow::Quality)[4]);
	Settings->SetOverallScalabilityLevel(OldQuality);

	Settings->SetVSyncEnabled(bOldVSync);
	Settings->SetFrameRateLimit(OldCap);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
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
	UShinobiDropdown* Outfits = Page->GetOutfitField();
	if (TestNotNull(TEXT("outfits: a dropdown"), Outfits))
	{
		TestEqual(TEXT("every outfit listed"), Outfits->GetOptions().Num(), int32(UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits)));
		Outfits->Choose(1);
		TestEqual(TEXT("picked from the list"), ShinobiSettings::GetSavedOutfit(), FString(AShinobiPlayerCharacter::Outfits[1].Name));
		TestEqual(TEXT("the field shows the pick"), Outfits->GetSelected(), 1);
	}

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

namespace
{
	/** Visible all the way up (a collapsed list hides its rows from focus). */
	bool MenuTestShown(const UWidget* Widget)
	{
		for (; Widget; Widget = Widget->GetParent())
		{
			if (Widget->GetVisibility() == ESlateVisibility::Collapsed || Widget->GetVisibility() == ESlateVisibility::Hidden)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuReturnKeepsPickTest, "ShinobiCoop.UI.MainMenuReturnKeepsPick", ShinobiTestFlags)
bool FShinobiMainMenuReturnKeepsPickTest::RunTest(const FString& Parameters)
{
	const FString OldCharacter = ShinobiSettings::GetSavedCharacter();
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();

	// PLAY: focus returns to the chosen map's row, so the focus-follow doesn't switch maps.
	Menu->TourSelectHost(TEXT("Arena: Camden"), 1);
	const int32 Camden = UShinobiSessionSubsystem::GetHostOrder().IndexOfByKey(UShinobiSessionSubsystem::FindMapByName(TEXT("Arena: Camden")));
	Menu->SelectTab(EShinobiMenuTab::Jutsu);
	Menu->SelectTab(EShinobiMenuTab::Play);
	TestTrue(TEXT("focus on the chosen map's row"), Menu->GetFocusTarget(EShinobiMenuTab::Play) == Menu->GetHostRow(Camden));

	// FIND GAMES: something visible gets focus.
	Menu->TourShowFind();
	Menu->SelectTab(EShinobiMenuTab::Jutsu);
	Menu->SelectTab(EShinobiMenuTab::Play);
	TestTrue(TEXT("a visible focus target in Find Games"), MenuTestShown(Menu->GetFocusTarget(EShinobiMenuTab::Play)));

	// SHINOBI: focus returns to the card picked now, not the one picked when the menu opened.
	UShinobiRosterPage* Roster = Menu->GetRosterPage();
	if (TestNotNull(TEXT("roster page"), Roster))
	{
		Roster->Pick(2);
		TestTrue(TEXT("focus on the new pick"), Menu->GetFocusTarget(EShinobiMenuTab::Shinobi) == Roster->GetFocusTarget());
	}
	ShinobiSettings::SetSavedCharacter(OldCharacter);
	GConfig->Flush(false, GGameUserSettingsIni);
	return true;
}


namespace
{
	/** The saved made characters as they were, put back when a test is done with them. */
	struct FMenuTestSaveGuard
	{
		bool bExisted = UGameplayStatics::DoesSaveGameExist(UShinobiCharacterSave::SlotName, 0);
		TArray<FShinobiCharacterSlot> Slots = UShinobiCharacterSave::Load()->Slots;
		int32 Active = UShinobiCharacterSave::Load()->Active;
		FString Character = ShinobiSettings::GetSavedCharacter();

		~FMenuTestSaveGuard()
		{
			ShinobiSettings::SetSavedCharacter(Character);
			if (!bExisted)
			{
				UGameplayStatics::DeleteGameInSlot(UShinobiCharacterSave::SlotName, 0);
				return;
			}
			UShinobiCharacterSave* Save = UShinobiCharacterSave::Load();
			Save->Slots = Slots;
			Save->Active = Active;
			Save->Save();
		}
	};

	/** The button under Root whose text reads Label, or null. */
	UShinobiIndexButton* MenuTestButton(UUserWidget& Root, const FString& Label)
	{
		UShinobiIndexButton* Found = nullptr;
		Root.WidgetTree->ForEachWidget([&Found, &Label](UWidget* Widget)
		{
			UShinobiIndexButton* Button = Cast<UShinobiIndexButton>(Widget);
			const UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
			Found = !Found && Text && Text->GetText().ToString() == Label ? Button : Found;
		});
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuCreateCardTest, "ShinobiCoop.UI.MainMenuCreateCard", ShinobiTestFlags)
bool FShinobiMainMenuCreateCardTest::RunTest(const FString& Parameters)
{
	const FMenuTestSaveGuard Guard;
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // one made character, the roster pick played
		Save->Slots.SetNum(1);
		Save->Slots[0].Name = TEXT("Test Made");
		Save->Active = INDEX_NONE;
		Save->Save();
	}
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	for (int32 Index = 0; Index < int32(EShinobiMenuTab::Num); ++Index)
	{
		const UTextBlock* Label = Cast<UTextBlock>(Menu->GetTabButton(EShinobiMenuTab(Index))->GetChildAt(0));
		TestTrue(TEXT("no CREATE tab (it's in SHINOBI)"), Label && Label->GetText().ToString() != TEXT("CREATE"));
	}
	Menu->SelectTab(EShinobiMenuTab::Shinobi);
	UShinobiRosterPage* Roster = Menu->GetRosterPage();
	if (!TestNotNull(TEXT("roster page"), Roster))
	{
		return false;
	}
	TestNotNull(TEXT("a card for the made character"), Roster->GetMadeCard(0));
	const auto MadeButtonsShown = [Roster]() { return Roster->GetEditButton()->GetParent()->GetVisibility() != ESlateVisibility::Collapsed; };
	TestFalse(TEXT("EDIT hidden on a roster pick"), MadeButtonsShown());

	// "+" opens the creator on the next free slot; BACK returns to the strip.
	UShinobiIndexButton* Plus = Roster->GetCreateCard();
	if (TestNotNull(TEXT("a + card"), Plus))
	{
		Plus->OnClicked.Broadcast();
		TestTrue(TEXT("+ opens the creator in SHINOBI"), Menu->IsCreating() && Menu->GetTab() == EShinobiMenuTab::Shinobi);
		TestEqual(TEXT("on the free slot"), Menu->GetCreatorPage()->GetEditedSlot(), 1);
		TestFalse(TEXT("a new character"), Menu->GetCreatorPage()->IsEditingMade());
		UShinobiIndexButton* Back = MenuTestButton(*Menu->GetCreatorPage(), TEXT("< BACK"));
		if (TestNotNull(TEXT("a BACK button"), Back))
		{
			Back->OnClicked.Broadcast();
			TestFalse(TEXT("BACK returns to the strip"), Menu->IsCreating());
		}
	}

	// A made card plays it; EDIT opens it; DELETE (twice) removes it and the roster pick plays again.
	Roster->GetMadeCard(0)->OnClicked.Broadcast();
	TestEqual(TEXT("the made character is played"), UShinobiCharacterSave::Load()->Active, 0);
	TestTrue(TEXT("its card has focus next"), Roster->GetFocusTarget() == Roster->GetMadeCard(0));
	TestTrue(TEXT("EDIT shown"), MadeButtonsShown());
	Roster->GetEditButton()->OnClicked.Broadcast();
	TestTrue(TEXT("EDIT opens the creator on it"), Menu->IsCreating() && Menu->GetCreatorPage()->GetEditedSlot() == 0 && Menu->GetCreatorPage()->IsEditingMade());
	Menu->CloseCreator();
	Roster->GetDeleteButton()->OnClicked.Broadcast();
	TestEqual(TEXT("one press doesn't delete"), UShinobiCharacterSave::Load()->Slots.Num(), 1);
	Roster->GetDeleteButton()->OnClicked.Broadcast();
	TestEqual(TEXT("the second press deletes"), UShinobiCharacterSave::Load()->Slots.Num(), 0);
	TestEqual(TEXT("the roster pick is played"), UShinobiCharacterSave::Load()->Active, int32(INDEX_NONE));
	TestNull(TEXT("its card is gone"), Roster->GetMadeCard(0));

	// Every slot used: no "+".
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load();
		Save->Slots.SetNum(UShinobiCharacterSave::MaxSlots);
		for (FShinobiCharacterSlot& Slot : Save->Slots)
		{
			Slot.Name = TEXT("Full");
		}
		Save->Save();
	}
	Roster->Show();
	TestEqual(TEXT("no free slot"), UShinobiRosterPage::FindFreeSlot(), int32(INDEX_NONE));
	TestNull(TEXT("+ hidden when the slots are full"), Roster->GetCreateCard());
	return true;
}

namespace
{
	/** A tiny PNG (3 x 4, one colour) standing in for a filmed portrait. */
	TArray<uint8> MenuTestPortraitPng()
	{
		TArray<FColor> Pixels;
		Pixels.Init(FColor(200, 40, 30, 255), 3 * 4);
		TArray64<uint8> Png;
		FImageUtils::CompressImage(Png, TEXT("png"), FImageView(Pixels.GetData(), 3, 4));
		return TArray<uint8>(Png.GetData(), int32(Png.Num()));
	}

	/** The picture on a SHINOBI strip card (MakeCard: card > column > size box > overlay > image), or null. */
	UObject* MenuTestCardPicture(const UShinobiIndexButton* Card)
	{
		const UPanelWidget* Column = Card ? Cast<UPanelWidget>(Card->GetChildAt(0)) : nullptr;
		const UPanelWidget* Box = Column ? Cast<UPanelWidget>(Column->GetChildAt(0)) : nullptr;
		const UPanelWidget* Layers = Box ? Cast<UPanelWidget>(Box->GetChildAt(0)) : nullptr;
		const UImage* Picture = Layers ? Cast<UImage>(Layers->GetChildAt(0)) : nullptr;
		return Picture ? Picture->GetBrush().GetResourceObject() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMadePortraitTest, "ShinobiCoop.UI.MadeCharacterPortrait", ShinobiTestFlags)
bool FShinobiMadePortraitTest::RunTest(const FString& Parameters)
{
	// A made character's portrait (filmed by the menu stage) is kept with it, shown on its card, and survives SAVE & PLAY
	// unless the look changed.
	const FMenuTestSaveGuard Guard;
	const TArray<uint8> Png = MenuTestPortraitPng();
	if (!TestTrue(TEXT("a test PNG"), Png.Num() > 0))
	{
		return false;
	}
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // one with a portrait, one without
		Save->Slots.Reset();
		Save->Slots.SetNum(2);
		Save->Slots[0].Name = TEXT("Pictured");
		Save->Slots[0].Portrait = Png;
		Save->Slots[1].Name = TEXT("Unpictured");
		Save->Slots[1].Look.Sex = EShinobiSex::Female;
		Save->Active = INDEX_NONE;
		Save->Save();
	}
	const UShinobiCharacterSave* Saved = UShinobiCharacterSave::Load();
	TestTrue(TEXT("the portrait is saved with the character"), Saved->Slots[0].Portrait == Png);
	UTexture2D* Texture = ShinobiMenuArt::MadePortrait(Saved->Slots[0]);
	if (TestNotNull(TEXT("the portrait loads as a texture"), Texture))
	{
		TestEqual(TEXT("its width"), Texture->GetSizeX(), 3);
		TestEqual(TEXT("its height"), Texture->GetSizeY(), 4);
	}
	TestNull(TEXT("no portrait yet: no texture"), ShinobiMenuArt::MadePortrait(Saved->Slots[1]));
	TestTrue(TEXT("FindSlot: the character wearing a look"), Saved->FindSlot(Saved->Slots[1].Look) == &Saved->Slots[1]);

	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	Menu->SelectTab(EShinobiMenuTab::Shinobi);
	UShinobiRosterPage* Roster = Menu->GetRosterPage();
	if (!TestNotNull(TEXT("roster page"), Roster))
	{
		return false;
	}
	TestTrue(TEXT("the pictured card shows its portrait"), Cast<UTexture2D>(MenuTestCardPicture(Roster->GetMadeCard(0))) != nullptr);
	TestNull(TEXT("the other a dark card (its initial)"), Cast<UTexture2D>(MenuTestCardPicture(Roster->GetMadeCard(1))));
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // (as the stage hands one back)
		Save->Slots[1].Portrait = Png;
		Save->Save();
	}
	Roster->Show();
	TestTrue(TEXT("a portrait filmed later shows on its card"), Cast<UTexture2D>(MenuTestCardPicture(Roster->GetMadeCard(1))) != nullptr);

	// SAVE & PLAY with the look unchanged keeps the portrait; a changed look drops it (to be filmed again).
	Menu->OpenCreator(0);
	Menu->GetCreatorPage()->SaveAndPlay();
	TestTrue(TEXT("unchanged look: the portrait stays"), UShinobiCharacterSave::Load()->Slots[0].Portrait == Png);
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // (the slot's look as it was before an edit)
		Save->Slots[0].Look.Sex = EShinobiSex::Female;
		Save->Save();
	}
	Menu->GetCreatorPage()->SaveAndPlay(); // (the creator still holds the male look)
	TestTrue(TEXT("changed look: the portrait goes"), UShinobiCharacterSave::Load()->Slots[0].Portrait.IsEmpty());
	TestTrue(TEXT("the other character's portrait isn't touched"), UShinobiCharacterSave::Load()->Slots[1].Portrait == Png);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorSavePortraitTest, "ShinobiCoop.UI.CreatorSaveTakesPortraitFirst", ShinobiTestFlags)
bool FShinobiCreatorSavePortraitTest::RunTest(const FString& Parameters)
{
	// SAVE & PLAY has the stage film the portrait before the character is saved and the strip comes back; the menu can't
	// leave for PLAY meanwhile.
	const FMenuTestSaveGuard Guard;
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // no made characters yet
		Save->Slots.Reset();
		Save->Active = INDEX_NONE;
		Save->Save();
	}
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	Menu->SelectTab(EShinobiMenuTab::Shinobi);
	Menu->OpenCreator(0);
	UShinobiCreatorPage* Creator = Menu->GetCreatorPage();
	Creator->SaveAndPlay();
	if (!TestTrue(TEXT("it waits for the portrait"), Creator->IsSaving()))
	{
		return false;
	}
	TestTrue(TEXT("the stage films the look being saved"), Stage->IsPortraitPending() && Stage->GetPortraitLook() == Creator->GetLook());
	TestTrue(TEXT("nothing is saved yet"), UShinobiCharacterSave::Load()->GetMadeSlots().IsEmpty());
	Menu->SelectTab(EShinobiMenuTab::Play);
	TestTrue(TEXT("no leaving for PLAY meanwhile"), Menu->IsCreating() && Menu->GetTab() == EShinobiMenuTab::Shinobi);
	Menu->CloseCreator();
	TestTrue(TEXT("nor back to the strip"), Menu->IsCreating());

	const TArray<uint8> Png = MenuTestPortraitPng();
	Creator->FinishSave(TArray<uint8>(Png)); // (as the stage hands it over)
	const UShinobiCharacterSave* Saved = UShinobiCharacterSave::Load();
	TestTrue(TEXT("saved with its portrait"), Saved->Slots.IsValidIndex(0) && Saved->Slots[0].Portrait == Png);
	TestEqual(TEXT("and played"), Saved->Active, 0);
	TestFalse(TEXT("no longer waiting"), Creator->IsSaving());
	TestFalse(TEXT("then back to the strip"), Menu->IsCreating());
	TestFalse(TEXT("the stage's request is done"), Stage->IsPortraitPending());
	TestTrue(TEXT("its card shows the portrait at once"), Cast<UTexture2D>(MenuTestCardPicture(Menu->GetRosterPage()->GetMadeCard(0))) != nullptr);

	// No picture to be had: it saves without one once the wait runs out (the strip films it later).
	Menu->OpenCreator(1);
	Creator->SaveAndPlay();
	TestTrue(TEXT("a second character waits too"), Creator->IsSaving());
	Creator->TickSave(UShinobiCreatorPage::SaveTimeoutSeconds);
	TestFalse(TEXT("the wait runs out"), Creator->IsSaving());
	Saved = UShinobiCharacterSave::Load();
	TestTrue(TEXT("saved without a portrait"), Saved->Slots.IsValidIndex(1) && !Saved->Slots[1].Name.IsEmpty() && Saved->Slots[1].Portrait.IsEmpty());
	TestFalse(TEXT("and back to the strip"), Menu->IsCreating());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStagePortraitRequestTest, "ShinobiCoop.UI.MenuStagePortraitRequest", ShinobiTestFlags)
bool FShinobiStagePortraitRequestTest::RunTest(const FString& Parameters)
{
	// A portrait request waits for its look; a roster body, another request's look or CancelPortrait drops it.
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	FShinobiAppearance Look;
	Stage->RequestPortrait(Look, [](TArray<uint8>&&) {});
	TestTrue(TEXT("pending"), Stage->IsPortraitPending());
	TestTrue(TEXT("for that look"), Stage->GetPortraitLook() == Look);
	Stage->Show(0, 0);
	TestFalse(TEXT("a roster body drops it"), Stage->IsPortraitPending());
	Stage->RequestPortrait(Look, [](TArray<uint8>&&) {});
	Stage->CancelPortrait();
	TestFalse(TEXT("CancelPortrait drops it"), Stage->IsPortraitPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSettingsMadeCharacterTest, "ShinobiCoop.UI.SettingsCharacterListsMadeOnes", ShinobiTestFlags)
bool FShinobiSettingsMadeCharacterTest::RunTest(const FString& Parameters)
{
	// The pause menu's Character row: the roster, then the made characters; the played one is selected.
	const FMenuTestSaveGuard Guard;
	using ShinobiSettings::ERow;
	constexpr int32 RosterCount = UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster);
	{
		UShinobiCharacterSave* Save = UShinobiCharacterSave::Load(); // two made characters with an empty slot between
		Save->Slots.Reset();
		Save->Slots.SetNum(3);
		Save->Slots[0].Name = TEXT("Made One");
		Save->Slots[2].Name = TEXT("Made Two");
		Save->Active = INDEX_NONE;
		Save->Save();
	}
	ShinobiSettings::SetSavedCharacter(AShinobiPlayerCharacter::Roster[0].Name);
	const TArray<FString> Options = ShinobiSettings::RowOptions(ERow::Character);
	if (!TestEqual(TEXT("the roster, then the made characters"), Options.Num(), RosterCount + 2))
	{
		return false;
	}
	TestEqual(TEXT("the first made one"), Options[RosterCount], FString(TEXT("Made One")));
	TestEqual(TEXT("the second made one"), Options[RosterCount + 1], FString(TEXT("Made Two")));
	TestEqual(TEXT("a roster pick: it's selected"), ShinobiSettings::RowIndex(ERow::Character), 0);

	ShinobiSettings::SetRowIndex(ERow::Character, nullptr, RosterCount + 1);
	TestEqual(TEXT("picking a made one plays it"), UShinobiCharacterSave::Load()->Active, 2);
	TestEqual(TEXT("it's selected, not the last roster pick"), ShinobiSettings::RowIndex(ERow::Character), RosterCount + 1);
	TestEqual(TEXT("its name is shown"), ShinobiSettings::RowValue(ERow::Character), FString(TEXT("Made Two")));
	TestEqual(TEXT("no faces to pick for it"), ShinobiSettings::RowOptions(ERow::Face).Num(), 1);
	ShinobiSettings::Step(ERow::Face, nullptr, 1);
	TestEqual(TEXT("the Face row leaves it played"), UShinobiCharacterSave::Load()->Active, 2);

	ShinobiSettings::SetRowIndex(ERow::Character, nullptr, 1 % RosterCount);
	TestEqual(TEXT("a roster pick stops playing the made one"), UShinobiCharacterSave::Load()->Active, int32(INDEX_NONE));
	TestEqual(TEXT("the roster pick is saved"), ShinobiSettings::GetSavedCharacter(), FString(AShinobiPlayerCharacter::Roster[1 % RosterCount].Name));
	TestEqual(TEXT("and selected"), ShinobiSettings::RowIndex(ERow::Character), 1 % RosterCount);
	return true;
}

namespace
{
	/** The stage body's head bone height in the stage's space (as AShinobiMenuStage frames it). */
	float MenuTestHeadHeight(const AShinobiMenuStage& Stage)
	{
		const UShinobiBodyComponent* Body = Stage.GetBody();
		for (int32 Bone = 0; Bone < Body->GetNumBones(); ++Bone)
		{
			if (Body->GetBoneName(Bone).ToString().EndsWith(TEXT("Head"), ESearchCase::IgnoreCase))
			{
				return Stage.GetActorTransform().InverseTransformPosition(Body->GetBoneLocation(Body->GetBoneName(Bone))).Z;
			}
		}
		return 165.f;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageZoomFramesHeadTest, "ShinobiCoop.UI.MenuStageZoomFramesHead", ShinobiTestFlags)
bool FShinobiMenuStageZoomFramesHeadTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	Stage->SetAutoZoom(1.f);
	W.Tick(2.f);
	TestEqual(TEXT("close-up FOV"), Stage->GetCamera()->FieldOfView, 22.f, 0.5f);
	TestEqual(TEXT("camera at the head"), float(Stage->GetCamera()->GetRelativeLocation().Z), MenuTestHeadHeight(*Stage), 10.f);
	Stage->SetAutoZoom(0.f);
	W.Tick(2.f);
	TestEqual(TEXT("full-body FOV"), Stage->GetCamera()->FieldOfView, 45.f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageYawTurnsCharacterTest, "ShinobiCoop.UI.MenuStageYawTurnsCharacter", ShinobiTestFlags)
bool FShinobiMenuStageYawTurnsCharacterTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	W.Tick(0.5f);
	const FTransform Camera = Stage->GetCamera()->GetRelativeTransform();
	Stage->AddYaw(90.f);
	W.Tick(2.f);
	TestEqual(TEXT("turned"), Stage->GetYaw(), 90.f, 1.f);
	TestEqual(TEXT("the character turns"), float(Stage->GetDriver()->GetRelativeRotation().Yaw), 0.f, 1.f);
	TestTrue(TEXT("the camera stays"), Stage->GetCamera()->GetRelativeTransform().Equals(Camera, 0.1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageZoomClampsTest, "ShinobiCoop.UI.MenuStageZoomClamps", ShinobiTestFlags)
bool FShinobiMenuStageZoomClampsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	Stage->AddZoom(5.f);
	W.Tick(3.f);
	TestEqual(TEXT("all the way in"), Stage->GetZoom(), 1.f, 0.01f);
	Stage->AddZoom(-5.f);
	W.Tick(3.f);
	TestEqual(TEXT("all the way out"), Stage->GetZoom(), 0.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageResetViewTest, "ShinobiCoop.UI.MenuStageResetView", ShinobiTestFlags)
bool FShinobiMenuStageResetViewTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	Stage->SetAutoZoom(1.f);
	Stage->AddYaw(120.f);
	Stage->AddZoom(-0.5f);
	W.Tick(1.f);
	Stage->ResetView();
	W.Tick(3.f);
	TestEqual(TEXT("facing front"), Stage->GetYaw(), 0.f, 1.f);
	TestEqual(TEXT("back to the tab's framing"), Stage->GetZoom(), 1.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuHintsFollowDeviceTest, "ShinobiCoop.UI.MainMenuHintsFollowDevice", ShinobiTestFlags)
bool FShinobiMainMenuHintsFollowDeviceTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	Menu->UseGamepadHints(false);
	const FString Mouse = Menu->GetHintText().ToString();
	TestTrue(TEXT("mouse hints"), Mouse.Contains(TEXT("Wheel")) && Mouse.Contains(TEXT("Drag")));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const UWidget* Icon = Menu->GetHintIcon(Index);
		TestTrue(TEXT("a mouse picture per hint, shown for the mouse"), Icon && Icon->GetVisibility() != ESlateVisibility::Collapsed);
	}
	Menu->UseGamepadHints(true);
	TestTrue(TEXT("gamepad hints"), Menu->GetHintText().ToString().Contains(TEXT("RS")));
	const UWidget* ZoomIcon = Menu->GetHintIcon(0);
	TestTrue(TEXT("no mouse picture for a gamepad"), ZoomIcon && ZoomIcon->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStudioSetLightRigTest, "ShinobiCoop.UI.StudioSetLightRig", ShinobiTestFlags)
bool FShinobiStudioSetLightRigTest::RunTest(const FString& Parameters)
{
	// Key front-left above, fill from the right at about 35 %, a cool rim from behind (the camera looks along -X from +X,
	// so screen-left is +Y).
	FShinobiTestWorld W;
	const AShinobiStudioSet* Set = W.Spawn<AShinobiStudioSet>();
	const FVector Key = Set->GetKeyLight()->GetRelativeLocation();
	const FVector Fill = Set->GetFillLight()->GetRelativeLocation();
	const FVector Rim = Set->GetRimLight()->GetRelativeLocation();
	TestTrue(TEXT("key in front, left, above"), Key.X > 0.f && Key.Y > 0.f && Key.Z > 200.f);
	TestTrue(TEXT("fill from the right"), Fill.Y < 0.f);
	TestTrue(TEXT("rim behind"), Rim.X < 0.f);
	TestEqual(TEXT("fill at 35 % of the key"), Set->GetFillLight()->Intensity, 0.35f * Set->GetKeyLight()->Intensity, 0.01f * Set->GetKeyLight()->Intensity);
	TestTrue(TEXT("rim cool"), Set->GetRimLight()->GetLightColor().B > Set->GetRimLight()->GetLightColor().R);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStudioPresetsRetuneTest, "ShinobiCoop.UI.StudioPresetsRetune", ShinobiTestFlags)
bool FShinobiStudioPresetsRetuneTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiStudioSet* Set = W.Spawn<AShinobiStudioSet>();
	const auto Tint = [Set]()
	{
		FLinearColor Value = FLinearColor::Transparent;
		const UMaterialInterface* Material = Set->GetBackdrop()->GetMaterial(0);
		return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Tint")), Value) ? Value : FLinearColor::Transparent;
	};
	Set->ApplyPreset(EShinobiStudioLight::Studio);
	const FLinearColor StudioKey = Set->GetKeyLight()->GetLightColor();
	const FLinearColor StudioTint = Tint();
	TestNotEqual(TEXT("the backdrop takes a tint"), StudioTint, FLinearColor::Transparent);
	UMaterialInterface* Backdrop = Set->GetBackdrop()->GetMaterial(0);
	TestEqual(TEXT("on the studio backdrop material (Scripts/setup_menu.py)"), Backdrop ? Backdrop->GetBaseMaterial()->GetName() : FString(), FString(TEXT("M_StudioBackdrop")));
	for (const EShinobiStudioLight Preset : { EShinobiStudioLight::Day, EShinobiStudioLight::Dusk })
	{
		Set->ApplyPreset(Preset);
		TestTrue(TEXT("its own key colour"), !Set->GetKeyLight()->GetLightColor().Equals(StudioKey, 0.01f));
		TestTrue(TEXT("its own backdrop tint"), !Tint().Equals(StudioTint, 0.01f));
	}
	Set->ApplyPreset(EShinobiStudioLight::Studio);
	TestTrue(TEXT("Studio again"), Set->GetKeyLight()->GetLightColor().Equals(StudioKey, 0.001f) && Tint().Equals(StudioTint, 0.001f));
	TestTrue(TEXT("remembers it"), Set->GetPreset() == EShinobiStudioLight::Studio);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageMovesToStudioTest, "ShinobiCoop.UI.MenuStageMovesToStudio", ShinobiTestFlags)
bool FShinobiMenuStageMovesToStudioTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	const AShinobiStudioSet* Set = W.Spawn<AShinobiStudioSet>(FVector(0.f, 0.f, 20000.f));
	const FVector Home = Stage->GetActorLocation();
	Stage->SetStudio(true);
	TestTrue(TEXT("in the studio"), Stage->IsInStudio() && Stage->GetActorLocation().Equals(Set->GetActorLocation(), 1.f));
	Stage->SetStudio(false);
	TestTrue(TEXT("back on the roof"), !Stage->IsInStudio() && Stage->GetActorLocation().Equals(Home, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageLeavesStudioMidFadeTest, "ShinobiCoop.UI.MenuStageLeavesStudioMidFade", ShinobiTestFlags)
bool FShinobiMenuStageLeavesStudioMidFadeTest::RunTest(const FString& Parameters)
{
	// The creator fades to black, moves the stage into the studio and fades back; leaving halfway turns the fade around.
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	W.Spawn<AShinobiStudioSet>(FVector(0.f, 0.f, 20000.f));
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	Menu->OpenCreator(0);
	for (int32 Step = 0; Step < 20; ++Step) { Menu->TickStudioFade(0.05f); }
	TestTrue(TEXT("the creator is in the studio"), Stage->IsInStudio());
	TestEqual(TEXT("faded back in"), Menu->GetStudioFade(), 0.f, 0.001f);
	Menu->CloseCreator();
	Menu->TickStudioFade(0.1f); // halfway to black
	Menu->OpenCreator(0);
	Menu->SelectTab(EShinobiMenuTab::Jutsu); // (another tab leaves the creator too)
	TestFalse(TEXT("another tab closes the creator"), Menu->IsCreating());
	Menu->SelectTab(EShinobiMenuTab::Shinobi);
	for (int32 Step = 0; Step < 20; ++Step) { Menu->TickStudioFade(0.05f); }
	TestFalse(TEXT("SHINOBI is on the roof"), Stage->IsInStudio());
	TestEqual(TEXT("no black left"), Menu->GetStudioFade(), 0.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiStudioLightsStayInStudioTest, "ShinobiCoop.UI.StudioLightsStayInStudio", ShinobiTestFlags)
bool FShinobiStudioLightsStayInStudioTest::RunTest(const FString& Parameters)
{
	// Placed as Scripts/setup_menu.py places it: none of its lights reaches the rooftop stage.
	FShinobiTestWorld W;
	const AShinobiStudioSet* Set = W.Spawn<AShinobiStudioSet>(FVector(0.f, 0.f, AShinobiStudioSet::HeightAboveStageCm));
	for (const ULocalLightComponent* Light : { Set->GetKeyLight(), Set->GetFillLight(), Set->GetRimLight() })
	{
		TestTrue(TEXT("light ends before the roof"), Light->AttenuationRadius < Light->GetComponentLocation().Size());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMainMenuPreviewCatcherTest, "ShinobiCoop.UI.MainMenuPreviewInputOnlyFromOpenStage", ShinobiTestFlags)
bool FShinobiMainMenuPreviewCatcherTest::RunTest(const FString& Parameters)
{
	// Drags and the wheel move the preview only on the open stage: a catcher under the pages takes them, so a press on a
	// panel's background or the wheel at a list's end doesn't turn or zoom the character.
	FShinobiTestWorld W;
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	const UWidget* Catcher = Menu->GetPreviewCatcher();
	const UPanelWidget* Root = Cast<UPanelWidget>(Menu->WidgetTree->RootWidget);
	if (!TestNotNull(TEXT("a catcher"), Catcher) || !TestNotNull(TEXT("root"), Root))
	{
		return false;
	}
	TestTrue(TEXT("it takes clicks"), Catcher->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("under the pages"), Root->GetChildIndex(Catcher) != INDEX_NONE && Root->GetChildIndex(Catcher) < Root->GetChildIndex(Menu->GetPagesWidget()));
	TestTrue(TEXT("the root itself doesn't"), Root->GetVisibility() != ESlateVisibility::Visible);
	const UCanvasPanelSlot* CatcherSlot = Cast<UCanvasPanelSlot>(Catcher->Slot);
	TestTrue(TEXT("it leaves the tab bar clear (a click on a tab reaches the tab)"), CatcherSlot && CatcherSlot->GetOffsets().Top > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMenuStageKeepsHeightOnWideTest, "ShinobiCoop.UI.MenuStageKeepsHeightOnWide", ShinobiTestFlags)
bool FShinobiMenuStageKeepsHeightOnWideTest::RunTest(const FString& Parameters)
{
	// An ultrawide window shows more to the sides, not less above and below (the character's head stays in frame).
	FShinobiTestWorld W;
	const AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	TestTrue(TEXT("keeps the vertical field of view"), Stage->GetCamera()->bOverrideAspectRatioAxisConstraint
		&& Stage->GetCamera()->AspectRatioAxisConstraint == EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiPreviewTurnsWithRightMouseTest, "ShinobiCoop.UI.PreviewTurnsWithRightMouse", ShinobiTestFlags)
bool FShinobiPreviewTurnsWithRightMouseTest::RunTest(const FString& Parameters)
{
	// Holding the right mouse button turns the preview (the left one stays for the panels); a drag right turns the
	// character's front to the right.
	TestTrue(TEXT("RMB turns"), UShinobiPreviewCatcher::TurnsPreview(EKeys::RightMouseButton));
	TestFalse(TEXT("LMB doesn't"), UShinobiPreviewCatcher::TurnsPreview(EKeys::LeftMouseButton));
	TestEqual(TEXT("a drag right turns it right"), UShinobiPreviewCatcher::TurnForDrag(10.f), -4.f, 0.001f);
	return true;
}

#endif
