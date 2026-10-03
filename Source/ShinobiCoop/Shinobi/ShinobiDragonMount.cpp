#include "Shinobi/ShinobiDragonMount.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	// The model (Scripts/character/build_dragon.py, from a Rodin generation) is 1.9 m long in its rest pose, centred on the origin with its
	// feet on the ground, facing +X; every flying clip tips it nose-down about its hips. Its back in that flying pose, between the wings
	// (the torso's top without the spikes), is at about (0.3, 0, 0.72) m. Scaled up, the saddle goes where a seated rider's hips are.
	constexpr float DragonScale = 3.5f;
	const FVector Saddle(30.f, 0.f, 72.f);
	constexpr float SeatBelowCapsuleCentre = 25.f; // how far under the rider's capsule centre the saddle sits
	constexpr float MouthAhead = 330.f; // the head's mouth, ahead of and a little above the seated rider's capsule centre
	constexpr float MouthHeight = 55.f;
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
}

AShinobiDragonMount* AShinobiDragonMount::Spawn(AShinobiCharacterBase* Rider)
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
		Dragon->FinishSpawning(Where);
	}
	return Dragon;
}

void AShinobiDragonMount::BeginPlay()
{
	Super::BeginPlay();
	if (!IsNetMode(NM_DedicatedServer))
	{
		USkeletalMesh* Model = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Shinobi/Creatures/Dragon/SK_Dragon.SK_Dragon"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		IdleClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Shinobi/Creatures/Dragon/ANIM_Dragon_Idle.ANIM_Dragon_Idle"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		FlapClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Shinobi/Creatures/Dragon/ANIM_Dragon_Flap.ANIM_Dragon_Flap"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		GlideClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Shinobi/Creatures/Dragon/ANIM_Dragon_Glide.ANIM_Dragon_Glide"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		BreathClip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Shinobi/Creatures/Dragon/ANIM_Dragon_Breath.ANIM_Dragon_Breath"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Model)
		{
			Body->SetSkeletalMeshAsset(Model);
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Body->SetRelativeScale3D(FVector(DragonScale));
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
	const FTransform Wanted(FRotator::ZeroRotator, FVector(-Saddle.X * DragonScale, 0.f, -SeatBelowCapsuleCentre - Saddle.Z * DragonScale));
	AttachToComponent(Rider->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
	SetActorRelativeTransform(Wanted * MeshDefault.Inverse());
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
	return IsValid(Rider) ? MouthLocation(*Rider) : GetActorLocation();
}

FVector AShinobiDragonMount::MouthLocation(const AShinobiCharacterBase& Rider)
{
	// Where the head is, from the rider: the same on every machine (a dedicated server has no skeleton to ask), so the breath's cone
	// and its flames agree everywhere.
	return Rider.GetActorLocation() + Rider.GetActorForwardVector() * MouthAhead + FVector(0.f, 0.f, MouthHeight);
}
