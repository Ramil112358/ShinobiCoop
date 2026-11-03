#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shinobi/UI/ShinobiMenuWidgets.h"
#include "ShinobiRosterPage.generated.h"

class AShinobiMenuStage;
struct FShinobiAppearance;
class UShinobiIndexButton;
class UTextBlock;
class UTexture2D;

/**
 * The main menu's SHINOBI tab: along the bottom a portrait card per roster character, then a card per character made in
 * the creator (UShinobiCharacterSave's slots; its portrait once the stage has filmed it, AShinobiMenuStage::RequestPortrait)
 * and a "+" card that makes a new one (hidden once every slot is used). On the right: the pick's name, a face stepper
 * (roster characters with several), an outfit dropdown, and for a made character EDIT (back into the creator) and DELETE
 * (press twice). Every change is saved (ShinobiSettings, UShinobiCharacterSave) and worn by the stage at once.
 */
UCLASS()
class SHINOBICOOP_API UShinobiRosterPage : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set by the main menu before the page is built; may stay null (tests, a level without a stage). */
	TWeakObjectPtr<AShinobiMenuStage> Stage;
	/** "+" (a free slot) or EDIT (the made character's slot): open the creator on that slot. */
	FShinobiIndexClicked OnOpenCreator;

	/** Plays Roster[RosterIndex] from now on. */
	void Pick(int32 RosterIndex);
	/** Plays the made character in save slot SlotIndex from now on. */
	void PickMade(int32 SlotIndex);
	/** Deletes the made character in save slot SlotIndex (the roster pick is played if it was). */
	void DeleteMade(int32 SlotIndex);
	/** The first save slot with no made character in it, or INDEX_NONE when all are used. */
	static int32 FindFreeSlot();
	/** The save slots holding a made character, in order (one card each). */
	static TArray<int32> MadeSlots();
	/** The "+" card (null while hidden). */
	UShinobiIndexButton* GetCreateCard() const;
	/** The card of the made character in save slot SlotIndex, or null. */
	UShinobiIndexButton* GetMadeCard(int32 SlotIndex) const;
	/** The EDIT and DELETE buttons, shown while a made character is picked. */
	UShinobiIndexButton* GetEditButton() const { return EditButton; }
	UShinobiIndexButton* GetDeleteButton() const { return DeleteButton; }
	/** The next (+1) or previous (-1) outfit, wrapping (LT / RT). */
	void StepOutfit(int32 Direction);
	/** Wears Outfits[OutfitIndex] from now on (the OUTFIT dropdown). */
	void PickOutfit(int32 OutfitIndex);
	/** The OUTFIT dropdown. */
	class UShinobiDropdown* GetOutfitField() const { return OutfitField; }
	/** The picked character's next (+1) or previous (-1) face, wrapping (a character with one face keeps it). */
	void StepFace(int32 Direction);
	/** The picked character's card (a made one's when it is played). */
	UWidget* GetFocusTarget() const;
	/** Card looks, name and outfit line from the saved picks; the stage wears them (unless !bWear: only the page). */
	void Show(bool bWear = true);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	/** A card for the strip: Picture (or a dark card with Initial on it) over Label. */
	UShinobiIndexButton* MakeCard(UTexture2D* Picture, const FString& Initial, int32 InitialSize, const FString& Label, int32 Index, FShinobiIndexClicked Handler, UTextBlock*& OutLabel);
	/** Rebuilds the made cards and "+" when the saved characters (or their portraits) changed. */
	void RebuildMadeCards();
	/** Has the stage film the made character wearing Look, and keeps the picture as its portrait (its card shows it). */
	void RequestMadePortrait(AShinobiMenuStage& Shown, const FShinobiAppearance& Look);

	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> Cards;
	/** Where the made cards and "+" go, after the roster cards. */
	UPROPERTY()
	TObjectPtr<class UHorizontalBox> MadeStrip;
	/** One per MadeSlots() entry, in that order. */
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> MadeCards;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> MadeCardNames;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> CreateCard;
	UPROPERTY()
	TObjectPtr<UTextBlock> CreateCardName;
	/** The made cards' names when they were last built. */
	TArray<FString> BuiltMadeNames;
	/** Shown while a made character is picked. */
	UPROPERTY()
	TObjectPtr<UWidget> MadeButtons;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> EditButton;
	UPROPERTY()
	TObjectPtr<UShinobiIndexButton> DeleteButton;
	/** DELETE was pressed once: the next press deletes. */
	bool bDeleteArmed = false;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CardNames;
	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY()
	TObjectPtr<class UShinobiDropdown> OutfitField;
	/** The FACE label and row: shown for a character with several faces. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> FaceRow;
	UPROPERTY()
	TObjectPtr<UTextBlock> FaceText;
};
