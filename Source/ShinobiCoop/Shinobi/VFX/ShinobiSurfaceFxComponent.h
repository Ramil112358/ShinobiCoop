#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ShinobiSurfaceFxComponent.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Chakra on the soles while a player runs on a wall or walks on water: a thin glow shell around each foot (the charge
 * aura's material, M_ChakraBodyAura, in chakra blue) and a faint light. Purely cosmetic and not replicated: every machine
 * reads the replicated movement state (gravity, movement mode), so the other players see it too. Death, downed and
 * mounting hide it at once; otherwise it fades.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiSurfaceFxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiSurfaceFxComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0 = hidden, 1 = fully shown. */
	float GetFade() const { return Fade; }

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.01"))
	float FadeTime = 0.15f;

	/** Shell size around each foot in uu: length along the foot, width, height. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FVector ShellSize = FVector(30.f, 14.f, 14.f);

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightIntensity = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightRadius = 150.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Shells;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Glow;

	float Fade = 0.f;
	float Age = 0.f;
};
