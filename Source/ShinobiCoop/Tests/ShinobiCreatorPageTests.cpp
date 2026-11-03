#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: the catalog and Mutable content are local)

#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Engine/DataTable.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Shinobi/UI/ShinobiCreatorPage.h"
#include "Shinobi/UI/ShinobiMenuStage.h"
#include "Shinobi/UI/ShinobiDropdown.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "Tests/AutomationCommon.h"

namespace
{
	/** A menu stage and a creator page on it, kept alive across latent waits. */
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
		TestEqual(TEXT("the second pick worn"), Bench->Stage->GetBody()->GetAppearance().Top, uint8(1));
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

namespace
{
	constexpr EShinobiWardrobeSlot CreatorTestSlots[] = { EShinobiWardrobeSlot::Top, EShinobiWardrobeSlot::Bottom, EShinobiWardrobeSlot::Shoes,
		EShinobiWardrobeSlot::Outer, EShinobiWardrobeSlot::Accessory };

	/** Clicks Panel's Index-th child (a UShinobiIndexButton); false if there is none. */
	bool CreatorTestClick(UPanelWidget* Panel, int32 Index)
	{
		UShinobiIndexButton* Button = Panel ? Cast<UShinobiIndexButton>(Panel->GetChildAt(Index)) : nullptr;
		return Button && Button->OnIndexClicked.ExecuteIfBound(Button->Index);
	}

	/** A male look in CLOTHES wearing Top row Row. */
	void CreatorTestWearTop(FCreatorBench& Bench, uint8 Row)
	{
		Bench.Page->EditLook([Row](FShinobiAppearance& L) { L.Sex = EShinobiSex::Male; L.Top = Row; }, EShinobiLookEdit::Rebuild);
		Bench.Page->ShowCategory(UShinobiCreatorPage::ECategory::Clothes);
		Bench.Page->TickEdits(0.f);
	}

	/** The first male Top row that is (or isn't) tinted. */
	uint8 CreatorTestTop(bool bTinted)
	{
		for (const uint8 Row : UShinobiCreatorPage::RowsForSex(EShinobiWardrobeSlot::Top, EShinobiSex::Male))
		{
			const FShinobiWardrobeRow* Piece = UShinobiMutableLook::FindRow(EShinobiWardrobeSlot::Top, Row);
			if (Piece && UShinobiCreatorPage::IsTinted(*Piece, EShinobiSex::Male) == bTinted)
			{
				return Row;
			}
		}
		return FShinobiAppearance::None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorClothesStripsTest, "ShinobiCoop.UI.CreatorClothesStrips", ShinobiTestFlags)
bool FShinobiCreatorClothesStripsTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestWearTop(Bench, 0);
	int32 Shown = 0;
	for (const EShinobiWardrobeSlot Slot : CreatorTestSlots)
	{
		const TArray<uint8> Rows = UShinobiCreatorPage::RowsForSex(Slot, EShinobiSex::Male);
		const UPanelWidget* Strip = Bench.Page->GetClothesStrip(Slot);
		if (Rows.Num() > 0 && TestNotNull(*FString::Printf(TEXT("slot %d strip"), int32(Slot)), Strip))
		{
			TestEqual(*FString::Printf(TEXT("slot %d: None and a button per piece"), int32(Slot)), Strip->GetChildrenCount(), Rows.Num() + 1);
			++Shown;
		}
	}
	TestTrue(TEXT("strips shown"), Shown >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorPickPieceTest, "ShinobiCoop.UI.CreatorPickPiece", ShinobiTestFlags)
bool FShinobiCreatorPickPieceTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestWearTop(Bench, FShinobiAppearance::None);
	const TArray<uint8> Rows = UShinobiCreatorPage::RowsForSex(EShinobiWardrobeSlot::Top, EShinobiSex::Male);
	TestTrue(TEXT("the second button picked"), CreatorTestClick(Bench.Page->GetClothesStrip(EShinobiWardrobeSlot::Top), 1));
	TestEqual(TEXT("the first male top worn"), Bench.Page->GetLook().Top, Rows.Num() > 0 ? Rows[0] : FShinobiAppearance::None);
	TestTrue(TEXT("None first"), CreatorTestClick(Bench.Page->GetClothesStrip(EShinobiWardrobeSlot::Top), 0));
	TestEqual(TEXT("no top"), Bench.Page->GetLook().Top, FShinobiAppearance::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorSwatchesOnlyWhenTintedTest, "ShinobiCoop.UI.CreatorSwatchesOnlyWhenTinted", ShinobiTestFlags)
bool FShinobiCreatorSwatchesOnlyWhenTintedTest::RunTest(const FString& Parameters)
{
	const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const uint8 Tinted = CreatorTestTop(true);
	const uint8 Plain = CreatorTestTop(false);
	if (!TestNotNull(TEXT("DT_Palette"), Palette) || !TestNotEqual(TEXT("a tinted male top"), Tinted, FShinobiAppearance::None)
		|| !TestNotEqual(TEXT("an untinted male top"), Plain, FShinobiAppearance::None))
	{
		return false;
	}
	FCreatorBench Bench;
	CreatorTestWearTop(Bench, Tinted);
	const UPanelWidget* Swatches = Bench.Page->GetSwatches(EShinobiWardrobeSlot::Top);
	TestTrue(TEXT("a swatch per palette colour"), Swatches && Swatches->GetChildrenCount() == Palette->GetRowNames().Num());
	CreatorTestWearTop(Bench, Plain);
	TestNull(TEXT("no swatches for an untinted piece"), Bench.Page->GetSwatches(EShinobiWardrobeSlot::Top));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorSwatchSetsColourTest, "ShinobiCoop.UI.CreatorSwatchSetsColour", ShinobiTestFlags)
bool FShinobiCreatorSwatchSetsColourTest::RunTest(const FString& Parameters)
{
	const uint8 Tinted = CreatorTestTop(true);
	if (!TestNotEqual(TEXT("a tinted male top"), Tinted, FShinobiAppearance::None))
	{
		return false;
	}
	FCreatorBench Bench;
	CreatorTestWearTop(Bench, Tinted);
	TestTrue(TEXT("swatch 3 clicked"), CreatorTestClick(Bench.Page->GetSwatches(EShinobiWardrobeSlot::Top), 3));
	TestEqual(TEXT("the top's colour"), Bench.Page->GetLook().SlotColour[int32(EShinobiWardrobeSlot::Top)], uint8(3));
	return true;
}

namespace
{
	/** The first row (a horizontal box) of the page's rows whose caption reads Caption. */
	UPanelWidget* CreatorTestRow(UShinobiCreatorPage& Page, const FString& Caption)
	{
		TArray<UWidget*> Widgets;
		CreatorTestWidgets(Page.GetRows(), Widgets);
		for (UWidget* Widget : Widgets)
		{
			UPanelWidget* Row = Cast<UPanelWidget>(Widget);
			const UTextBlock* First = Row && Row->GetChildrenCount() > 0 ? Cast<UTextBlock>(Row->GetChildAt(0)) : nullptr;
			if (First && First->GetText().ToString().TrimStartAndEnd() == Caption)
			{
				return Row;
			}
		}
		return nullptr;
	}

	/** The last widget of class T under Root (a row's ">" arrow, its slider). */
	template <typename T>
	T* CreatorTestLast(UWidget* Root)
	{
		TArray<UWidget*> Widgets;
		if (Root)
		{
			CreatorTestWidgets(Root, Widgets);
		}
		for (int32 Index = Widgets.Num() - 1; Index >= 0; --Index)
		{
			if (T* Found = Cast<T>(Widgets[Index]))
			{
				return Found;
			}
		}
		return nullptr;
	}

	/** Clicks the first button labelled Label; false if there is none. */
	bool CreatorTestPress(UShinobiCreatorPage& Page, const FString& Label)
	{
		TArray<UWidget*> Widgets;
		CreatorTestWidgets(Page.GetRows(), Widgets);
		for (UWidget* Widget : Widgets)
		{
			UShinobiIndexButton* Button = Cast<UShinobiIndexButton>(Widget);
			const UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
			if (Text && Text->GetText().ToString() == Label)
			{
				return Button->OnIndexClicked.ExecuteIfBound(Button->Index);
			}
		}
		return false;
	}

	/** Picks the next option of the row captioned Caption (its dropdown: a click, then the next entry of the list; a
	 * two-option row: its ">"), then lets the page rebuild its rows. */
	bool CreatorTestNext(UShinobiCreatorPage& Page, const FString& Caption)
	{
		UShinobiIndexButton* Next = CreatorTestLast<UShinobiIndexButton>(CreatorTestRow(Page, Caption));
		bool bClicked = Next && Next->OnIndexClicked.ExecuteIfBound(Next->Index); // (headless, the dropdown's list can't open)
		if (UShinobiDropdown* Field = Cast<UShinobiDropdown>(Next); Field && Field->GetOptions().Num() > 0)
		{
			Field->Choose((Field->GetSelected() + 1) % Field->GetOptions().Num());
		}
		Page.TickEdits(0.f);
		return bClicked;
	}

	/** Moves the slider of the row captioned Caption to Value. */
	bool CreatorTestSlide(UShinobiCreatorPage& Page, const FString& Caption, float Value)
	{
		UShinobiIndexSlider* Slider = CreatorTestLast<UShinobiIndexSlider>(CreatorTestRow(Page, Caption));
		return Slider && Slider->OnIndexValue.ExecuteIfBound(0, Value);
	}

	void CreatorTestBodySkin(FCreatorBench& Bench)
	{
		Bench.Page->EditLook([](FShinobiAppearance& L) { L = FShinobiAppearance(); }, EShinobiLookEdit::Rebuild);
		Bench.Page->ShowCategory(UShinobiCreatorPage::ECategory::BodySkin);
		Bench.Page->TickEdits(0.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorDropdownRowsTest, "ShinobiCoop.UI.CreatorManyOptionsAreDropdowns", ShinobiTestFlags)
bool FShinobiCreatorDropdownRowsTest::RunTest(const FString& Parameters)
{
	// Build has four options: one pick from a list. Sex has two: it stays "< value >".
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	UShinobiDropdown* Build = Cast<UShinobiDropdown>(CreatorTestLast<UShinobiIndexButton>(CreatorTestRow(*Bench.Page, TEXT("Build"))));
	if (TestNotNull(TEXT("Build is a dropdown"), Build))
	{
		TestEqual(TEXT("every build listed"), Build->GetOptions().Num(), 4);
		Build->Choose(0);
		Bench.Page->TickEdits(0.f);
		TestTrue(TEXT("picking Slim straight from the list"), Bench.Page->GetLook().Build == EShinobiBuild::Under);
	}
	Bench.Page->ShowCategory(UShinobiCreatorPage::ECategory::Character);
	Bench.Page->TickEdits(0.f);
	UShinobiIndexButton* Sex = CreatorTestLast<UShinobiIndexButton>(CreatorTestRow(*Bench.Page, TEXT("Sex")));
	TestTrue(TEXT("Sex keeps its arrows"), Sex && !Sex->IsA<UShinobiDropdown>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorBuildPickersTest, "ShinobiCoop.UI.CreatorBuildPickers", ShinobiTestFlags)
bool FShinobiCreatorBuildPickersTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	Bench.Page->EditLook([](FShinobiAppearance& L) { L.Build = EShinobiBuild::Over; }, EShinobiLookEdit::Rebuild);
	Bench.Page->TickEdits(0.f);
	TestTrue(TEXT("Build stepped"), CreatorTestNext(*Bench.Page, TEXT("Build")));
	TestTrue(TEXT("Heavy, then Own"), Bench.Page->GetLook().Build == EShinobiBuild::Own);
	TestTrue(TEXT("blend slid"), CreatorTestSlide(*Bench.Page, TEXT("Build blend"), 0.5f));
	TestEqual(TEXT("blend 0..1 to 0..255"), Bench.Page->GetLook().BuildBlend, uint8(128));
	TestTrue(TEXT("rebuilds once it rests"), Bench.Page->IsRedressPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorSkinToneInstantTest, "ShinobiCoop.UI.CreatorSkinToneInstant", ShinobiTestFlags)
bool FShinobiCreatorSkinToneInstantTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	TestTrue(TEXT("tone slid"), CreatorTestSlide(*Bench.Page, TEXT("Skin tone"), 0.f));
	TestEqual(TEXT("darkest"), Bench.Page->GetLook().SkinTone, uint8(0));
	TestFalse(TEXT("no rebuild"), Bench.Page->IsRedressPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorTattooAddEditDeleteTest, "ShinobiCoop.UI.CreatorTattooAddEditDelete", ShinobiTestFlags)
bool FShinobiCreatorTattooAddEditDeleteTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	TestTrue(TEXT("added"), CreatorTestPress(*Bench.Page, TEXT("+ ADD TATTOO")));
	Bench.Page->TickEdits(0.f);
	const FShinobiTattoo Fresh{0, 0, 0, 0, 0, 128};
	TestTrue(TEXT("one tattoo, design 0 on region 0 at size 128"), Bench.Page->GetLook().Tattoos.Num() == 1 && Bench.Page->GetLook().Tattoos[0] == Fresh);
	TestTrue(TEXT("region stepped"), CreatorTestNext(*Bench.Page, TEXT("Region")));
	TestEqual(TEXT("next region"), Bench.Page->GetLook().Tattoos.Num() == 1 ? Bench.Page->GetLook().Tattoos[0].Region : uint8(255), uint8(1));
	TestTrue(TEXT("moved"), CreatorTestSlide(*Bench.Page, TEXT("Offset U"), 0.5f));
	TestEqual(TEXT("offset"), Bench.Page->GetLook().Tattoos.Num() == 1 ? int32(Bench.Page->GetLook().Tattoos[0].OffsetU) : 0, 64);
	TestFalse(TEXT("all instant"), Bench.Page->IsRedressPending());
	TestTrue(TEXT("deleted"), CreatorTestPress(*Bench.Page, TEXT("DELETE")));
	TestEqual(TEXT("none left"), Bench.Page->GetLook().Tattoos.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorTattooCapTest, "ShinobiCoop.UI.CreatorTattooCap", ShinobiTestFlags)
bool FShinobiCreatorTattooCapTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	for (int32 Add = 0; Add < 3; ++Add)
	{
		TestTrue(TEXT("added"), CreatorTestPress(*Bench.Page, TEXT("+ ADD TATTOO")));
		Bench.Page->TickEdits(0.f);
	}
	TestEqual(TEXT("three"), Bench.Page->GetLook().Tattoos.Num(), 3);
	TestFalse(TEXT("no fourth"), CreatorTestPress(*Bench.Page, TEXT("+ ADD TATTOO")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorMakeUpAndFrecklesTest, "ShinobiCoop.UI.CreatorMakeUpAndFreckles", ShinobiTestFlags)
bool FShinobiCreatorMakeUpAndFrecklesTest::RunTest(const FString& Parameters)
{
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	TestTrue(TEXT("make-up stepped"), CreatorTestNext(*Bench.Page, TEXT("Make up")));
	TestEqual(TEXT("the first look"), Bench.Page->GetLook().MakeUp, uint8(1));
	TestTrue(TEXT("freckles stepped"), CreatorTestNext(*Bench.Page, TEXT("Freckles")));
	TestEqual(TEXT("the first freckles"), Bench.Page->GetLook().Freckles, uint8(1));
	TestFalse(TEXT("both instant"), Bench.Page->IsRedressPending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorRowsScrollToFocusTest, "ShinobiCoop.UI.CreatorRowsScrollToFocus", ShinobiTestFlags)
bool FShinobiCreatorRowsScrollToFocusTest::RunTest(const FString& Parameters)
{
	// Gamepad: the rows sit straight in a scroll box that follows focus, so moving down past the fold scrolls to them.
	FCreatorBench Bench;
	const UScrollBox* Scroll = Cast<UScrollBox>(Bench.Page->GetRows());
	TestTrue(TEXT("the rows are the scroll box's own children"), Scroll != nullptr);
	TestTrue(TEXT("it scrolls to the focused row"), Scroll && Scroll->GetScrollWhenFocusChanges() != EScrollWhenFocusChanges::NoScroll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCreatorPickKeepsPlaceTest, "ShinobiCoop.UI.CreatorPickKeepsPlace", ShinobiTestFlags)
bool FShinobiCreatorPickKeepsPlaceTest::RunTest(const FString& Parameters)
{
	// A pick rebuilds the rows: focus goes back to the control in the clicked one's place, and strips keep their scroll.
	FCreatorBench Bench;
	CreatorTestBodySkin(Bench);
	TestTrue(TEXT("Build stepped"), CreatorTestNext(*Bench.Page, TEXT("Build")));
	const UWidget* Next = CreatorTestLast<UShinobiIndexButton>(CreatorTestRow(*Bench.Page, TEXT("Build")));
	TestTrue(TEXT("focus back on the Build row's >"), Next && Bench.Page->GetRefocusTarget() == Next);

	CreatorTestWearTop(Bench, CreatorTestTop(true));
	UScrollBox* Strip = Cast<UScrollBox>(Bench.Page->GetClothesStrip(EShinobiWardrobeSlot::Top));
	if (TestNotNull(TEXT("top strip"), Strip))
	{
		Strip->TakeWidget(); // (on screen it is built; headless, the scroll lives on the Slate widget)
		Strip->SetScrollOffset(120.f);
		TestTrue(TEXT("a swatch picked"), CreatorTestClick(Bench.Page->GetSwatches(EShinobiWardrobeSlot::Top), 2));
		Bench.Page->TickEdits(0.f);
		const UScrollBox* Rebuilt = Cast<UScrollBox>(Bench.Page->GetClothesStrip(EShinobiWardrobeSlot::Top));
		TestTrue(TEXT("rebuilt"), Rebuilt && Rebuilt != Strip);
		if (Rebuilt)
		{
			const_cast<UScrollBox*>(Rebuilt)->TakeWidget();
		}
		TestEqual(TEXT("same scroll"), Rebuilt ? Rebuilt->GetScrollOffset() : -1.f, 120.f, 0.5f);
	}
	return true;
}

#endif
