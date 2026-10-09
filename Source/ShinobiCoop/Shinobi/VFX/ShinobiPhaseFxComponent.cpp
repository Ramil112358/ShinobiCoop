#include "Shinobi/VFX/ShinobiPhaseFxComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Materials/MaterialInterface.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

void UShinobiPhaseFxComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		PhasedHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Phased).AddUObject(this, &UShinobiPhaseFxComponent::OnPhasedChanged);
	}
}

void UShinobiPhaseFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(PhasedHandle, ShinobiTags::State_Phased);
	}
	Super::EndPlay(EndPlayReason);
}

void UShinobiPhaseFxComponent::OnPhasedChanged(const FGameplayTag Tag, int32 Count)
{
	if (Count > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(GhostTimer, this, &UShinobiPhaseFxComponent::ApplyGhost, GhostDelay);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(GhostTimer); // stepped out before the spiral finished
		RestoreBody();
	}
}

void UShinobiPhaseFxComponent::ApplyGhost()
{
	if (Saved.Num() > 0)
	{
		return; // already a ghost
	}
	UMaterialInstanceDynamic* Ghost = ShinobiVfx::MakePhased(this, GhostOpacity);
	if (!Ghost)
	{
		return;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	GetOwner()->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh->IsVisible() || !Mesh->GetSkeletalMeshAsset())
		{
			continue; // the hidden Manny that drives a hero, or an outfit slot with nothing worn
		}
		FSaved& Entry = Saved.AddDefaulted_GetRef();
		Entry.Mesh = Mesh;
		Entry.bCustomDepth = Mesh->bRenderCustomDepth;
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Entry.Materials.Add(Mesh->GetMaterial(Slot));
			Mesh->SetMaterial(Slot, Ghost);
		}
		Mesh->SetRenderCustomDepth(false);
	}
}

void UShinobiPhaseFxComponent::RestoreBody()
{
	for (const FSaved& Entry : Saved)
	{
		if (USkeletalMeshComponent* Mesh = Entry.Mesh.Get())
		{
			for (int32 Slot = 0; Slot < Entry.Materials.Num(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Entry.Materials[Slot].Get());
			}
			Mesh->SetRenderCustomDepth(Entry.bCustomDepth);
		}
	}
	Saved.Reset();
}
