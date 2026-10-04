#include "Shinobi/ShinobiDragonMount.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	// Both models (Scripts/character/build_dragon.py for Tairn, build_dragon_crimson.py for the crimson one; each a Rodin generation) are
	// about 1.9 m long in their rest pose, centred on the origin with their feet on the ground, facing +X; every flying clip tips them
	// nose-down about the hips. Their backs in that flying pose, between the wings (the torso's top without the spikes), are at the Saddle
	// points below (model centimetres, before the scale); the scale makes them dragon-sized and the saddle goes where a seated rider's hips
	// are. Tairn is among the biggest dragons in the books, so he is the bigger.
	struct FDragonLook
	{
		const TCHAR* Name;       // /Game/Shinobi/Creatures/<Name>/SK_<Name> and ANIM_<Name>_<Clip>
		float Scale;
		FVector Saddle;
		float MouthAhead;        // the head's mouth, ahead of and a little above the seated rider's capsule centre
		float MouthHeight;
	};

	const FDragonLook& DragonLookFor(EShinobiDragonKind Kind)
	{
		static const FDragonLook Tairn = { TEXT("Tairn"), 4.f, FVector(16.f, 0.f, 84.f), 380.f, 50.f };
		static const FDragonLook Crimson = { TEXT("CrimsonDragon"), 3.5f, FVector(16.f, 0.f, 80.f), 330.f, 55.f };
		return Kind == EShinobiDragonKind::Tairn ? Tairn : Crimson;
	}

	constexpr float SeatBelowCapsuleCentre = 15.f;
	constexpr float BankPerYawRate = 0.12f; // degrees of roll per degree a second of turn: a 300 degree a second turn banks 36 degrees
	constexpr float MaxBank = 35.f;
	constexpr float MaxPitch = 25.f;        // at full flight speed straight up or down
	constexpr float BankSpeed = 4.f;        // how quickly the lean follows // how far under the rider's capsule centre the saddle sits
	constexpr float GlideSpeed = 800.f;
	constexpr float GlideClimb = 250.f;
	constexpr float HoverSpeed = 150.f;
}

AShinobiDragonMount::AShinobiDragonMount()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false); // it rides its rider on every machine
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetRootComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(true);
}

void AShinobiDragonMount::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiDragonMount, Rider, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiDragonMount, Kind, COND_InitialOnly);
}

AShinobiDragonMount* AShinobiDragonMount::Find(const AShinobiCharacterBase& Rider)
{
	for (TActorIterator<AShinobiDragonMount> It(Rider.GetWorld()); It; ++It)
	{
		if (It->Rider == &Rider)
		{
			return *It;
		}
	}
	return nullptr;
}

AShinobiDragonMount* AShinobiDragonMount::Spawn(AShinobiCharacterBase* Rider, EShinobiDragonKind Kind)
{
	UWorld* World = Rider ? Rider->GetWorld() : nullptr;
	if (!World || !Rider->HasAuthority())
	{
		return nullptr;
	}
	TArray<AShinobiDragonMount*> Previous;
	for (TActorIterator<AShinobiDragonMount> It(World); It; ++It)
	{
		if (It->Rider == Rider)
		{
			Previous.Add(*It);
		}
	}
	for (AShinobiDragonMount* Old : Previous)
	{
		Old->Destroy();
	}
	const FTransform Where = Rider->GetActorTransform();
	AShinobiDragonMount* Dragon = World->SpawnActorDeferred<AShinobiDragonMount>(StaticClass(), Where, Rider, Rider, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Dragon)
	{
		Dragon->Rider = Rider;
		Dragon->Kind = Kind;
		Dragon->FinishSpawning(Where);
	}
	return Dragon;
}

void AShinobiDragonMount::BeginPlay()
{
	Super::BeginPlay();
	if (!IsNetMode(NM_DedicatedServer))
	{
		const FDragonLook& Look = DragonLookFor(Kind);
		const FString Folder = FString::Printf(TEXT("/Game/Shinobi/Creatures/%s/"), Look.Name);
		auto Clip = [&Folder, &Look](const TCHAR* Name) { return LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%sANIM_%s_%s.ANIM_%s_%s"), *Folder, Look.Name, Name, Look.Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet); };
		USkeletalMesh* Model = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%sSK_%s.SK_%s"), *Folder, Look.Name, Look.Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		IdleClip = Clip(TEXT("Idle"));
		FlapClip = Clip(TEXT("Flap"));
		GlideClip = Clip(TEXT("Glide"));
		BreathClip = Clip(TEXT("Breath"));
		if (Model)
		{
			Body->SetSkeletalMeshAsset(Model);
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Body->SetRelativeScale3D(FVector(Look.Scale));
			Body->SetRelativeLocation(-Look.Saddle * Look.Scale); // the saddle is at the actor's origin: the actor leans, the dragon hangs from the seat
		}
		else
		{
			Body->SetVisibility(false);
		}
	}
	AttachToRider();
}

void AShinobiDragonMount::OnRep_Rider()
{
	AttachToRider();
}

void AShinobiDragonMount::AttachToRider()
{
	if (!IsValid(Rider) || GetAttachParentActor() == Rider)
	{
		return;
	}
	// On the rider's mesh, which the movement component smooths on clients. The transform cancels the mesh's DEFAULT offset from the
	// capsule (not its current one, which includes the smoothing nudge of the moment) so the saddle is just under the capsule's centre.
	const FTransform MeshDefault(Rider->GetBaseRotationOffset(), Rider->GetBaseTranslationOffset());
	// The actor sits at the seat (just under the capsule's centre) and the dragon's body hangs off it by the saddle offset, so banking the actor
	// leans the dragon about the seat, and the rider stays on it.
	MeshDefaultInverse = MeshDefault.Inverse();
	AttachToComponent(Rider->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeTransform(FTransform(FRotator(BankPitch, 0.f, BankRoll), FVector(0.f, 0.f, -SeatBelowCapsuleCentre)) * MeshDefaultInverse);
}

void AShinobiDragonMount::UpdateBank(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f || GetAttachParentActor() != Rider)
	{
		return;
	}
	const float Yaw = Rider->GetActorRotation().Yaw;
	const float YawRate = bHaveRiderYaw ? FRotator::NormalizeAxis(Yaw - LastRiderYaw) / DeltaSeconds : 0.f; // degrees a second, positive turning right
	LastRiderYaw = Yaw;
	bHaveRiderYaw = true;
	const float TargetRoll = FMath::Clamp(YawRate * BankPerYawRate, -MaxBank, MaxBank);
	const float TargetPitch = FMath::Clamp(Rider->GetVelocity().Z / UShinobiMovementComponent::FlightSpeed * MaxPitch, -MaxPitch, MaxPitch);
	BankRoll = FMath::FInterpTo(BankRoll, TargetRoll, DeltaSeconds, BankSpeed);
	BankPitch = FMath::FInterpTo(BankPitch, TargetPitch, DeltaSeconds, BankSpeed);
	SetActorRelativeTransform(FTransform(FRotator(BankPitch, 0.f, BankRoll), FVector(0.f, 0.f, -SeatBelowCapsuleCentre)) * MeshDefaultInverse);
}

void AShinobiDragonMount::Play(UAnimSequence* Clip, bool bLoop)
{
	if (Clip && Clip != Playing)
	{
		Playing = Clip;
		Body->PlayAnimation(Clip, bLoop);
	}
}

void AShinobiDragonMount::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AttachToRider(); // a no-op once attached
	if (!IsValid(Rider))
	{
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	if (HasAuthority() && !Rider->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Mounted))
	{
		Destroy(); // dismounted, downed, dead or timed out
		return;
	}
	if (!IsNetMode(NM_DedicatedServer))
	{
		UpdateBank(DeltaSeconds);
	}
	// Breathing fire rears the head back and thrusts it forward (once per breath); otherwise hover, beat the wings to climb or move
	// slowly, and glide when fast and level.
	if (Rider->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_DragonBreathing) && BreathClip)
	{
		Play(BreathClip, false);
		return;
	}
	const FVector Velocity = Rider->GetVelocity();
	const bool bGliding = Velocity.Size2D() > GlideSpeed && FMath::Abs(Velocity.Z) < GlideClimb;
	const bool bHovering = Velocity.Size() < HoverSpeed;
	Play(bGliding ? GlideClip.Get() : bHovering ? IdleClip.Get() : FlapClip.Get(), true);
}

FVector AShinobiDragonMount::GetMouthLocation() const
{
	return IsValid(Rider) ? MouthLocation(*Rider, Kind) : GetActorLocation();
}

FVector AShinobiDragonMount::BreathDirection(const AShinobiCharacterBase& Rider)
{
	const FVector Ahead = Rider.GetActorForwardVector().GetSafeNormal2D();
	return FRotator(-BreathPitch, (Ahead.IsNearlyZero() ? FVector::ForwardVector : Ahead).Rotation().Yaw, 0.f).Vector();
}

FVector AShinobiDragonMount::BreathTarget(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind)
{
	const FVector Mouth = MouthLocation(Rider, Kind);
	const FVector End = Mouth + BreathDirection(Rider) * BreathRange;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BreathTarget), false, &Rider);
	return Rider.GetWorld() && Rider.GetWorld()->LineTraceSingleByChannel(Hit, Mouth, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

FVector AShinobiDragonMount::MouthLocation(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind)
{
	const FDragonLook& Look = DragonLookFor(Kind);
	// Where the head is, from the rider: the same on every machine (a dedicated server has no skeleton to ask), so the breath's cone
	// and its flames agree everywhere.
	return Rider.GetActorLocation() + Rider.GetActorForwardVector() * Look.MouthAhead + FVector(0.f, 0.f, Look.MouthHeight);
}
