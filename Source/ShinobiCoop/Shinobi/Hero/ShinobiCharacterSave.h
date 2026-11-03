#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiCharacterSave.generated.h"

/** One made character: its name, look and portrait. */
USTRUCT()
struct FShinobiCharacterSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FShinobiAppearance Look;

	/** Its head and shoulders as a PNG (3:4, like the roster's portraits), filmed by the main menu's stage
	 * (AShinobiMenuStage::RequestPortrait) the first time it stands there wearing Look; empty until then, and again
	 * whenever Look changes. The SHINOBI tab's card and the in-game HUD show it. */
	UPROPERTY()
	TArray<uint8> Portrait;
};

/**
 * The characters this machine's player made in the creator (local, per machine like the settings): up to MaxSlots, and
 * which one they play (Active; INDEX_NONE plays the roster pick, ShinobiSettings::GetSavedCharacter).
 */
UCLASS()
class SHINOBICOOP_API UShinobiCharacterSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxSlots = 4;
	static constexpr const TCHAR* SlotName = TEXT("ShinobiCharacters");

	UPROPERTY()
	TArray<FShinobiCharacterSlot> Slots;

	UPROPERTY()
	int32 Active = INDEX_NONE;

	/** The saved characters, or an empty save when there are none yet. */
	static UShinobiCharacterSave* Load();
	bool Save();

	/** The played slot's look, or null when the roster pick is played. */
	const FShinobiAppearance* GetActiveLook() const { return Slots.IsValidIndex(Active) ? &Slots[Active].Look : nullptr; }

	/** The slots holding a made character (a name), in order. */
	TArray<int32> GetMadeSlots() const;

	/** The made character wearing Look: the played one if it does, else the first; null when none does. */
	const FShinobiCharacterSlot* FindSlot(const FShinobiAppearance& Look) const;
};
