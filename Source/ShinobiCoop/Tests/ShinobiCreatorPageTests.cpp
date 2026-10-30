#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: the catalog and Mutable content are local)

#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Tests/AutomationCommon.h"

namespace
{
	/** A menu stage and a CREATE page on it, kept alive across latent waits. */
	struct FCreatorBench
	{
		FShinobiTestWorld World;
		AShinobiMenuStage* Stage = nullptr;
		UShinobiCreatorPage* Page = nullptr;

		FCreatorBench()
		{
			Stage = World.Spawn<AShinobiMenuStage>();
			Page = CreateWidget<UShinobiCreatorPage>(World.World, UShinobiCreatorPage::StaticClass());
			Page->AddToRoot();
			Page->Stage = Stage;
			Page->TakeWidget();
		}

		~FCreatorBench() { Page->RemoveFromRoot(); }
	};

	const UShinobiCreatorCatalog* CreatorTestCatalog(FAutomationTestBase& Test)
	{
		const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Test.TestNotNull(TEXT("creator catalog (Scripts/setup_creator.py, setup_mutable.py catalog)"), Catalog);
		return Catalog;
	}

	/** Every widget under Root, depth first. */
	void CreatorTestWidgets(UWidget* Root, TArray<UWidget*>& Out)
	{
		Out.Add(Root);
		if (const UPanelWidget* Panel = Cast<UPanelWidget>(Root))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				CreatorTestWidgets(Panel->GetChildAt(Index), Out);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorTabsTest, "ShinobiCoop.UI.CreatorTabs", ShinobiTestFlags)
bool FShinobiCreatorTabsTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	TArray<FString> Labels;
	for (const UShinobiIndexButton* Button : Bench.Page->GetCategoryButtons())
	{
		const UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0));
		Labels.Add(Label ? Label->GetText().ToString() : FString());
	}
	TestEqual(TEXT("the four categories"), FString::Join(Labels, TEXT("|")), FString(TEXT("CHARACTER|CLOTHES|BODY & SKIN|FACE")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorPresetPickTest, "ShinobiCoop.UI.CreatorPresetPick", ShinobiTestFlags)
bool FShinobiCreatorPresetPickTest::RunTest(const FString& Parameters)
{
	const UShinobiCreatorCatalog* Catalog = CreatorTestCatalog(*this);
	if (!Catalog || !TestTrue(TEXT("presets in the catalog"), Catalog->Presets.Num() > 0))
	{
		return false;
	}
	FCreatorBench Bench;
	for (int32 Index = 0; Index < Catalog->Presets.Num(); ++Index)
	{
		Bench.Page->ApplyPreset(Index);
		TestTrue(*FString::Printf(TEXT("%s's look"), *Catalog->Presets[Index].Roster.ToString()), Bench.Page->GetLook() == Catalog->Presets[Index].Look);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorHeadsBySexTest, "ShinobiCoop.UI.CreatorHeadsBySex", ShinobiTestFlags)
bool FShinobiCreatorHeadsBySexTest::RunTest(const FString& Parameters)
{
	const UShinobiCreatorCatalog* Catalog = CreatorTestCatalog(*this);
	if (!Catalog)
	{
		return false;
	}
	TArray<FString> Female;
	for (const int32 Head : UShinobiCreatorPage::HeadsOfSex(*Catalog, EShinobiSex::Female))
	{
		Female.Add(Catalog->Heads[Head].Id.ToString());
	}
	Female.Sort();
	TestEqual(TEXT("the female heads"), FString::Join(Female, TEXT(",")), FString(TEXT("Bika,Cat")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorSexSwitchTest, "ShinobiCoop.UI.CreatorSexSwitchKeepsValidLook", ShinobiTestFlags)
bool FShinobiCreatorSexSwitchTest::RunTest(const FString& Parameters)
{
	// A female look with Bika's head, a top made only for women and a late face, switched to male.
	const UShinobiCreatorCatalog* Catalog = CreatorTestCatalog(*this);
	if (!Catalog)
	{
		return false;
	}
	FShinobiAppearance Female;
	Female.Sex = EShinobiSex::Female;
	const TArray<int32> Heads = UShinobiCreatorPage::HeadsOfSex(*Catalog, EShinobiSex::Female);
	Female.Head = Heads.Num() > 0 ? uint8(Heads[0]) : FShinobiAppearance::None;
	uint8 WomenOnly = FShinobiAppearance::None;
	for (int32 Row = 0; Row < 255 && WomenOnly == FShinobiAppearance::None; ++Row)
	{
		const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(EShinobiWardrobeSlot::Top, uint8(Row));
		if (!Piece)
		{
			break;
		}
		WomenOnly = Piece->MaleMesh.IsNull() && !Piece->FemaleMesh.IsNull() ? uint8(Row) : WomenOnly;
	}
	TestNotEqual(TEXT("a women-only top (MHSample)"), WomenOnly, FShinobiAppearance::None);
	Female.Top = WomenOnly;
	Female.Face = uint8(FMath::Max(0, Catalog->CountFaces(EShinobiSex::Female) - 1));

	FCreatorBench Bench;
	Bench.Page->EditLook([&Female](FShinobiAppearance& L) { L = Female; }, EShinobiLookEdit::Rebuild);
	TestEqual(TEXT("female look kept"), Bench.Page->GetLook().Top, WomenOnly);
	Bench.Page->EditLook([](FShinobiAppearance& L) { L.Sex = EShinobiSex::Male; }, EShinobiLookEdit::Rebuild);
	const FShinobiAppearance& Male = Bench.Page->GetLook();
	TestEqual(TEXT("no female head"), Male.Head, FShinobiAppearance::None);
	TestEqual(TEXT("no women-only top"), Male.Top, FShinobiAppearance::None);
	TestTrue(TEXT("a male face"), Male.Face < Catalog->CountFaces(EShinobiSex::Male));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorDebouncedBlendTest, "ShinobiCoop.UI.CreatorDebouncedBlend", ShinobiTestFlags)
bool FShinobiCreatorDebouncedBlendTest::RunTest(const FString& Parameters)
{
	// A slider that needs a Mutable build rebuilds once it rests, with its last value.
	FCreatorBench Bench;
	UShinobiCreatorPage* Page = Bench.Page;
	Page->TickEdits(0.f);
	Page->EditLook([](FShinobiAppearance& L) { L.BuildBlend = 10; }, EShinobiLookEdit::Debounced);
	Page->TickEdits(0.05f);
	Page->EditLook([](FShinobiAppearance& L) { L.BuildBlend = 20; }, EShinobiLookEdit::Debounced);
	Page->TickEdits(0.1f);
	TestTrue(TEXT("still resting"), Page->IsRedressPending());
	TestNotEqual(TEXT("not rebuilt yet"), Bench.Stage->GetShownLook().BuildBlend, uint8(20));
	Page->TickEdits(0.1f);
	TestFalse(TEXT("rebuilt"), Page->IsRedressPending());
	TestEqual(TEXT("with the last value"), Bench.Stage->GetShownLook().BuildBlend, uint8(20));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorLastPickWinsTest, "ShinobiCoop.UI.CreatorLastPickWins", ShinobiTestFlags)
bool FShinobiCreatorLastPickWinsTest::RunTest(const FString& Parameters)
{
	// Two tops picked on consecutive ticks while the first builds: the second is what the stage ends up wearing.
	TSharedRef<FCreatorBench> Bench = MakeShared<FCreatorBench>();
	Bench->Page->EditLook([](FShinobiAppearance& L) { L.Sex = EShinobiSex::Male; L.Head = FShinobiAppearance::None; L.Top = 0; }, EShinobiLookEdit::Rebuild);
	Bench->Page->TickEdits(0.016f);
	Bench->Page->EditLook([](FShinobiAppearance& L) { L.Top = 1; }, EShinobiLookEdit::Rebuild);
	Bench->Page->TickEdits(0.016f);
	const double Deadline = FPlatformTime::Seconds() + 60.0;
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Bench, Deadline]()
	{
		if (Bench->Stage->GetBody()->IsMutableBuildPending() && FPlatformTime::Seconds() < Deadline)
		{
			return false;
		}
		TestFalse(TEXT("built in time"), Bench->Stage->GetBody()->IsMutableBuildPending());
		TestEqual(TEXT("the second pick shown"), Bench->Stage->GetShownLook().Top, uint8(1));
		TestNotNull(TEXT("a body worn"), Bench->Stage->GetBody()->GetSkeletalMeshAsset());
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorEveryRowFocusableTest, "ShinobiCoop.UI.CreatorEveryRowFocusable", ShinobiTestFlags)
bool FShinobiCreatorEveryRowFocusableTest::RunTest(const FString& Parameters)
{
	// Gamepad: every button and slider of every category takes focus.
	FCreatorBench Bench;
	for (int32 Category = 0; Category < int32(UShinobiCreatorPage::ECategory::Num); ++Category)
	{
		Bench.Page->ShowCategory(UShinobiCreatorPage::ECategory(Category));
		TArray<UWidget*> Widgets;
		CreatorTestWidgets(Bench.Page->GetRows(), Widgets);
		int32 Controls = 0;
		for (const UWidget* Widget : Widgets)
		{
			if (const UButton* Button = Cast<UButton>(Widget))
			{
				++Controls;
				TestTrue(*FString::Printf(TEXT("category %d: button focusable"), Category), Button->GetIsFocusable());
			}
			else if (const USlider* Slider = Cast<USlider>(Widget))
			{
				++Controls;
				TestTrue(*FString::Printf(TEXT("category %d: slider focusable"), Category), Slider->IsFocusable);
			}
		}
		TestTrue(*FString::Printf(TEXT("category %d has controls"), Category), Controls > 0);
	}
	return true;
}

#endif
