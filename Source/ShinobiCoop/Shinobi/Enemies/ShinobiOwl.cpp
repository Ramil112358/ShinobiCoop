#include "Shinobi/Enemies/ShinobiOwl.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/VFX/ShinobiBurst.h"

namespace
{
	// The owl model (Scripts/character/build_owl.py) faces +X with her feet's lowest point on Z = 0, 1 m tall; her middle is half that.
	const TCHAR* OwlMesh = TEXT("/Game/Shinobi/Creatures/Owl/SK_Owl.SK_Owl");
	const TCHAR* OwlHover = TEXT("/Game/Shinobi/Creatures/Owl/ANIM_Owl_Hover.ANIM_Owl_Hover");
	const TCHAR* OwlStrike = TEXT("/Game/Shinobi/Creatures/Owl/ANIM_Owl_Strike.ANIM_Owl_Strike");
	constexpr float ModelMiddle = 50.f;
}

AShinobiOwl::AShinobiOwl()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false); // every machine flies her to the post itself
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -ModelMiddle * Scale));
	Body->SetRelativeScale3D(FVector(Scale));
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose; // the server times her clips too
}

void AShinobiOwl::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiOwl, Summoner, COND_InitialOnly);
	DOREPLIFETIME(AShinobiOwl, Target);
	DOREPLIFETIME(AShinobiOwl, Strikes);
}

FVector AShinobiOwl::GetPost(const AShinobiCharacterBase& Caster)
{
	// In the caster's own frame, which a wall-run turns with gravity: up is away from the wall they run on.
	const FVector Feet = Caster.GetActorLocation() - Caster.GetActorUpVector() * Caster.GetSimpleCollisionHalfHeight();
	return Feet + Caster.GetActorQuat().RotateVector(FVector(-PostBehind, -PostLeft, PostHeight));
}

AShinobiOwl* AShinobiOwl::Summon(AShinobiCharacterBase* Caster)
{
	UWorld* World = Caster ? Caster->GetWorld() : nullptr;
	if (!World || !Caster->HasAuthority())
	{
		return nullptr;
	}
	TArray<AShinobiOwl*> Previous;
	for (TActorIterator<AShinobiOwl> It(World); It; ++It)
	{
		if (It->Summoner == Caster)
		{
			Previous.Add(*It);
		}
	}
	for (AShinobiOwl* Old : Previous)
	{
		Old->Dismiss();
	}
	const FTransform Where(Caster->GetActorRotation(), GetPost(*Caster));
	AShinobiOwl* Owl = World->SpawnActorDeferred<AShinobiOwl>(StaticClass(), Where, Caster, Caster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Owl)
	{
		return nullptr;
	}
	Owl->Summoner = Caster;
	Owl->FinishSpawning(Where);
	AShinobiBurst::Spawn(World, EShinobiBurst::SummonPoof, Where.GetLocation(), Scale);
	return Owl;
}

void AShinobiOwl::BeginPlay()
{
	Super::BeginPlay();
	if (!IsNetMode(NM_DedicatedServer))
	{
		if (USkeletalMesh* Model = LoadObject<USkeletalMesh>(nullptr, OwlMesh, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Body->SetSkeletalMeshAsset(Model);
			HoverClip = LoadObject<UAnimSequence>(nullptr, OwlHover, nullptr, LOAD_NoWarn | LOAD_Quiet);
			StrikeClip = LoadObject<UAnimSequence>(nullptr, OwlStrike, nullptr, LOAD_NoWarn | LOAD_Quiet);
			PlayClip(HoverClip, true);
		}
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(LifetimeTimer, this, &AShinobiOwl::Dismiss, Lifetime);
		GetWorldTimerManager().SetTimer(StrikeTimer, this, &AShinobiOwl::StartStrike, StrikeInterval, true, FirstStrikeDelay);
	}
}

void AShinobiOwl::Dismiss()
{
	if (!HasAuthority() || IsActorBeingDestroyed())
	{
		return;
	}
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SummonPoof, GetActorLocation(), Scale);
	Destroy();
}

void AShinobiOwl::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The contract ends with the caster: downed, dead, or gone from the game.
	if (!IsValid(Summoner) || !Summoner->IsAlive())
	{
		Dismiss();
		return;
	}
	const FVector Post = GetPost(*Summoner);
	const FVector Here = GetActorLocation();
	const bool bSnap = FVector::Dist(Here, Post) > SnapDistance;
	const FVector There = bSnap ? Post : FMath::VInterpTo(Here, Post, DeltaSeconds, FollowSpeed);
	// She looks where her caster looks, and at her target while she strikes.
	const bool bStriking = IsValid(Target) && Target->IsAlive() && Body->GetSingleNodeInstance() && StrikeClip && Body->IsPlaying() && Body->GetSingleNodeInstance()->GetAnimationAsset() == StrikeClip;
	const FRotator Facing = bStriking ? FRotationMatrix::MakeFromXZ(Target->GetActorLocation() - There, Summoner->GetActorUpVector()).Rotator() : Summoner->GetActorRotation();
	SetActorLocationAndRotation(There, FMath::RInterpTo(GetActorRotation(), Facing, DeltaSeconds, FollowSpeed));
	if (HoverClip && !Body->IsPlaying())
	{
		PlayClip(HoverClip, true); // a strike has played out
	}
	UpdateFlight(bSnap ? FVector::ZeroVector : (There - Here) / FMath::Max(DeltaSeconds, UE_KINDA_SMALL_NUMBER), DeltaSeconds);
}

void AShinobiOwl::UpdateFlight(const FVector& Velocity, float DeltaSeconds)
{
	// A bird leans into her flight: nose down chasing ahead, nose up braking back, a wing dipped into a sideways drift.
	const FVector Local = GetActorQuat().UnrotateVector(Velocity);
	const FRotator Wanted(-FMath::Clamp(Local.X / LeanSpeed, -1.f, 1.f) * MaxPitch, 0.f, FMath::Clamp(Local.Y / LeanSpeed, -1.f, 1.f) * MaxBank);
	Lean = FMath::RInterpTo(Lean, Wanted, DeltaSeconds, LeanResponse);
	// About her middle, not her feet, so she tips in place.
	const FVector Middle(0.f, 0.f, ModelMiddle * Scale);
	Body->SetRelativeLocationAndRotation(-Lean.RotateVector(Middle), Lean);
	// and beats harder the faster she goes.
	const UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance();
	const bool bHovering = Single && Single->GetAnimationAsset() == HoverClip;
	Body->SetPlayRate(bHovering ? 1.f + (MaxFlapRate - 1.f) * FMath::Clamp(Velocity.Size() / LeanSpeed, 0.f, 1.f) : 1.f);
}

AShinobiCharacterBase* AShinobiOwl::FindTarget() const
{
	AShinobiCharacterBase* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	// Seen from her caster, not from her: with a wall at the caster's left she hovers half inside it.
	const FVector From = Summoner->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OwlSight), false, this);
	Params.AddIgnoredActor(Summoner);
	for (AShinobiCharacterBase* Candidate : ShinobiCombat::FindHostilesInRadius(Summoner, Summoner->GetActorLocation(), StrikeRange))
	{
		if (!Candidate->IsTargetable())
		{
			continue; // in Kamui: untouchable and unseen, as the enemy AI treats it
		}
		const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), Summoner->GetActorLocation());
		FHitResult Hit;
		// Only the world's walls and roofs hide a foe from her: by object type, as characters block the WorldStatic channel too.
		if (Distance < BestDistance && !GetWorld()->LineTraceSingleByObjectType(Hit, From, Candidate->GetActorLocation(), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Best = Candidate;
			BestDistance = Distance;
		}
	}
	return Best;
}

void AShinobiOwl::StartStrike()
{
	if (!IsValid(Summoner) || !Summoner->IsAlive())
	{
		return;
	}
	Target = FindTarget();
	if (!Target)
	{
		return;
	}
	++Strikes;
	PlayClip(StrikeClip, false);
	GetWorldTimerManager().SetTimer(LandTimer, this, &AShinobiOwl::LandStrike, WindUp);
}

void AShinobiOwl::LandStrike()
{
	if (!IsValid(Summoner) || !Summoner->IsAlive() || !IsValid(Target) || !Target->IsTargetable())
	{
		return;
	}
	// Where the foe is now, so the bolt does not miss one that moved during the wind-up.
	ShinobiCombat::ApplyDamage(Summoner->GetAbilitySystemComponent(), Target, Damage, false);
	const FVector Feet = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetSimpleCollisionHalfHeight());
	AShinobiBurst::Spawn(GetWorld(), EShinobiBurst::SkyBolt, Feet);
}

void AShinobiOwl::OnRep_Strikes()
{
	PlayClip(StrikeClip, false);
}

void AShinobiOwl::PlayClip(UAnimSequence* Clip, bool bLoop)
{
	if (Clip && Body->GetSkeletalMeshAsset())
	{
		Body->PlayAnimation(Clip, bLoop);
	}
}
