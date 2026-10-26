#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiCreatorPage.generated.h"

class AShinobiMenuStage;
class UEditableTextBox;
class UShinobiCharacterSave;
class UShinobiIndexButton;
class UTextBlock;
class UVerticalBox;

/**
 * The main menu's CREATE tab: make a character on the stage and save it to one of UShinobiCharacterSave's slots. Catalog
 * picks (frame, face, hair, clothes) re-dress the stage; shape sliders edit the worn look live. SAVE & PLAY stores the
 * slot and plays it from the next match (a roster pick on the SHINOBI tab switches back). The code box shows the look's
 * preset code; LOAD CODE wears a pasted one.
 */
UCLASS()
class SHINOBICOOP_API UShinobiCreatorPage : public UUserWidget
{
	GENERATED_BODY()

public:
	enum class ECategory : uint8 { Body, Face, Hair, Clothes, Num };

	/** Set by the main menu before the page is built; may stay null (tests). */
	TWeakObjectPtr<AShinobiMenuStage> Stage;

	/** Edits slot SlotIndex (an empty one starts from the default look) and shows it on the stage. */
	void EditSlot(int32 SlotIndex);
	void ShowCategory(ECategory Category);
	/** Saves the edited look to its slot and plays it. */
	void SaveAndPlay();
	/** Wears the code box's preset code; false (and a message) when it isn't one. */
	bool LoadCode();

	/** The stage wears the edited look again (entering the tab). */
	void ShowOnStage() { Refresh(true); }

	const FShinobiAppearance& GetLook() const { return Look; }
	UWidget* GetFocusTarget() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	/** One "< value >" row stepping Field through 0..Count-1 (and None first when bNone), named by Name (else 1, 2, ...);
	 * a step re-dresses the stage. */
	void AddPicker(const FString& Label, uint8& Field, int32 Count, bool bNone, TFunction<FString(int32)> Name = nullptr);
	/** One shape slider row (-1..1, or 0..1 when bUnit) starting at Value; Set takes each new value. */
	void AddSlider(const FString& Label, float Value, bool bUnit, TFunction<void(float)> Set);
	/** Rebuilds the rows, re-dresses the stage when bRedress, refreshes slot, name and code text. */
	void Refresh(bool bRedress);
	/** A pick changed the sex: indices that the other sex doesn't have start over. */
	void ResetForSex();

	UPROPERTY()
	TObjectPtr<UShinobiCharacterSave> Save;
	UPROPERTY()
	TObjectPtr<UShinobiCreatorCatalog> Catalog;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> SlotButtons;
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> CategoryButtons;
	UPROPERTY()
	TObjectPtr<UVerticalBox> Rows;
	UPROPERTY()
	TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY()
	TObjectPtr<UEditableTextBox> CodeBox;
	UPROPERTY()
	TObjectPtr<UTextBlock> MessageText;

	FShinobiAppearance Look;
	/** Outfits index worn over the look (the saved outfit pick, shared with the roster). */
	uint8 Cloak = 0;
	int32 EditedSlot = 0;
	ECategory Category = ECategory::Body;
	/** A picker was clicked: its row is rebuilt (and the stage re-dressed) next tick, not inside its own click. */
	bool bRedressPending = false;
};
