#include "Shinobi/Enemies/ShinobiSummon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "NavigationInvokerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/GA_EnemyAttacks.h"
#include "Shinobi/Enemies/GA_SummonAttacks.h"
#include "Shinobi/Enemies/ShinobiAIController.h"
#include "Shinobi/Enemies/ShinobiCreatureAnimInstance.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	struct FSummonStats
	{
		float Health;
		float Damage;       // per hit
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
			{ 600.f, 60.f, 350.f, 2.5f, 350.f, 12.f, 1.0f, FLinearColor(0.1f, 0.4f, 0.1f), 1 },
			{ 350.f, 70.f, 800.f, 2.0f, 500.f, 10.f, 1.0f, FLinearColor(0.35f, 0.1f, 0.5f), 1 },
			{ 120.f, 18.f, 170.f, 0.8f, 650.f, 15.f, 1.0f, FLinearColor(0.15f, 0.15f, 0.17f), AShinobiSummon::DogPackSize },
		};
		return Table[static_cast<int32>(Kind)];
	}
	/** What a summon wears over the mannequin. Clips are ClipPrefix + name; a null name means none (a creature with no death clip ragdolls). */
	struct FCreatureLook
	{
		const TCHAR* Mesh;
		const TCHAR* ClipPrefix;
		const TCHAR* Idle;
		const TCHAR* Walk;
		const TCHAR* Run;     // null: it walks faster instead
		const TCHAR* Attack;
		const TCHAR* Hit;
		const TCHAR* Death;
		float WalkSpeed;      // world cm/s at which each clip covers the ground at play rate 1 (the model's scale included)
		float RunSpeed;
		float Yaw;            // turns the model to face +X
		FVector Scale;
		float CapsuleRadius;
		float CapsuleHalfHeight;
		bool bDarkCoat;       // swap in the "...Dark" twin of each material
	};

	const FCreatureLook& CreatureLookFor(EShinobiSummonKind Kind)
	{
		// The Animal Variety Pack wolf faces +Y and is a 2 m wolf; a ninken is a little bigger. Its Walk covers 93 cm/s and its Run 444
		// (read off the root-motion variants of the clips), times the 1.1 scale.
		static const FCreatureLook Dog = { TEXT("/Game/AnimalVarietyPack/Wolf/Meshes/SK_Wolf"), TEXT("/Game/AnimalVarietyPack/Wolf/Animations/ANIM_Wolf_"),
			TEXT("IdleAggressive"), TEXT("Walk"), TEXT("Run"), TEXT("Bite"), TEXT("GetHitFront"), nullptr, 102.f, 488.f, -90.f, FVector(1.1f), 45.f, 55.f, true };
		// The Mesh2Motion snake (Scripts/character/export_creature.py) faces +X, 4 m long; a summoned one is two and a half times as long and
		// five times as thick. One winding clip, played at 1 when it moves at its top speed.
		static const FCreatureLook Snake = { TEXT("/Game/Shinobi/Creatures/Snake/SK_Snake"), TEXT("/Game/Shinobi/Creatures/Snake/ANIM_Snake_"),
			TEXT("Idle"), TEXT("Move"), nullptr, TEXT("Bite"), TEXT("Hit"), TEXT("Death"), 500.f, 500.f, 0.f, FVector(2.5f, 5.f, 5.f), 60.f, 40.f, false };
		// The Rodin toad (Scripts/character/build_toad.py) faces +X, 1.9 m long and 1.5 m tall; a summoned one is two and a half times that.
		// One hop clip, played at 1 when it moves at its top speed.
		static const FCreatureLook Toad = { TEXT("/Game/Shinobi/Creatures/Toad/SK_Toad"), TEXT("/Game/Shinobi/Creatures/Toad/ANIM_Toad_"),
			TEXT("Idle"), TEXT("Move"), nullptr, TEXT("Slam"), TEXT("Hit"), TEXT("Death"), 350.f, 350.f, 0.f, FVector(2.5f), 170.f, 190.f, false };
		return Kind == EShinobiSummonKind::Dog ? Dog : Kind == EShinobiSummonKind::Snake ? Snake : Toad;
	}
}

AShinobiSummon::AShinobiSummon(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Team = EShinobiTeam::Players; // until Summon() gives it its caster's
	UseMannequinMesh();
	NavInvoker->SetGenerationRadii(1500.f, 2500.f); // fast and plentiful: the inherited 5 km would keep rebuilding navmesh tiles
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
	GetCapsuleComponent()->SetCapsuleSize(CreatureLookFor(Kind).CapsuleRadius, CreatureLookFor(Kind).CapsuleHalfHeight);
	WearCreature();
}

void AShinobiSummon::WearCreature()
{
	const FCreatureLook* Look = &CreatureLookFor(Kind);
	USkeletalMesh* Model = !IsNetMode(NM_DedicatedServer) ? LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Look->Mesh, *FPaths::GetBaseFilename(Look->Mesh)), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
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
	FShinobiCreatureClips Clips;
	Clips.Idle = Clip(Look->Idle);
	Clips.Walk = Clip(Look->Walk);
	Clips.Run = Clip(Look->Run);
	Clips.Attack = Clip(Look->Attack);
	Clips.Hit = Clip(Look->Hit);
	Clips.Death = Clip(Look->Death);
	Clips.WalkSpeed = Look->WalkSpeed;
	Clips.RunSpeed = Look->RunSpeed;
	if (const UGA_EnemyAttack* Attack = AttackAbility ? Cast<UGA_EnemyAttack>(AttackAbility->GetDefaultObject()) : nullptr)
	{
		Clips.AttackSeconds = Attack->WindUp + Attack->Recovery; // the clip spans the whole attack, so its blow lands with the ability's
	}

	Creature = NewObject<USkeletalMeshComponent>(this, TEXT("Creature"));
	Creature->SetSkeletalMeshAsset(Model);
	Creature->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Creature->SetAnimInstanceClass(UShinobiCreatureAnimInstance::StaticClass());
	Creature->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The model's paws (belly) rest on Z = 0; the capsule's floor is at -half height, and it faces +X. It rides GetMesh(), which the
	// movement component smooths on clients (a capsule child would snap to every network update); the transform below cancels the
	// mannequin mesh's own offset so the creature ends up where the capsule says.
	const FTransform Wanted(FRotator(0.f, Look->Yaw, 0.f), FVector(0.f, 0.f, -Look->CapsuleHalfHeight));
	Creature->SetupAttachment(GetMesh());
	Creature->SetRelativeTransform(Wanted * FTransform(GetBaseRotationOffset(), GetBaseTranslationOffset()).Inverse()); // the default offset: the mesh's current one includes network smoothing
	Creature->SetRelativeScale3D(Look->Scale);
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
	if (UShinobiCreatureAnimInstance* Anim = Cast<UShinobiCreatureAnimInstance>(Creature->GetAnimInstance()))
	{
		Anim->Setup(Clips);
	}
	GetMesh()->SetVisibility(false); // the mannequin only carries the capsule's old look; hidden under the creature
}

bool AShinobiSummon::CanTarget(const AShinobiCharacterBase& Candidate) const
{
	const AShinobiCharacterBase* Caster = Summoner.Get();
	return Caster && FVector::Dist2D(Candidate.GetActorLocation(), Caster->GetActorLocation()) <= LeashRange;
}

bool AShinobiSummon::GetIdlePost(FVector& OutPost) const
{
	const AShinobiCharacterBase* Caster = Summoner.Get();
	if (!Caster)
	{
		return false;
	}
	OutPost = Caster->GetActorLocation() + FVector(FMath::Cos(FollowAngle), FMath::Sin(FollowAngle), 0.f) * FollowRadius;
	return true;
}

FVector AShinobiSummon::GetPostBesideCaster() const
{
	FVector Post = GetActorLocation();
	GetIdlePost(Post);
	// On the ground there (a caster on a dragon is high above it), standing as tall as this summon's capsule.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SummonPost), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Post + FVector(0.f, 0.f, 400.f), Post - FVector(0.f, 0.f, 3000.f), ECC_WorldStatic, Params))
	{
		Post = Hit.ImpactPoint + FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	}
	return Post;
}

void AShinobiSummon::CatchUpWithCaster()
{
	const AShinobiCharacterBase* Caster = Summoner.Get();
	const AShinobiAIController* AI = Cast<AShinobiAIController>(GetController());
	const float Limit = AI && !AI->HasTarget() ? IdleCatchUpDistance : CatchUpDistance;
	if (!Caster || IsBusy() || FVector::Dist2D(GetActorLocation(), Caster->GetActorLocation()) <= Limit)
	{
		return;
	}
	const FVector Floor = FVector(0.f, 0.f, GetSimpleCollisionHalfHeight());
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SummonPoof, GetActorLocation() - Floor, GetActorScale3D().X);
	const FVector Post = GetPostBesideCaster();
	TeleportTo(Post, FRotator(0.f, (Caster->GetActorLocation() - Post).Rotation().Yaw, 0.f));
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SummonPoof, Post - Floor, GetActorScale3D().X);
}

void AShinobiSummon::HurryToPost()
{
	const AShinobiAIController* AI = Cast<AShinobiAIController>(GetController());
	FVector Post;
	const bool bHurry = AI && !AI->HasTarget() && !IsBusy() && GetIdlePost(Post) && FVector::Dist2D(GetActorLocation(), Post) > HurryDistance;
	GetCharacterMovement()->MaxWalkSpeed = SummonStatsFor(Kind).Speed * (bHurry ? CatchUpSpeedScale : 1.f);
}

void AShinobiSummon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		// The contract ends with the caster: downed, dead, or gone from the game (a respawned pawn is a different caster).
		const AShinobiCharacterBase* Caster = Summoner.Get();
		if (!Caster || !Caster->IsAlive())
		{
			Dismiss();
			return;
		}
		CatchUpWithCaster();
		HurryToPost();
	}
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
	if (!HasAuthority() || !IsAlive() || IsActorBeingDestroyed())
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
	const float HalfHeight = CreatureLookFor(Kind).CapsuleHalfHeight;
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
		// Where it waits beside the caster: the toad behind on one side, the snake on the other, the dogs fanned out all round.
		Summon->FollowAngle = Kind == EShinobiSummonKind::Toad ? PI * 1.25f : Kind == EShinobiSummonKind::Snake ? PI * 0.75f : Angle;
		Summon->FollowRadius = Kind == EShinobiSummonKind::Dog ? 330.f : FollowDistance * 0.85f;
		Summon->Summoner = Caster;
		Summon->Team = Caster->GetTeam(); // a caster on a PvP side brings the summon along
		Summon->FinishSpawning(Where);
		Pack.Add(Summon);
		AShinobiBurst::Spawn(World, EShinobiBurst::SummonPoof, Spot, Stats.Scale);
	}
	return Pack;
}
