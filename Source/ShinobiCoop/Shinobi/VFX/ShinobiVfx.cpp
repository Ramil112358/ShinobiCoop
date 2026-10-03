#include "Shinobi/VFX/ShinobiVfx.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace ShinobiVfx
{
	UMaterialInstanceDynamic* MakeGlow(UObject* Outer, const FLinearColor& Color, float Intensity, float Core, float Inflate)
	{
		static const TCHAR* Path = TEXT("/Game/Shinobi/VFX/M_ChakraBodyAura.M_ChakraBodyAura");
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!ensureMsgf(Material, TEXT("ShinobiVfx: missing material %s"), Path))
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(Material, Outer);
		Glow->SetVectorParameterValue(TEXT("Color"), Color);
		Glow->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		Glow->SetScalarParameterValue(TEXT("Core"), Core);
		Glow->SetScalarParameterValue(TEXT("Inflate"), Inflate);
		Glow->SetScalarParameterValue(TEXT("Fade"), 1.f);
		return Glow;
	}

	namespace
	{
		UMaterialInstanceDynamic* MakeInstance(UObject* Outer, const TCHAR* Path)
		{
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
			if (!ensureMsgf(Material, TEXT("ShinobiVfx: missing material %s"), Path))
			{
				return nullptr;
			}
			return UMaterialInstanceDynamic::Create(Material, Outer);
		}
	}

	UMaterialInstanceDynamic* MakeFlame(UObject* Outer, const FLinearColor& Color, float Intensity, bool bDark)
	{
		UMaterialInstanceDynamic* Flame = MakeInstance(Outer, bDark
			? TEXT("/Game/Shinobi/VFX/M_FlameDark.M_FlameDark")
			: TEXT("/Game/Shinobi/VFX/M_Flame.M_Flame"));
		if (Flame)
		{
			Flame->SetVectorParameterValue(TEXT("Color"), Color);
			Flame->SetScalarParameterValue(TEXT("Intensity"), Intensity);
			Flame->SetScalarParameterValue(TEXT("Fade"), 1.f);
		}
		return Flame;
	}

	UMaterialInstanceDynamic* MakeWater(UObject* Outer, const FLinearColor& Color, float Intensity)
	{
		UMaterialInstanceDynamic* Water = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_WaterWall.M_WaterWall"));
		if (Water)
		{
			Water->SetVectorParameterValue(TEXT("Color"), Color);
			Water->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		}
		return Water;
	}

	UMaterialInstanceDynamic* MakeSusanoo(UObject* Outer, float Fade)
	{
		UMaterialInstanceDynamic* Armour = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_Susanoo.M_Susanoo"));
		if (Armour)
		{
			Armour->SetScalarParameterValue(TEXT("Fade"), Fade);
		}
		return Armour;
	}

	UMaterialInstanceDynamic* MakePhased(UObject* Outer, float Opacity)
	{
		UMaterialInstanceDynamic* Ghost = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_Phased.M_Phased"));
		if (Ghost)
		{
			Ghost->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		}
		return Ghost;
	}

	UMaterialInstanceDynamic* MakeStreak(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Streak = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_Streak.M_Streak"));
		if (Streak)
		{
			Streak->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return Streak;
	}

	namespace
	{
		struct FSpriteInfo
		{
			const TCHAR* Material;
			const TCHAR* Atlas;
			int32 FirstCell;
			int32 Count;
		};

		const FSpriteInfo& Info(ESprite Kind)
		{
			static const TCHAR* Add = TEXT("/Game/Shinobi/VFX/M_Sprite.M_Sprite");
			static const TCHAR* Dark = TEXT("/Game/Shinobi/VFX/M_SpriteDark.M_SpriteDark");
			static const TCHAR* Smoke = TEXT("/Game/Shinobi/VFX/M_SpriteSmoke.M_SpriteSmoke");
			static const TCHAR* Flames = TEXT("/Game/Shinobi/VFX/T_Flames.T_Flames");
			static const TCHAR* SmokeEmbers = TEXT("/Game/Shinobi/VFX/T_SmokeEmbers.T_SmokeEmbers");
			static const TCHAR* Energy = TEXT("/Game/Shinobi/VFX/T_Energy.T_Energy");
			static const FSpriteInfo Table[] = {
				{Add, Flames, 0, 8},        // Flame
				{Dark, Flames, 0, 8},       // BlackFlame
				{Smoke, SmokeEmbers, 0, 4}, // Smoke
				{Add, SmokeEmbers, 4, 4},   // Ember
				{Add, Energy, 0, 4},        // Lightning
				{Add, Energy, 4, 1},        // Ring
				{Add, Energy, 5, 1},        // Vortex
				{Add, Energy, 6, 1},        // Slash
				{Add, Energy, 7, 1},        // Burst
			};
			return Table[static_cast<int32>(Kind)];
		}
	}

	int32 SpriteVariants(ESprite Kind)
	{
		return Info(Kind).Count;
	}

	void SetSpriteVariant(UMaterialInstanceDynamic* Material, ESprite Kind, int32 Variant)
	{
		if (!Material)
		{
			return;
		}
		const FSpriteInfo& Sprite = Info(Kind);
		const int32 Cell = Sprite.FirstCell + FMath::Abs(Variant) % Sprite.Count;
		Material->SetScalarParameterValue(TEXT("CellU"), static_cast<float>(Cell % 4));
		Material->SetScalarParameterValue(TEXT("CellV"), static_cast<float>(Cell / 4));
	}

	UMaterialInstanceDynamic* MakeSprite(UObject* Outer, ESprite Kind, int32 Variant, const FLinearColor& Tint, float Intensity)
	{
		const FSpriteInfo& Sprite = Info(Kind);
		UMaterialInstanceDynamic* Material = MakeInstance(Outer, Sprite.Material);
		UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, Sprite.Atlas);
		if (!Material || !ensureMsgf(Atlas, TEXT("ShinobiVfx: missing texture %s"), Sprite.Atlas))
		{
			return nullptr;
		}
		Material->SetTextureParameterValue(TEXT("Atlas"), Atlas);
		Material->SetVectorParameterValue(TEXT("Tint"), Tint);
		Material->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		Material->SetScalarParameterValue(TEXT("Fade"), 1.f);
		SetSpriteVariant(Material, Kind, Variant);
		return Material;
	}

	void SpawnScorch(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Tint, float Linger)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (!World || World->GetNetMode() == NM_DedicatedServer || !Plane)
		{
			return;
		}
		// A fireball can land at body height: settle the mark on whatever floor is below.
		FVector Ground = Location;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 60.f), Location - FVector(0.f, 0.f, 600.f), ECC_Visibility))
		{
			Ground = Hit.ImpactPoint;
		}
		AActor* Scorch = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ground));
		UMaterialInstanceDynamic* Material = MakeInstance(Scorch, TEXT("/Game/Shinobi/VFX/M_SpriteScorch.M_SpriteScorch"));
		if (!Scorch || !Material)
		{
			return;
		}
		Material->SetVectorParameterValue(TEXT("Tint"), Tint);
		Material->SetScalarParameterValue(TEXT("Start"), static_cast<float>(World->GetTimeSeconds()));
		Material->SetScalarParameterValue(TEXT("Linger"), Linger);
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Scorch);
		Mesh->SetStaticMesh(Plane);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetMaterial(0, Material);
		Scorch->SetRootComponent(Mesh); // a root assigned after spawning starts at the origin: place it explicitly
		Mesh->SetWorldTransform(FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Ground + FVector(0.f, 0.f, 3.f), FVector(Radius * 2.f / 100.f)));
		Mesh->RegisterComponent();
		Scorch->SetLifeSpan(Linger);
	}
}
