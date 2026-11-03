#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiCreatorPage.generated.h"

class AShinobiMenuStage;
struct FShinobiWardrobeRow;
class UPanelWidget;
class UCircularThrobber;
class UEditableTextBox;
class UShinobiCharacterSave;
class UShinobiIndexButton;
class UTextBlock;
class UScrollBox;
class UScrollBoxSlot;
class UVerticalBox;

/** How an edit reaches the stage: a Mutable rebuild now or once a slider rests, or at once on the worn body. */
enum class EShinobiLookEdit : uint8
{
	/** A pick: the rows show the new value and the stage rebuilds next tick. */
	Rebuild,
	/** A slider that needs a Mutable build: the stage rebuilds once it rests for DebounceSeconds. */
	Debounced,
	/** Skin tone, make-up, freckles, tattoos: UShinobiBodyComponent::UpdateSkin, no build. */
	Skin,
	/** Body and face sliders: UpdateBody / UpdateFace, no build. */
	Shape,
};

/**
 * The character creator (docs/superpowers/specs/2026-10-29-mutable-customisation-design.md, "CREATE tab UI"), opened from
 * the main menu's SHINOBI tab: its "+" card makes a new character, EDIT on a made one's card changes it (Open). It makes a
 * character on the stage and saves it to one of UShinobiCharacterSave's slots. CHARACTER starts from a roster preset and
 * picks sex and head; CLOTHES, BODY & SKIN and FACE edit the rest. Every row changes the look through EditLook. SAVE & PLAY
 * has the stage film the character's portrait, then stores the slot with it, plays it from the next match and goes back to
 * the SHINOBI strip (OnSaved); BACK goes back unsaved (OnBack). The code box shows the look's preset code; LOAD CODE wears a pasted one.
 */
UCLASS()
class SHINOBICOOP_API UShinobiCreatorPage : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class ECategory : uint8 { Character, Clothes, BodySkin, Face, Num };

	/** How long a Debounced slider rests before the stage rebuilds. */
	static constexpr float DebounceSeconds = 0.15f;

	/** Set by the main menu before the page is built; may stay null (tests). */
	TWeakObjectPtr<AShinobiMenuStage> Stage;
	/** BACK: leave without saving. */
	FSimpleDelegate OnBack;
	/** SAVE & PLAY saved the slot. */
	FSimpleDelegate OnSaved;

	/** Starts editing slot SlotIndex afresh (an empty one starts from the default look, on CHARACTER), re-reading the
	 * save and the outfit; the stage wears it on the next ShowOnStage. */
	void Open(int32 SlotIndex);
	/** Edits slot SlotIndex (an empty one starts from the default look) and shows it on the stage. */
	void EditSlot(int32 SlotIndex);
	int32 GetEditedSlot() const { return EditedSlot; }
	/** Whether the edited slot already holds a made character (EDIT) rather than a new one ("+"). */
	bool IsEditingMade() const;
	/** Starts from Catalog->Presets[PresetIndex] (named after its roster character). */
	void ApplyPreset(int32 PresetIndex);
	void ShowCategory(ECategory Category);
	ECategory GetCategory() const { return Category; }
	/** Saves the edited look to its slot and plays it. Its portrait comes first (the SHINOBI card and the in-game HUD show
	 * it): the stage films the look while the page waits (IsSaving), then FinishSave saves the two and goes back to the
	 * strip. Without a stage, or with the look and its portrait unchanged, it saves at once. */
	void SaveAndPlay();
	/** SAVE & PLAY is waiting for the portrait: the page and the menu's tabs take no input meanwhile. */
	bool IsSaving() const { return bSaving; }
	/** Ends SAVE & PLAY's wait: saves the slot with Portrait (empty: none was had; the strip films it later), plays it and
	 * goes back to the strip (the stage hands its picture here). */
	void FinishSave(TArray<uint8>&& Portrait);
	/** The wait for a portrait gives up after SaveTimeoutSeconds, or once the stage drops the request (NativeTick; tests). */
	void TickSave(float DeltaSeconds);
	static constexpr float SaveTimeoutSeconds = 30.f;
	/** Wears the code box's preset code; false (and a message) when it isn't one. */
	bool LoadCode();
	/** Changes the look; Edit says how the stage follows. A changed sex clears what the new sex doesn't have. */
	void EditLook(TFunctionRef<void(FShinobiAppearance&)> Change, EShinobiLookEdit Edit);
	/** Runs pending rebuilds (NativeTick; tests). */
	void TickEdits(float DeltaSeconds);
	/** A rebuild is queued or waiting for a slider to rest. */
	bool IsRedressPending() const { return bRedressPending || DebounceLeft > 0.f; }

	/** The stage wears the edited look again (entering the tab). */
	void ShowOnStage() { FrameCategory(); Refresh(true); }

	const FShinobiAppearance& GetLook() const { return Look; }
	UWidget* GetFocusTarget() const;
	/** The category's rows (tests). */
	UPanelWidget* GetRows() const;
	/** The control a rows rebuild hands focus back to: the one at the clicked control's place (null when none was). */
	UWidget* GetRefocusTarget() const { return RefocusTarget; }
	const TArray<TObjectPtr<UShinobiIndexButton>>& GetCategoryButtons() const { return CategoryButtons; }

	/** Indices into Catalog.Heads of the heads made for Sex. */
	static TArray<int32> HeadsOfSex(const UShinobiCreatorCatalog& Catalog, EShinobiSex Sex);
	/** Clears what Look's sex doesn't have: a head of the other sex, wardrobe rows without a mesh for it, faces, blend
	 * faces and grooms past its counts. */
	static void ValidateForSex(FShinobiAppearance& Look, const UShinobiCreatorCatalog& Catalog);
	/** The wardrobe table rows (FShinobiAppearance's slot index) of WardrobeSlot that have a mesh for Sex, in table order. */
	static TArray<uint8> RowsForSex(EShinobiWardrobeSlot WardrobeSlot, EShinobiSex Sex);
	/** Whether Row takes a palette colour on Sex's mesh (a tinted material copy). */
	static bool IsTinted(const FShinobiWardrobeRow& Row, EShinobiSex Sex);
	/** CLOTHES: WardrobeSlot's strip of pieces (None first), and the palette swatches under the worn one; null when not shown. */
	UPanelWidget* GetClothesStrip(EShinobiWardrobeSlot WardrobeSlot) const;
	UPanelWidget* GetSwatches(EShinobiWardrobeSlot WardrobeSlot) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	/** One row choosing among Names (Current: the shown one): a dropdown for more than two, else "< value >" arrows; a pick
	 * or step calls Pick with the new position. */
	void AddChoice(const FString& Label, const TArray<FString>& Names, int32 Current, TFunction<void(int32)> Pick);
	/** AddChoice over a look field: 0..Count-1, and None first when bNone, named by Name (else 1, 2, ...); a Rebuild edit. */
	void AddPicker(const FString& Label, uint8 FShinobiAppearance::* Field, int32 Count, bool bNone, TFunction<FString(int32)> Name = nullptr);
	/** One slider row (-1..1, or 0..1 when bUnit) starting at Value, over Background if given; Set takes each new value. */
	void AddSlider(const FString& Label, float Value, bool bUnit, TFunction<void(float)> Set, UWidget* Background = nullptr);
	/** A strip of image buttons (Pictures; a null one shows its name), Picked's marked; a click calls Pick with its position. */
	UPanelWidget* AddStrip(const TArray<UTexture2D*>& Pictures, const TArray<FString>& Names, int32 Picked, float Size, TFunction<void(int32)> Pick);
	/** Adds a row widget to the rows. */
	UScrollBoxSlot* AddRow(UWidget* Row);
	/** The rows' buttons and sliders, depth first (a control's place survives a rebuild of the same category). */
	TArray<UWidget*> RowControls() const;
	/** A caption line in the rows. */
	void AddNote(const FString& Text);
	/** A button for the rows (and the bars above them). */
	UShinobiIndexButton* MakeButton(const FString& Label, int32 Index, TFunction<void(int32)> Handler, int32 Size = 14);
	/** Rebuilds the rows, re-dresses the stage when bRedress, refreshes slot, name and code text. */
	void Refresh(bool bRedress);
	/** The stage wears the look. */
	void Redress();
	/** The stage's framing for the category (the face close-up on FACE). */
	void FrameCategory();
	void BuildCharacterRows();
	void BuildClothesRows();
	void BuildBodySkinRows();
	void BuildFaceRows();

	UPROPERTY()
	TObjectPtr<UShinobiCharacterSave> Save;
	UPROPERTY()
	TObjectPtr<UShinobiCreatorCatalog> Catalog;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> CategoryButtons;
	/** NEW SHINOBI or EDIT SHINOBI, over the categories. */
	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;
	/** The rows, straight in the scroll box, which follows focus down past the fold. */
	UPROPERTY()
	TObjectPtr<UScrollBox> Rows;
	UPROPERTY()
	TObjectPtr<UWidget> RefocusTarget;
	UPROPERTY()
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY()
	TObjectPtr<UEditableTextBox> CodeBox;
	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;
	/** On the stage while a Mutable build is pending (the previous look stays visible). */
	UPROPERTY()
	TObjectPtr<UCircularThrobber> Spinner;
	/** CLOTHES, per wardrobe slot (null when not shown). */
	UPROPERTY()
	TArray<TObjectPtr<UPanelWidget>> ClothesStrips;
	UPROPERTY()
	TArray<TObjectPtr<UPanelWidget>> Swatches;

	FShinobiAppearance Look;
	/** Outfits index worn over the look (the saved outfit pick, shared with the roster). */
	uint8 Cloak = 0;
	int32 EditedSlot = 0;
	ECategory Category = ECategory::Character;
	/** A pick was made: the rows rebuild (and the stage re-dresses) next tick, not inside its own click. */
	bool bRedressPending = false;
	/** An instant edit moved a selection mark: the rows rebuild next tick. */
	bool bRowsPending = false;
	/** The place among RowControls of the control last clicked (INDEX_NONE: none), refocused after the rows rebuild. */
	int32 ClickedPlace = INDEX_NONE;
	/** The category the rows were last built for (strips keep their scroll only within it). */
	ECategory BuiltCategory = ECategory::Num;
	/** A Debounced slider moved: the stage re-dresses when this runs out. */
	float DebounceLeft = 0.f;
	/** SAVE & PLAY waits for the portrait (SaveWaited seconds so far). */
	bool bSaving = false;
	float SaveWaited = 0.f;
	/** The page's inputs, disabled while it saves. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> SaveLocked;
};
