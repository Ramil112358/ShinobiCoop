#include "Shinobi/Enemies/ShinobiSummon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/Enemies/GA_SummonAttacks.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	struct FSummonStats
	{
		float Health;
		float Damage;       // per hit (a dog bites twice per attack)
		float AttackRange;
		float AttackInterval;
		float Speed;
		float Lifetime;
		float Scale;
		FLinearColor Tint;  // the placeholder mannequin's colour, until the creature's own model is worn
		int32 Count;
	};

	const FSummonStats& SummonStatsFor(EShinobiSummonKind Kind)
	{
		static const FSummonStats Table[] = {
			{ 600.f, 60.f, 350.f, 2.5f, 350.f, 12.f, 2.5f, FLinearColor(0.1f, 0.4f, 0.1f), 1 },
			{ 350.f, 70.f, 800.f, 2.0f, 500.f, 10.f, 1.0f, FLinearColor(0.35f, 0.1f, 0.5f), 1 },
			{ 120.f, 18.f, 170.f, 0.8f, 800.f, 15.f, 1.0f, FLinearColor(0.15f, 0.15f, 0.17f), AShinobiSummon::DogPackSize },
		};
		return Table[static_cast<int32>(Kind)];
	}
	/** What a summon wears over the mannequin. Clips are ClipPrefix + name; a null name means none (the body ragdolls instead of dying). */
	struct FCreatureLook
	{
		const TCHAR* Mesh;
		const TCHAR* ClipPrefix;
		const TCHAR* Idle;
		const TCHAR* Walk;
		const TCHAR* Run;
		const TCHAR* Bite;
		const TCHAR* Death;
		float Yaw;        // turns the model to face +X
		float Scale;
		float CapsuleRadius;
		float CapsuleHalfHeight;
		bool bDarkCoat;   // swap in the "...Dark" twin of each material
	};

	const FCreatureLook* CreatureLookFor(EShinobiSummonKind Kind)
	{
		// The Animal Variety Pack wolf faces +Y and is a 2 m wolf; a ninken is a little bigger.
		static const FCreatureLook Dog = { TEXT("/Game/AnimalVarietyPack/Wolf/Meshes/SK_Wolf"), TEXT("/Game/AnimalVarietyPack/Wolf/Animations/ANIM_Wolf_"),
			TEXT("IdleAggressive"), TEXT("Walk"), TEXT("Run"), TEXT("Bite"), nullptr, -90.f, 1.1f, 45.f, 55.f, true };
		// The Mesh2Motion snake (Scripts/character/export_creature.py) faces +X, 4 m long; a summoned one is three times that.
		static const FCreatureLook Snake = { TEXT("/Game/Shinobi/Creatures/Snake/SK_Snake"), TEXT("/Game/Shinobi/Creatures/Snake/ANIM_Snake_"),
			TEXT("Idle"), TEXT("Move"), TEXT("Move"), TEXT("Bite"), TEXT("Death"), 0.f, 3.f, 60.f, 40.f, false };
		return Kind == EShinobiSummonKind::Dog ? &Dog : Kind == EShinobiSummonKind::Snake ? &Snake : nullptr;
	}
}

AShinobiSummon::AShinobiSummon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players;
	UseMannequinMesh();
}

void AShinobiSummon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiSummon, Kind, COND_InitialOnly);
}

float AShinobiSummon::GetLifetime(EShinobiSummonKind Kind)
{
	return SummonStatsFor(Kind).Lifetime;
}

void AShinobiSummon::Configure()
{
	const FSummonStats& Stats = SummonStatsFor(Kind);
	BaseHealth = Stats.Health;
	AttackDamage = Stats.Damage;
	AttackRange = Stats.AttackRange;
	AttackInterval = Stats.AttackInterval;
	BodyTint = Stats.Tint;
	AttackAbility = Kind == EShinobiSummonKind::Toad ? TSubclassOf<UShinobiGameplayAbility>(UGA_ToadSlam::StaticClass())
		: Kind == EShinobiSummonKind::Snake ? TSubclassOf<UShinobiGameplayAbility>(UGA_SnakeStrike::StaticClass())
		: TSubclassOf<UShinobiGameplayAbility>(UGA_DogBite::StaticClass());
	StartupAbilities = { AttackAbility }; // granted by the character base's BeginPlay, after this
	GetCharacterMovement()->MaxWalkSpeed = Stats.Speed;
	SetActorScale3D(FVector(Stats.Scale));
	if (const FCreatureLook* Look = CreatureLookFor(Kind))
	{
		GetCapsuleComponent()->SetCapsuleSize(Look->CapsuleRadius, Look->CapsuleHalfHeight);
		WearCreature();
	}
}

void AShinobiSummon::WearCreature()
{
	const FCreatureLook* Look = CreatureLookFor(Kind);
	USkeletalMesh* Model = Look && !IsNetMode(NM_DedicatedServer) ? LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Look->Mesh, *FPaths::GetBaseFilename(Look->Mesh)), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Model)
	{
		return;
	}
	auto Clip = [Look](const TCHAR* Name) -> UAnimSequence*
	{
		if (!Name)
		{
			return nullptr;
		}
		const FString Path = FString(Look->ClipPrefix) + Name;
		return LoadObject<UAnimSequence>(nullptr, *(Path + TEXT(".") + FPaths::GetBaseFilename(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	IdleClip = Clip(Look->Idle);
	WalkClip = Clip(Look->Walk);
	RunClip = Clip(Look->Run);
	BiteClip = Clip(Look->Bite);
	DeathClip = Clip(Look->Death);

	Creature = NewObject<USkeletalMeshComponent>(this, TEXT("Creature"));
	Creature->SetSkeletalMeshAsset(Model);
	Creature->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Creature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The model's paws (belly) rest on Z = 0; the capsule's floor is at -half height.
	Creature->SetupAttachment(GetCapsuleComponent());
	Creature->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -Look->CapsuleHalfHeight), FRotator(0.f, Look->Yaw, 0.f));
	Creature->SetRelativeScale3D(FVector(Look->Scale));
	if (Look->bDarkCoat) // every material has a "...Dark" twin beside it
	{
		for (int32 Slot = 0; Slot < Creature->GetNumMaterials(); ++Slot)
		{
			if (const UMaterialInterface* Coat = Creature->GetMaterial(Slot))
			{
				const FString Dark = FString::Printf(TEXT("%s/%sDark.%sDark"), *FPackageName::GetLongPackagePath(Coat->GetPackage()->GetName()), *Coat->GetName(), *Coat->GetName());
				if (UMaterialInterface* Twin = LoadObject<UMaterialInterface>(nullptr, *Dark, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					Creature->SetMaterial(Slot, Twin);
				}
			}
		}
	}
	Creature->RegisterComponent();
	GetMesh()->SetVisibility(false); // the mannequin only carries the capsule's old look; hidden under the creature
}

void AShinobiSummon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Creature)
	{
		UpdateCreatureClip();
	}
}

void AShinobiSummon::UpdateCreatureClip()
{
	if (!IsAlive()) // a creature with a death clip plays it; one without (the wolf) is a ragdoll
	{
		if (DeathClip && !bDeathPlayed)
		{
			bDeathPlayed = true;
			Creature->PlayAnimation(DeathClip, false);
		}
		return;
	}
	const bool bBiting = AbilitySystem->HasMatchingGameplayTag(ShinobiTags::State_SummonAttacking);
	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Wanted = bBiting ? BiteClip.Get() : Speed > 250.f ? RunClip.Get() : Speed > 30.f ? WalkClip.Get() : IdleClip.Get();
	if (Wanted && (Wanted != PlayingClip || (bBiting && !bWasBiting)))
	{
		PlayingClip = Wanted;
		Creature->PlayAnimation(Wanted, !bBiting);
	}
	bWasBiting = bBiting;
}

void AShinobiSummon::BeginPlay()
{
	Configure();
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(LifetimeTimer, this, &AShinobiSummon::Dismiss, SummonStatsFor(Kind).Lifetime);
	}
}

void AShinobiSummon::Dismiss()
{
	if (!IsAlive() || IsActorBeingDestroyed())
	{
		return;
	}
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SummonPoof, GetActorLocation() - FVector(0.f, 0.f, GetSimpleCollisionHalfHeight()), GetActorScale3D().X);
	Destroy();
}

TArray<AShinobiSummon*> AShinobiSummon::Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground)
{
	TArray<AShinobiSummon*> Pack;
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!World || !Caster->HasAuthority())
	{
		return Pack;
	}
	TArray<AShinobiSummon*> Previous;
	for (TActorIterator<AShinobiSummon> It(World); It; ++It)
	{
		if (It->Summoner == Caster && It->Kind == Kind)
		{
			Previous.Add(*It);
		}
	}
	for (AShinobiSummon* Old : Previous)
	{
		Old->Dismiss();
	}

	const FSummonStats& Stats = SummonStatsFor(Kind);
	const FCreatureLook* Look = CreatureLookFor(Kind);
	const float HalfHeight = Look ? Look->CapsuleHalfHeight : GetDefault<AShinobiSummon>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Stats.Scale;
	const float Yaw = Caster->GetActorRotation().Yaw;
	for (int32 Index = 0; Index < Stats.Count; ++Index)
	{
		const float Angle = 2.f * PI * Index / Stats.Count;
		const FVector Spot = Ground + (Stats.Count > 1 ? FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 160.f : FVector::ZeroVector);
		const FTransform Where(FRotator(0.f, Yaw, 0.f), Spot + FVector(0.f, 0.f, HalfHeight), FVector(Stats.Scale));
		AShinobiSummon* Summon = World->SpawnActorDeferred<AShinobiSummon>(StaticClass(), Where, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Summon)
		{
			continue;
		}
		Summon->Kind = Kind;
		Summon->Summoner = Caster;
		Summon->FinishSpawning(Where);
		Pack.Add(Summon);
		AShinobiBurst::Spawn(World, EShinobiBurst::SummonPoof, Spot, Stats.Scale);
	}
	return Pack;
}
