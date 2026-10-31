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
	TestFalse(TEXT("main menu: no Face row"), Menu.Contains(ERow::Face));
	TestFalse(TEXT("main menu: no Outfit row"), Menu.Contains(ERow::Outfit));
	TestEqual(TEXT("main menu rows"), Menu.Num(), int32(ERow::Num) - 3);
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
	for (const EShinobiMenuTab Tab : { EShinobiMenuTab::Play, EShinobiMenuTab::Shinobi, EShinobiMenuTab::Create, EShinobiMenuTab::Jutsu, EShinobiMenuTab::Settings })
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
	UShinobiRosterPage* Roster = Cast<UShinobiRosterPage>(Menu->GetTabPage(EShinobiMenuTab::Shinobi));
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
	Menu->UseGamepadHints(true);
	TestTrue(TEXT("gamepad hints"), Menu->GetHintText().ToString().Contains(TEXT("RS")));
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
	// CREATE fades to black, moves the stage into the studio and fades back; leaving halfway turns the fade around.
	FShinobiTestWorld W;
	AShinobiMenuStage* Stage = W.Spawn<AShinobiMenuStage>();
	W.Spawn<AShinobiStudioSet>(FVector(0.f, 0.f, 20000.f));
	UShinobiMainMenuWidget* Menu = CreateWidget<UShinobiMainMenuWidget>(W.World, UShinobiMainMenuWidget::StaticClass());
	Menu->TakeWidget();
	Menu->SelectTab(EShinobiMenuTab::Create);
	for (int32 Step = 0; Step < 20; ++Step) { Menu->TickStudioFade(0.05f); }
	TestTrue(TEXT("CREATE is in the studio"), Stage->IsInStudio());
	TestEqual(TEXT("faded back in"), Menu->GetStudioFade(), 0.f, 0.001f);
	Menu->SelectTab(EShinobiMenuTab::Shinobi);
	Menu->TickStudioFade(0.1f); // halfway to black
	Menu->SelectTab(EShinobiMenuTab::Create);
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
