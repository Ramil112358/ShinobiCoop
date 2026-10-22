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

#endif
