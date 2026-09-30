#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiVillager.generated.h"

class UNavigationInvokerComponent;
class UShinobiOverheadTextComponent;

/**
 * An ambient villager (sub-project #3). Walks its route (server, navmesh via its AI controller; straight-line steering
 * where there's no navmesh), idling 2-6 s at each point. Pressing E in front of one shows a one-line bark over its head
 * for BarkSeconds (local, cosmetic). Not an AShinobiCharacterBase, so nothing in combat ever targets it. Spawned by
 * AShinobiWorldGameMode from the level's villager markers.
 */
UCLASS()
class SHINOBICOOP_API AShinobiVillager : public ACharacter
{
	GENERATED_BODY()

public:
	AShinobiVillager(const FObjectInitializer& ObjectInitializer);

	static constexpr float BarkSeconds = 3.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** One of the lines, picked deterministically from Seed. */
	static FText PickBark(int32 Seed);
	static int32 NumBarks();

	/** Local: shows a bark (the next line for this villager) over its head for BarkSeconds. Returns the line. */
	FText Talk();

	UPROPERTY(EditAnywhere, Category = "Villager")
	TArray<FVector> Route;

	/** Which of the look / voice variations this villager uses (its character, first bark); replicated. */
	UPROPERTY(Replicated, EditAnywhere, Category = "Villager")
	int32 Variant = 0;

	/** The villagers' characters (built by Scripts/setup_hero.py at /Game/Shinobi/Characters/<Name>), by Variant modulo
	 * the count: townsfolk first (so the valley's 6th route repeats Goro, not a guard), then the village guards. Never
	 * shared with an enemy type's BodyPool. */
	static constexpr const TCHAR* Bodies[] = { TEXT("Goro"), TEXT("Hana"), TEXT("Mika"), TEXT("Jin"), TEXT("Suzu") };

	/** The visible mesh: the character over the hidden Quinn, or the kimono-tinted Quinn when it isn't imported. */
	USkeletalMeshComponent* GetBodyMesh() const { return Body ? Body.Get() : GetMesh(); }

private:
	void HideBark();

	UPROPERTY(VisibleAnywhere, Category = "Villager")
	TObjectPtr<UShinobiBodyComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Villager")
	TObjectPtr<UShinobiOverheadTextComponent> BarkText;

	UPROPERTY(VisibleAnywhere, Category = "Villager")
	TObjectPtr<UNavigationInvokerComponent> NavInvoker;

	int32 NextPoint = 0;
	float IdleTime = 1.f;
	bool bWalking = false;
	int32 TalkCount = 0;
	FTimerHandle BarkTimer;
};
