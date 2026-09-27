#include "Shinobi/UI/ShinobiOverheadTextComponent.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

UShinobiOverheadTextComponent::UShinobiOverheadTextComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetHorizontalAlignment(EHTA_Center);
	SetWorldSize(28.f);
	SetTextRenderColor(FColor::Red);
	SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetText(FText::GetEmpty()); // not the widget default "Text" before the first tick
}

void UShinobiOverheadTextComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Refresh();

	// TextRender geometry faces its local +X, so it's mirrored/culled from behind: turn it (yaw only) to the local
	// camera. GetFirstPlayerController() would also return remote clients' PCs on a listen server, so use the game
	// instance's local-player lookup, which is null on a dedicated server and in tests.
	const UGameInstance* GI = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	const APlayerController* PC = GI ? GI->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (const APlayerCameraManager* CameraManager = PC ? PC->PlayerCameraManager : nullptr)
	{
		SetWorldRotation(FRotator(0.f, (CameraManager->GetCameraLocation() - GetComponentLocation()).Rotation().Yaw, 0.f));
	}
}

void UShinobiOverheadTextComponent::SetNameMode(const FString& InName)
{
	if (bNameMode && InName == Name)
	{
		return;
	}
	bNameMode = true;
	Name = InName;
	SetTextRenderColor(FColor(170, 230, 255));
	SetText(FText::FromString(Name));
}

void UShinobiOverheadTextComponent::Refresh()
{
	if (bNameMode)
	{
		return;
	}
	const AShinobiCharacterBase* Shinobi = Cast<AShinobiCharacterBase>(GetOwner());
	const UShinobiAttributeSet* Stats = Shinobi ? Shinobi->GetAttributes() : nullptr;
	const UAbilitySystemComponent* ASC = Shinobi ? Shinobi->GetAbilitySystemComponent() : nullptr;
	if (!Stats || !ASC)
	{
		return;
	}
	const int32 CurrentHealth = FMath::RoundToInt(Stats->GetHealth());
	const int32 CurrentMaxHealth = FMath::RoundToInt(Stats->GetMaxHealth());
	FString Statuses;
	if (ASC->HasMatchingGameplayTag(ShinobiTags::State_Burning_Normal))
	{
		Statuses += TEXT(" [Burn]");
	}
	if (ASC->HasMatchingGameplayTag(ShinobiTags::State_Burning_Black))
	{
		Statuses += TEXT(" [Black Flame]");
	}
	if (ASC->HasMatchingGameplayTag(ShinobiTags::State_Shocked))
	{
		Statuses += TEXT(" [Shock]");
	}
	if (ASC->HasMatchingGameplayTag(ShinobiTags::State_ShockBonus))
	{
		Statuses += TEXT(" [+25%]");
	}
	if (CurrentHealth != DisplayedHealth || CurrentMaxHealth != DisplayedMaxHealth || Statuses != DisplayedStatuses)
	{
		DisplayedHealth = CurrentHealth;
		DisplayedMaxHealth = CurrentMaxHealth;
		DisplayedStatuses = Statuses;
		SetText(FText::FromString(FString::Printf(TEXT("HP %d / %d%s"), DisplayedHealth, DisplayedMaxHealth, *DisplayedStatuses)));
	}
}
