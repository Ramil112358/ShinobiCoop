#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Kismet/GameplayStatics.h"

UShinobiCharacterSave* UShinobiCharacterSave::Load()
{
	UShinobiCharacterSave* Saved = UGameplayStatics::DoesSaveGameExist(SlotName, 0) // (loading a missing slot logs a warning)
		? Cast<UShinobiCharacterSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)) : nullptr;
	if (!Saved)
	{
		return CastChecked<UShinobiCharacterSave>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	}
	// (a hand-edited or older file: the same limits as a client's look)
	Saved->Slots.SetNum(FMath::Min(Saved->Slots.Num(), MaxSlots));
	for (FShinobiCharacterSlot& Slot : Saved->Slots)
	{
		Slot.Look.Sanitize();
	}
	return Saved;
}

bool UShinobiCharacterSave::Save()
{
	return UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}

TArray<int32> UShinobiCharacterSave::GetMadeSlots() const
{
	TArray<int32> Found;
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex].Name.IsEmpty())
		{
			Found.Add(SlotIndex);
		}
	}
	return Found;
}

const FShinobiCharacterSlot* UShinobiCharacterSave::FindSlot(const FShinobiAppearance& Look) const
{
	if (Slots.IsValidIndex(Active) && Slots[Active].Look == Look)
	{
		return &Slots[Active];
	}
	return Slots.FindByPredicate([&Look](const FShinobiCharacterSlot& Slot) { return !Slot.Name.IsEmpty() && Slot.Look == Look; });
}
