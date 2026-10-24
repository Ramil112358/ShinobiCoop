#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiVillager.generated.h"

class UAnimSequence;
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
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** One of the lines (the valley's, or London's), picked deterministically from Seed. */
	static FText PickBark(int32 Seed, bool bLondonLines = false);
	static int32 NumBarks(bool bLondonLines = false);

	/** Local: shows a bark (the next line for this villager) over its head for BarkSeconds. Returns the line. */
	FText Talk();

	UPROPERTY(EditAnywhere, Category = "Villager")
	TArray<FVector> Route;

	/** Which of the look / voice variations this villager uses (its character, first bark); replicated. */
	UPROPERTY(Replicated, EditAnywhere, Category = "Villager")
	int32 Variant = 0;

	/** A Londoner (marker tag Barks:London): speaks London's lines instead of the valley's; replicated. */
	UPROPERTY(Replicated, EditAnywhere, Category = "Villager")
	bool bLondon = false;

	/** A city pedestrian (marker tag Look:Crowd): wears a City Sample crowd character (CrowdLookClass) instead of the
	 * villager body, its gender, build, face, outfit and hair picked from Variant, so every machine dresses it alike. */
	UPROPERTY(Replicated, EditAnywhere, Category = "Villager")
	bool bCrowd = false;

	/** Crowd characters draw only this near a camera (cm); their hair (grooms) nearer still. 120 m: a street's length, so
	 * the half of London's crowd that walks near the hub (Scripts/london/cluster_crowd.py) shows from the spawn. */
	static constexpr float CrowdDrawDistance = 12000.f;
	static constexpr float CrowdHairDrawDistance = 2000.f;

	static constexpr const TCHAR* CrowdLookClass = TEXT("/Game/CitySampleCrowd/Blueprints/BP_CrowdCharacter.BP_CrowdCharacter_C");

	/** Copies the hidden Quinn's AnimGen walk onto a crowd body (Scripts/anim/setup_crowd_retarget.py). */
	static constexpr const TCHAR* CrowdRetargeter = TEXT("/Game/Shinobi/AnimGen/RTG_MannyToCrowd.RTG_MannyToCrowd");

	/** The crowd character actor while bCrowd (local, cosmetic: never replicated). */
	AActor* GetCrowdLook() const { return CrowdLook; }

	/** The villagers' characters (built by Scripts/setup_hero.py at /Game/Shinobi/Characters/<Name>), by Variant modulo
	 * the count: townsfolk first (so the valley's 6th route repeats Goro, not a guard), then the village guards. Never
	 * shared with an enemy type's BodyPool. */
	static constexpr const TCHAR* Bodies[] = { TEXT("Goro"), TEXT("Hana"), TEXT("Mika"), TEXT("Jin"), TEXT("Suzu") };

	/** The visible mesh: the character over the hidden Quinn, or the kimono-tinted Quinn when it isn't imported. */
	USkeletalMeshComponent* GetBodyMesh() const { return Body ? Body.Get() : GetMesh(); }

private:
	void HideBark();
	/** AnimGen: the AnimGen AnimBP for the hidden Quinn to walk with (the body retargets from it), else null for City
	 * Sample's own idle and walk loops. */
	bool WearCrowdLook(UClass* AnimGen);

	UPROPERTY(Transient)
	TObjectPtr<AActor> CrowdLook;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CrowdRoot;

	/** City Sample's loops, when not walking with AnimGen (null when it is). */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrowdIdle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CrowdWalk;

	bool bCrowdWalking = false;

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
