#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "ShinobiFeedbackComponent.generated.h"

class UAnimMontage;
class UTextRenderComponent;
struct FOnAttributeChangeData;

/** A floating damage number: rises, then shrinks away. Local and cosmetic, never replicated. */
UCLASS(NotPlaceable)
class SHINOBICOOP_API AShinobiDamageNumber : public AActor
{
	GENERATED_BODY()

public:
	AShinobiDamageNumber();

	static constexpr float Lifetime = 0.9f;

	/** Big for hits of 20+ (the stagger threshold); red on players, white on enemies. */
	void Init(int32 Amount, bool bOnPlayer);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UTextRenderComponent> Text;

	float Age = 0.f;
	float BaseSize = 1.f;
};

/**
 * Hit feedback on every machine that renders, driven by what already replicates: a drop in the owner's Health spawns
 * a damage number, an impact sound, a short hit-stop (anim freeze only) for 20+, and camera shake for the local
 * player; montage starts play their swing / dodge / cast / fall sound. Created by AShinobiCharacterBase.
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShinobiFeedbackComponent();

	static constexpr float HeavyHit = 20.f;
	static constexpr float HitStopSeconds = 0.08f;
	/** Health changes this soon after BeginPlay are initial replication, not damage. */
	static constexpr float SettleSeconds = 1.f;

protected:
	virtual void BeginPlay() override;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnDamaged(float Amount);

	UFUNCTION()
	void OnMontageStarted(UAnimMontage* Montage);

	void EndHitStop();

	double ReadyTime = 0.0;
	FTimerHandle HitStopTimer;
};
