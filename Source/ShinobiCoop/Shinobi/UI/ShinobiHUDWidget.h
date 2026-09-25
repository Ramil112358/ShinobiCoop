#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShinobiHUDWidget.generated.h"

class UProgressBar;
class UVerticalBox;

/** Health / chakra / stamina bars. Widget tree is built in code; values are polled each tick. */
UCLASS()
class SHINOBICOOP_API UShinobiHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UProgressBar* AddBar(UVerticalBox* Box, FName Name, const FLinearColor& Color, float Height);

	UPROPERTY()
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> ChakraBar;

	UPROPERTY()
	TObjectPtr<UProgressBar> StaminaBar;
};
