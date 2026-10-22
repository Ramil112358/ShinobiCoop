#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"

class UShinobiJutsuAbility;

/**
 * The player's jutsu loadout: which jutsu sits in each of the 8 bar slots (Skills screen, K). Slot i fires on
 * ASC input SlotInputID(i). Pure data and rules here; AShinobiPlayerCharacter holds the live copy and the server
 * decides. Saved per machine in GameUserSettings.ini, like ShinobiSettings.
 */
namespace ShinobiLoadout
{
	constexpr int32 NumSlots = 8;

	/** Always NumSlots long; null = an empty slot. */
	using FSlots = TArray<TSubclassOf<UShinobiJutsuAbility>>;

	/** 0 -> Jutsu1 ... 7 -> Jutsu8; out of range -> None. */
	SHINOBICOOP_API EShinobiAbilityInputID SlotInputID(int32 SlotIndex);
	/** The inverse of SlotInputID; INDEX_NONE for any other input. */
	SHINOBICOOP_API int32 SlotIndexOf(EShinobiAbilityInputID ID);

	/** Fireball, Chidori, Rasengan, Amaterasu, Teleport, Kamui, Almighty Push, Susanoo. */
	SHINOBICOOP_API FSlots Default();

	/** NumSlots long, every entry null or in Known, no jutsu twice. */
	SHINOBICOOP_API bool IsValid(const FSlots& Slots, const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known);

	/** True when nothing changes, or no changed slot is locked. Whole-loadout locks (combat, casting) are the caller's. */
	SHINOBICOOP_API bool CanChange(const FSlots& Current, const FSlots& Requested, TFunctionRef<bool(int32 SlotIndex)> IsSlotLocked);

	/** Jutsu into SlotIndex. Already in another slot: the two slots swap. Otherwise the jutsu it replaces leaves the loadout. */
	SHINOBICOOP_API FSlots Place(const FSlots& Current, TSubclassOf<UShinobiJutsuAbility> Jutsu, int32 SlotIndex);
	SHINOBICOOP_API FSlots Swap(const FSlots& Current, int32 A, int32 B);
	SHINOBICOOP_API FSlots Clear(const FSlots& Current, int32 SlotIndex);

	/** Debug and tests: one of the five fixed jutsu pages the game had before the loadout (1 basics, 2 movement, 3 elements,
	 * 4 summons, 5 dragons + Susanoo) put into slots 1-4 of Current, so old "page N, key K" scripts mean the same jutsu. */
	SHINOBICOOP_API FSlots WithLegacyPage(const FSlots& Current, int32 Page);

	/** The Skills screen's headings for the jutsu pool. */
	enum class EGroup : uint8 { Combat, Elements, SummonsAndDragons };
	SHINOBICOOP_API EGroup GroupOf(TSubclassOf<UShinobiJutsuAbility> Jutsu);
	/** "COMBAT", "ELEMENTS", "SUMMONS & DRAGONS". */
	SHINOBICOOP_API FText GroupName(EGroup Group);

	/** The saved loadout, or Default() when there is none or it isn't valid against Known (a renamed jutsu, an old save). */
	SHINOBICOOP_API FSlots LoadSaved(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known);
	SHINOBICOOP_API void Save(const FSlots& Slots);
}
