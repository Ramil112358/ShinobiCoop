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
