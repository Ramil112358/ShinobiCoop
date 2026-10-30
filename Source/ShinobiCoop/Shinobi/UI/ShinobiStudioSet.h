#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiStudioSet.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class URectLightComponent;
class UStaticMeshComponent;

/** The CREATE tab's light presets: they retune the same three lights and the backdrop's tint. */
UENUM()
enum class EShinobiStudioLight : uint8
{
	Studio,
	Day,
	Dusk,
};

/**
 * The CREATE tab's photo studio (Lvl_MainMenu; Scripts/setup_menu.py places it high above the rooftop): a seamless sphere
 * backdrop that also shuts out the rooftop's sun and sky, a dark floor with a faint orange ring, a key light front-left and
 * above, a fill from the right at 35 % and a cool rim from behind, and a post process with fixed exposure. The menu stage
 * moves here while CREATE is open (AShinobiMenuStage::SetStudio). Faces +X like the stage: the camera looks back along -X.
 */
UCLASS()
class SHINOBICOOP_API AShinobiStudioSet : public AActor
{
	GENERATED_BODY()

public:
	AShinobiStudioSet();

	/** How far above the stage Scripts/setup_menu.py puts the set (the lights' reach is far shorter). */
	static constexpr float HeightAboveStageCm = 20000.f;

	void ApplyPreset(EShinobiStudioLight Preset);
	EShinobiStudioLight GetPreset() const { return Preset; }

	URectLightComponent* GetKeyLight() const { return KeyLight; }
	URectLightComponent* GetFillLight() const { return FillLight; }
	URectLightComponent* GetRimLight() const { return RimLight; }
	UStaticMeshComponent* GetBackdrop() const { return Backdrop; }

protected:
	virtual void PostInitializeComponents() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Backdrop;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Floor;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URectLightComponent> KeyLight;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URectLightComponent> FillLight;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URectLightComponent> RimLight;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> PostProcessBounds;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> PostProcess;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackdropMaterial;

	EShinobiStudioLight Preset = EShinobiStudioLight::Studio;
};
