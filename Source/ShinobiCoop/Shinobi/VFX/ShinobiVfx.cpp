#include "Shinobi/VFX/ShinobiVfx.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Engine/CollisionProfile.h"
#include "TimerManager.h"

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

	UMaterialInstanceDynamic* MakePushDome(UObject* Outer)
	{
		UMaterialInstanceDynamic* Dome = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_PushDome.M_PushDome"));
		if (Dome)
		{
			Dome->SetScalarParameterValue(TEXT("Fade"), 1.f);
		}
		return Dome;
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
			static const TCHAR* TairnFire = TEXT("/Game/Shinobi/VFX/T_TairnFire.T_TairnFire");
			static const TCHAR* CrimsonFire = TEXT("/Game/Shinobi/VFX/T_CrimsonFire.T_CrimsonFire");
			static const TCHAR* ChakraFlow = TEXT("/Game/Shinobi/VFX/T_ChakraFlow.T_ChakraFlow");
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
				{Add, TairnFire, 0, 8},     // TairnFire
				{Add, CrimsonFire, 0, 8},   // CrimsonFire
				{Add, ChakraFlow, 0, 4},    // ChakraWisp
				{Add, ChakraFlow, 4, 4},    // ChakraSwirl
				{Add, TEXT("/Game/Shinobi/VFX/T_ChidoriOrb.T_ChidoriOrb"), 0, 8},   // ChidoriOrb
				{Add, TEXT("/Game/Shinobi/VFX/T_RasenganOrb.T_RasenganOrb"), 0, 8}, // RasenganOrb
				{TEXT("/Game/Shinobi/VFX/M_SpriteDust.M_SpriteDust"), TEXT("/Game/Shinobi/VFX/T_PushDust.T_PushDust"), 0, 8}, // Dust
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

	namespace
	{
		/** The floor under Location (a hit can land at body height); characters are not floor. */
		FVector GroundBelow(UWorld* World, const FVector& Location)
		{
			FHitResult Hit;
			const FCollisionObjectQueryParams Floor(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
			return World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 60.f), Location - FVector(0.f, 0.f, 600.f), Floor) ? Hit.ImpactPoint : Location;
		}
	}

	void SpawnScorch(UWorld* World, const FVector& Location, float Radius, const FLinearColor& Tint, float Linger, const TCHAR* Mark)
	{
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (!World || World->GetNetMode() == NM_DedicatedServer || !Plane)
		{
			return;
		}
		const FVector Ground = GroundBelow(World, Location);
		AActor* Scorch = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ground));
		UMaterialInstanceDynamic* Material = MakeInstance(Scorch, TEXT("/Game/Shinobi/VFX/M_SpriteScorch.M_SpriteScorch"));
		if (!Scorch || !Material)
		{
			return;
		}
		if (Mark)
		{
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Mark);
			if (ensureMsgf(Texture, TEXT("ShinobiVfx: missing texture %s"), Mark))
			{
				Material->SetTextureParameterValue(TEXT("Atlas"), Texture);
			}
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

	void SpawnDebris(UWorld* World, const FVector& Location, int32 Count, float Speed, float Size, float Linger)
	{
		// ponytail: uneven engine cubes in stone grey stand in until the Tripo debris chunks are imported
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!World || World->GetNetMode() == NM_DedicatedServer || !Mesh)
		{
			return;
		}
		const FVector Ground = GroundBelow(World, Location);
		AActor* Pile = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ground));
		if (!Pile)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Pile);
		Pile->SetRootComponent(Root); // a root assigned after spawning starts at the origin: place it explicitly
		Root->SetWorldLocation(Ground);
		Root->RegisterComponent();
		UMaterialInstanceDynamic* Rock = Basic ? UMaterialInstanceDynamic::Create(Basic, Pile) : nullptr;
		if (Rock)
		{
			Rock->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.1f, 0.085f));
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = Index * 2.f * PI / Count + FMath::FRandRange(-0.3f, 0.3f);
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(Pile);
			Chunk->SetStaticMesh(Mesh);
			Chunk->SetMaterial(0, Rock);
			Chunk->SetupAttachment(Root);
			Chunk->SetWorldTransform(FTransform(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f), Ground + Out * 25.f + FVector(0.f, 0.f, 15.f), FVector(FMath::FRandRange(0.6f, 1.4f), FMath::FRandRange(0.6f, 1.4f), FMath::FRandRange(0.4f, 1.f)) * Size));
			// Physics only (no traces hit it), and it passes through characters rather than tripping them.
			Chunk->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
			Chunk->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
			Chunk->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Chunk->SetCastShadow(false);
			Chunk->SetSimulatePhysics(true);
			Chunk->RegisterComponent();
			Chunk->SetPhysicsLinearVelocity((Out * FMath::FRandRange(0.6f, 1.f) + FVector(0.f, 0.f, FMath::FRandRange(0.4f, 0.9f))) * Speed);
			Chunk->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 720.f);
		}
		// Near the end they stop colliding and fall through the floor, rather than popping out of sight.
		FTimerHandle Sink;
		World->GetTimerManager().SetTimer(Sink, FTimerDelegate::CreateWeakLambda(Pile, [Pile]()
		{
			Pile->ForEachComponent<UStaticMeshComponent>(false, [](UStaticMeshComponent* Chunk)
			{
				Chunk->SetCollisionResponseToAllChannels(ECR_Ignore);
				Chunk->WakeRigidBody();
			});
		}), FMath::Max(Linger - 0.6f, 0.1f), false);
		Pile->SetLifeSpan(Linger);
	}
}
