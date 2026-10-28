#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiRosterPage.generated.h"

class AShinobiMenuStage;
class UShinobiIndexButton;
class UTextBlock;

/**
 * The main menu's SHINOBI tab: a portrait card per roster character along the bottom, the pick's name and an outfit
 * selector on the right. Every change is saved (ShinobiSettings) and worn by the stage at once.
 */
UCLASS()
class SHINOBICOOP_API UShinobiRosterPage : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set by the main menu before the page is built; may stay null (tests, a level without a stage). */
	TWeakObjectPtr<AShinobiMenuStage> Stage;

	/** Plays Roster[RosterIndex] from now on. */
	void Pick(int32 RosterIndex);
	/** The next (+1) or previous (-1) outfit, wrapping. */
	void StepOutfit(int32 Direction);
	/** The picked character's next (+1) or previous (-1) face, wrapping (a character with one face keeps it). */
	void StepFace(int32 Direction);
	/** The picked character's card. */
	UWidget* GetFocusTarget() const;
	/** Card looks, name and outfit line from the saved picks; the stage wears them. */
	void Show();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UShinobiIndexButton>> Cards;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> CardNames;
	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY()
	TObjectPtr<UTextBlock> OutfitText;
	/** The FACE label and row: shown for a character with several faces. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> FaceRow;
	UPROPERTY()
	TObjectPtr<UTextBlock> FaceText;
};
