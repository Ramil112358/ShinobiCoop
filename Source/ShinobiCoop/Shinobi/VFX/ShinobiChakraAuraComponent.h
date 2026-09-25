#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiChakraAuraComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Chakra-charge aura (Naruto Storm style): two counter-rotating flame shells, a pulsing ground ring and a
 * flickering light. Purely cosmetic and not replicated: it follows the owner's State.ChakraCharging tag,
 * which GAS replicates to every machine (ReplicateActivationOwnedTags), so all players see it.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiChakraAuraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiChakraAuraComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True while the owner has State.ChakraCharging. */
	bool IsCharging() const { return bCharging; }

	/** 0 = hidden, 1 = fully faded in. */
	float GetFade() const { return Fade; }

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FLinearColor Color = FLinearColor(0.15f, 0.55f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float Intensity = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.01"))
	float FadeTime = 0.15f;

	/** Degrees per second; the outer shell spins the other way. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float SpinSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightIntensity = 8000.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OnChargingTagChanged(const FGameplayTag Tag, int32 NewCount);
	UStaticMeshComponent* AddPiece(FName Name, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Offset, const FVector& Scale);
	void UpdatePieces();

	/** 0 inner shell, 1 outer shell, 2 ground ring. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	/** Parallel to Pieces; entries are null when the material asset is missing (e.g. before setup_vfx.py ran). */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PieceMaterials;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle TagEventHandle;
	bool bCharging = false;
	float Fade = 0.f;
	float Age = 0.f;
};
