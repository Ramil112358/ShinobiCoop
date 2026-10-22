#include "Shinobi/Abilities/ShinobiLoadout.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/Abilities/GA_AlmightyPush.h"
#include "Shinobi/Abilities/GA_Amaterasu.h"
#include "Shinobi/Abilities/GA_Chidori.h"
#include "Shinobi/Abilities/GA_Dragon.h"
#include "Shinobi/Abilities/GA_EarthWall.h"
#include "Shinobi/Abilities/GA_FireMeteor.h"
#include "Shinobi/Abilities/GA_IceMeteor.h"
#include "Shinobi/Abilities/GA_Summons.h"
#include "Shinobi/Abilities/GA_WaterDragon.h"
#include "Shinobi/Abilities/GA_WaterWall.h"
#include "Shinobi/Abilities/GA_Fireball.h"
#include "Shinobi/Abilities/GA_Kamui.h"
#include "Shinobi/Abilities/GA_Rasengan.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/Abilities/GA_Teleport.h"

namespace ShinobiLoadout
{
	namespace
	{
		const TCHAR* ConfigSection = TEXT("/Script/ShinobiCoop.ShinobiLoadout");
		const TCHAR* SlotsKey = TEXT("Slots");
		const TCHAR* EmptySlot = TEXT("None");

		constexpr EShinobiAbilityInputID SlotIDs[NumSlots] = {
			EShinobiAbilityInputID::Jutsu1, EShinobiAbilityInputID::Jutsu2, EShinobiAbilityInputID::Jutsu3, EShinobiAbilityInputID::Jutsu4,
			EShinobiAbilityInputID::Jutsu5, EShinobiAbilityInputID::Jutsu6, EShinobiAbilityInputID::Jutsu7, EShinobiAbilityInputID::Jutsu8,
		};
	}

	EShinobiAbilityInputID SlotInputID(int32 SlotIndex)
	{
		return SlotIndex >= 0 && SlotIndex < NumSlots ? SlotIDs[SlotIndex] : EShinobiAbilityInputID::None;
	}

	int32 SlotIndexOf(EShinobiAbilityInputID ID)
	{
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			if (SlotIDs[Index] == ID)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FSlots Default()
	{
		return {
			UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass(),
			UGA_Teleport::StaticClass(), UGA_Kamui::StaticClass(), UGA_AlmightyPush::StaticClass(), UGA_Susanoo::StaticClass(),
		};
	}

	bool IsValid(const FSlots& Slots, const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known)
	{
		if (Slots.Num() != NumSlots)
		{
			return false;
		}
		TSet<UClass*> Seen;
		for (const TSubclassOf<UShinobiJutsuAbility>& Jutsu : Slots)
		{
			if (!Jutsu)
			{
				continue;
			}
			bool bAlreadySeen = false;
			Seen.Add(Jutsu.Get(), &bAlreadySeen);
			if (bAlreadySeen || !Known.Contains(Jutsu))
			{
				return false;
			}
		}
		return true;
	}

	bool CanChange(const FSlots& Current, const FSlots& Requested, TFunctionRef<bool(int32 SlotIndex)> IsSlotLocked)
	{
		for (int32 Index = 0; Index < NumSlots; ++Index)
		{
			const bool bChanged = !Current.IsValidIndex(Index) || !Requested.IsValidIndex(Index) || Current[Index] != Requested[Index];
			if (bChanged && IsSlotLocked(Index))
			{
				return false;
			}
		}
		return true;
	}

	FSlots Place(const FSlots& Current, TSubclassOf<UShinobiJutsuAbility> Jutsu, int32 SlotIndex)
	{
		FSlots Result = Current;
		if (!Result.IsValidIndex(SlotIndex))
		{
			return Result;
		}
		const int32 From = Jutsu ? Result.Find(Jutsu) : INDEX_NONE;
		if (From != INDEX_NONE)
		{
			Result.Swap(From, SlotIndex);
		}
		else
		{
			Result[SlotIndex] = Jutsu;
		}
		return Result;
	}

	FSlots Swap(const FSlots& Current, int32 A, int32 B)
	{
		FSlots Result = Current;
		if (Result.IsValidIndex(A) && Result.IsValidIndex(B))
		{
			Result.Swap(A, B);
		}
		return Result;
	}

	FSlots Clear(const FSlots& Current, int32 SlotIndex)
	{
		FSlots Result = Current;
		if (Result.IsValidIndex(SlotIndex))
		{
			Result[SlotIndex] = nullptr;
		}
		return Result;
	}

	FSlots WithLegacyPage(const FSlots& Current, int32 Page)
	{
		const TSubclassOf<UShinobiJutsuAbility> Pages[5][4] = {
			{ UGA_Fireball::StaticClass(), UGA_Chidori::StaticClass(), UGA_Rasengan::StaticClass(), UGA_Amaterasu::StaticClass() },
			{ UGA_Teleport::StaticClass(), UGA_Kamui::StaticClass(), UGA_AlmightyPush::StaticClass(), UGA_EarthWall::StaticClass() },
			{ UGA_WaterWall::StaticClass(), UGA_WaterDragon::StaticClass(), UGA_FireMeteor::StaticClass(), UGA_IceMeteor::StaticClass() },
			{ UGA_SummonPusho::StaticClass(), UGA_SummonOwl::StaticClass(), UGA_SummonMisha::StaticClass(), UGA_SummonPabu::StaticClass() },
			{ UGA_SummonDragon::StaticClass(), UGA_SummonCrimsonDragon::StaticClass(), UGA_Susanoo::StaticClass(), nullptr },
		};
		FSlots Slots = Current;
		if (Page < 1 || Page > UE_ARRAY_COUNT(Pages))
		{
			return Slots;
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const TSubclassOf<UShinobiJutsuAbility> Jutsu = Pages[Page - 1][Index];
			Slots = Jutsu ? Place(Slots, Jutsu, Index) : Clear(Slots, Index);
		}
		return Slots;
	}

	EGroup GroupOf(TSubclassOf<UShinobiJutsuAbility> Jutsu)
	{
		const UClass* Class = Jutsu.Get();
		if (!Class)
		{
			return EGroup::Combat;
		}
		for (const UClass* Element : { UGA_EarthWall::StaticClass(), UGA_WaterWall::StaticClass(), UGA_WaterDragon::StaticClass(),
			UGA_FireMeteor::StaticClass(), UGA_IceMeteor::StaticClass() })
		{
			if (Class->IsChildOf(Element))
			{
				return EGroup::Elements;
			}
		}
		if (Class->IsChildOf(UGA_SummonBase::StaticClass()) || Class->IsChildOf(UGA_SummonDragonBase::StaticClass())
			|| Class->IsChildOf(UGA_Susanoo::StaticClass()))
		{
			return EGroup::SummonsAndDragons;
		}
		return EGroup::Combat;
	}

	FText GroupName(EGroup Group)
	{
		switch (Group)
		{
		case EGroup::Elements: return INVTEXT("ELEMENTS");
		case EGroup::SummonsAndDragons: return INVTEXT("SUMMONS & DRAGONS");
		default: return INVTEXT("COMBAT");
		}
	}

	FSlots LoadSaved(const TArray<TSubclassOf<UShinobiJutsuAbility>>& Known)
	{
		TArray<FString> Paths;
		GConfig->GetArray(ConfigSection, SlotsKey, Paths, GGameUserSettingsIni);
		if (Paths.Num() != NumSlots)
		{
			return Default();
		}
		FSlots Slots;
		for (const FString& Path : Paths)
		{
			if (Path.IsEmpty() || Path == EmptySlot)
			{
				Slots.Add(nullptr);
				continue;
			}
			UClass* Class = FindObject<UClass>(nullptr, *Path);
			if (!Class || !Class->IsChildOf(UShinobiJutsuAbility::StaticClass()))
			{
				return Default(); // a renamed or removed jutsu: start over rather than guess
			}
			Slots.Add(Class);
		}
		return IsValid(Slots, Known) ? Slots : Default();
	}

	void Save(const FSlots& Slots)
	{
		TArray<FString> Paths;
		for (const TSubclassOf<UShinobiJutsuAbility>& Jutsu : Slots)
		{
			Paths.Add(Jutsu ? Jutsu->GetPathName() : FString(EmptySlot));
		}
		GConfig->SetArray(ConfigSection, SlotsKey, Paths, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
