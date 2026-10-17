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
			{ 500.f, 70.f, 700.f, 2.2f, 380.f, 12.f, 1.0f, FLinearColor(0.55f, 0.35f, 0.2f), 1 },
			{ 450.f, 45.f, 190.f, 0.8f, 650.f, 15.f, 1.0f, FLinearColor(0.75f, 0.5f, 0.3f), 1 }, // one Pabu worth about the old pack of four
			{ 500.f, 65.f, 650.f, 1.6f, 600.f, 15.f, 1.0f, FLinearColor(0.7f, 0.55f, 0.35f), 1 }, // Pusho: AttackRange is how far he pounces from
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
	};

	const FCreatureLook& CreatureLookFor(EShinobiSummonKind Kind)
	{
		// Pabu (Tripo model, rigged and keyed by Scripts/character/build_pabu.py) faces +X at true corgi size, 0.66 m long and 0.45 m to the
		// ear tips; a summoned one is 1.6 times that. His trot covers 144 cm/s and his gallop 480 at play rate 1 (printed by the build script).
		static const FCreatureLook Pabu = { TEXT("/Game/Shinobi/Creatures/Pabu/SK_Pabu"), TEXT("/Game/Shinobi/Creatures/Pabu/ANIM_Pabu_"),
			TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Bite"), TEXT("Hit"), TEXT("Death"), 144.f, 480.f, 0.f, FVector(1.6f), 35.f, 38.f };
		// Misha (Tripo model of the user's plush, rigged and keyed by Scripts/character/build_misha.py) sits facing +X, 0.74 m to the top of
		// his head; a summoned one is a giant plush four and a half times that. He gets about in seated hops: one Move clip covering 315 cm/s
		// at play rate 1 (printed by the build script).
		static const FCreatureLook Misha = { TEXT("/Game/Shinobi/Creatures/Misha/SK_Misha"), TEXT("/Game/Shinobi/Creatures/Misha/ANIM_Misha_"),
			TEXT("Idle"), TEXT("Move"), nullptr, TEXT("Bark"), TEXT("Hit"), TEXT("Death"), 315.f, 315.f, 0.f, FVector(4.5f), 110.f, 168.f };
		// The Rodin toad (Scripts/character/build_toad.py) faces +X, 1.9 m long and 1.5 m tall; a summoned one is two and a half times that.
		// One hop clip, played at 1 when it moves at its top speed.
		static const FCreatureLook Toad = { TEXT("/Game/Shinobi/Creatures/Toad/SK_Toad"), TEXT("/Game/Shinobi/Creatures/Toad/ANIM_Toad_"),
			TEXT("Idle"), TEXT("Move"), nullptr, TEXT("Slam"), TEXT("Hit"), TEXT("Death"), 350.f, 350.f, 0.f, FVector(2.5f), 170.f, 190.f };
		// Pusho (Tripo model of the user's cat, rigged and keyed by Scripts/character/build_pusho.py) faces +X at true cat size, 0.36 m to the
		// ear tips; a summoned one is a panther-sized 2.75 times that, 1 m. His walk covers 165 cm/s and his gallop 605 at play rate 1.
		static const FCreatureLook Pusho = { TEXT("/Game/Shinobi/Creatures/Pusho/SK_Pusho"), TEXT("/Game/Shinobi/Creatures/Pusho/ANIM_Pusho_"),
			TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Pounce"), TEXT("Hit"), TEXT("Death"), 165.f, 605.f, 0.f, FVector(2.75f), 45.f, 50.f };
		switch (Kind)
		{
		case EShinobiSummonKind::Pabu: return Pabu;
		case EShinobiSummonKind::Misha: return Misha;
		case EShinobiSummonKind::Pusho: return Pusho;
		default: return Toad;
		}
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
	switch (Kind)
	{
	case EShinobiSummonKind::Toad: AttackAbility = UGA_ToadSlam::StaticClass(); break;
	case EShinobiSummonKind::Misha: AttackAbility = UGA_MishaBark::StaticClass(); break;
	case EShinobiSummonKind::Pusho: AttackAbility = UGA_PushoPounce::StaticClass(); break;
	default: AttackAbility = UGA_PabuBite::StaticClass(); break;
	}
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
		Clips.AttackSeconds = Attack->GetTotalSeconds(); // the clip spans the whole attack, so its blow lands with the ability's
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
		// Where it waits beside the caster: the toad behind on one side, Misha on the other, Pabu in front, Pusho in front on the other
		// side; a pack (Count > 1) fans out all round.
		Summon->FollowAngle = Kind == EShinobiSummonKind::Toad ? PI * 1.25f : Kind == EShinobiSummonKind::Misha ? PI * 0.75f
			: Kind == EShinobiSummonKind::Pusho ? PI * 1.75f : Stats.Count > 1 ? Angle : PI * 0.25f;
		Summon->FollowRadius = Kind == EShinobiSummonKind::Pabu || Kind == EShinobiSummonKind::Pusho ? 250.f : FollowDistance * 0.85f; // the small ones keep close
		Summon->Summoner = Caster;
		Summon->Team = Caster->GetTeam(); // a caster on a PvP side brings the summon along
		Summon->FinishSpawning(Where);
		Pack.Add(Summon);
		AShinobiBurst::Spawn(World, EShinobiBurst::SummonPoof, Spot, Stats.Scale);
	}
	return Pack;
}
