#include "Shinobi/UI/ShinobiStudioSet.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ShinobiCoop.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Scripts/setup_menu.py builds them.
	const TCHAR* const StudioBackdropMaterial = TEXT("/Game/Shinobi/UI/Studio/M_StudioBackdrop.M_StudioBackdrop");
	const TCHAR* const StudioFloorMaterial = TEXT("/Game/Shinobi/UI/Studio/M_StudioFloor.M_StudioFloor");
	const TCHAR* const StudioRingMaterial = TEXT("/Game/Shinobi/UI/Studio/M_StudioRing.M_StudioRing");

	struct FStudioPreset
	{
		FLinearColor Key;
		float KeyIntensity;
		FLinearColor Rim;
		float RimIntensity;
		FLinearColor BackdropTint;
	};

	// Indexed by EShinobiStudioLight; candelas (~8 lux on the character from the key, near the rooftop sun's 6 lux under the
	// same exposure). The fill is always the key's colour at 35 %.
	const FStudioPreset StudioPresets[] = {
		{ FLinearColor(1.f, 1.f, 1.f), 120.f, FLinearColor(0.75f, 0.85f, 1.f), 90.f, FLinearColor(1.f, 1.f, 1.f) },
		{ FLinearColor(1.f, 0.95f, 0.85f), 140.f, FLinearColor(0.9f, 0.95f, 1.f), 70.f, FLinearColor(1.7f, 1.65f, 1.6f) },
		{ FLinearColor(1.f, 0.6f, 0.35f), 110.f, FLinearColor(0.55f, 0.5f, 1.f), 110.f, FLinearColor(0.75f, 0.55f, 0.8f) },
	};
	constexpr float StudioFillShare = 0.35f;

	URectLightComponent* MakeStudioLight(AActor& Owner, USceneComponent* Root, const TCHAR* Name, const FVector& Location, const FVector& Aim)
	{
		URectLightComponent* Light = Owner.CreateDefaultSubobject<URectLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetRelativeLocationAndRotation(Location, (Aim - Location).Rotation());
		Light->SetAttenuationRadius(1500.f);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetSourceWidth(120.f);
		Light->SetSourceHeight(120.f);
		return Light;
	}

	UStaticMeshComponent* MakeStudioMesh(AActor& Owner, USceneComponent* Root, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Component = Owner.CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(Root);
		Component->SetStaticMesh(Mesh);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeScale3D(Scale);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Component;
	}
}

AShinobiStudioSet::AShinobiStudioSet()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));

	// A 10 m sphere seen from inside (a two-sided unlit material): the seamless backdrop, and shut, so the rooftop's sun
	// and sky stay out without lighting channels (which Lumen's bounce light ignores).
	Backdrop = MakeStudioMesh(*this, Root, TEXT("Backdrop"), Sphere.Object, FVector::ZeroVector, FVector(20.f));
	Floor = MakeStudioMesh(*this, Root, TEXT("Floor"), Plane.Object, FVector::ZeroVector, FVector(20.f));
	Ring = MakeStudioMesh(*this, Root, TEXT("Ring"), Plane.Object, FVector(0.f, 0.f, 0.5f), FVector(2.4f));
	Ring->SetCastShadow(false);

	// The camera sits at +X looking back along -X, so screen-left is +Y.
	KeyLight = MakeStudioLight(*this, Root, TEXT("KeyLight"), FVector(260.f, 210.f, 290.f), FVector(0.f, 0.f, 130.f));
	FillLight = MakeStudioLight(*this, Root, TEXT("FillLight"), FVector(250.f, -260.f, 140.f), FVector(0.f, 0.f, 120.f));
	RimLight = MakeStudioLight(*this, Root, TEXT("RimLight"), FVector(-260.f, 60.f, 260.f), FVector(0.f, 0.f, 150.f));
	FillLight->SetCastShadows(false);

	PostProcessBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PostProcessBounds"));
	PostProcessBounds->SetupAttachment(Root);
	PostProcessBounds->SetBoxExtent(FVector(2500.f));
	PostProcessBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(PostProcessBounds);
	PostProcess->bUnbound = false;
	PostProcess->Priority = 10.f; // over the rooftop's unbound look
	FPostProcessSettings& Settings = PostProcess->Settings;
	// Fixed exposure, as the rooftop's (setup_menu.py "Look"), neutral: no grading, a light vignette.
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Manual;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = 10.f;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = 0.35f;
	ApplyPreset(EShinobiStudioLight::Studio);
}

void AShinobiStudioSet::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	const auto Use = [](UStaticMeshComponent* Mesh, const TCHAR* Path)
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Mesh->SetMaterial(0, Material);
		}
		else
		{
			UE_LOG(LogShinobiCoop, Warning, TEXT("AShinobiStudioSet: no %s (Scripts/setup_menu.py)"), Path);
		}
	};
	Use(Backdrop, StudioBackdropMaterial);
	Use(Floor, StudioFloorMaterial);
	Use(Ring, StudioRingMaterial);
	BackdropMaterial = Backdrop->CreateDynamicMaterialInstance(0);
	ApplyPreset(Preset);
}

void AShinobiStudioSet::ApplyPreset(EShinobiStudioLight NewPreset)
{
	Preset = NewPreset;
	const FStudioPreset& Values = StudioPresets[FMath::Clamp(int32(NewPreset), 0, int32(UE_ARRAY_COUNT(StudioPresets)) - 1)];
	KeyLight->SetLightColor(Values.Key);
	KeyLight->SetIntensity(Values.KeyIntensity);
	FillLight->SetLightColor(Values.Key);
	FillLight->SetIntensity(Values.KeyIntensity * StudioFillShare);
	RimLight->SetLightColor(Values.Rim);
	RimLight->SetIntensity(Values.RimIntensity);
	if (BackdropMaterial)
	{
		BackdropMaterial->SetVectorParameterValue(TEXT("Tint"), Values.BackdropTint);
	}
}
