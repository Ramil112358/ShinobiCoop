#include "Shinobi/ShinobiDragonMount.h"
#include "AbilitySystemComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	// Both models (Scripts/character/build_dragon.py for Tairn, build_dragon_crimson.py for the crimson one; each a Tripo Studio model and
	// rig) face +X with their feet on the ground in their rest pose; every flying clip lays them out level (necks reached forward, tails
	// stretched out behind). Their backs in that flying pose, between the
	// wings (the torso's top without the spikes), are at the Saddle points below (model centimetres, before the scale; the preview run of
	// each script prints them, and where the snout is); the scale makes them dragon-sized and the saddle goes where a seated rider's hips
	// are. Tairn is among the biggest dragons in the books, so he is the bigger.
	struct FDragonLook
	{
		const TCHAR* Name;       // /Game/Shinobi/Creatures/<Name>/SK_<Name> and ANIM_<Name>_<Clip>
		float Scale;
		FVector Saddle;
		float MouthAhead;        // the mouth in the breath's open-jawed pose, from the seated rider's capsule centre
		float MouthHeight;
		const TCHAR* SkullBone;  // the head bone the upper jaw rides on
		FVector MouthRest;       // the mouth (between the jaws, behind the snout tip) in the reference pose, model centimetres as UE has them
		const TCHAR* CollarBone; // the reins' collar goes round the neck CollarAt of the way from this joint to the next
		const TCHAR* CollarNext;
		float CollarAt;
		float CollarRadius;      // the neck's thickness there, model centimetres
		float CollarDrop;        // how far below the joint the neck's middle is there (negative: Tairn's bones run under his neck), model centimetres
	};

	// The mouths and necks were measured on the meshes in Blender (the crimson rest pose has its head turned, hence the mouth's Y).
	const FDragonLook& DragonLookFor(EShinobiDragonKind Kind)
	{
		static const FDragonLook Tairn = { TEXT("Tairn"), 4.5f, FVector(18.f, 0.f, 37.f), 252.f, -128.f, // his long neck holds the head low
			TEXT("bone_9"), FVector(50.f, 0.f, 56.f), TEXT("bone_4"), TEXT("bone_5"), 0.7f, 5.7f, -3.6f };
		static const FDragonLook Crimson = { TEXT("CrimsonDragon"), 3.5f, FVector(22.f, 0.f, 50.f), 247.f, -117.f, // its head reaches forward, below the seat
			TEXT("Head_5"), FVector(68.3f, -21.f, 63.1f), TEXT("Head_1"), TEXT("Head_2"), 0.3f, 7.5f, 0.f };
		return Kind == EShinobiDragonKind::Tairn ? Tairn : Crimson;
	}

	// The rider kneels on the back (AM_Mount_Sit, a seiza posed on the floor), so the back is at the capsule's floor.
	constexpr float SeatBelowCapsuleCentre = 96.f;
	constexpr float BankPerYawRate = 0.12f; // degrees of roll per degree a second of turn: a 300 degree a second turn banks 36 degrees
	constexpr float MaxBank = 35.f;
	constexpr float MaxPitch = 25.f;        // at full flight speed straight up or down
	constexpr float BankSpeed = 4.f;        // how quickly the lean follows
	constexpr float HoverSpeed = 150.f;
	constexpr int32 CollarSegments = 14;
	constexpr float RopeThickness = 3.f;
	const FLinearColor RopeColour(0.2f, 0.12f, 0.06f); // weathered hemp

	/** Lays an engine cylinder (100 cm tall and across, centred) as a rope from A to B. */
	void LayRope(UStaticMeshComponent* Piece, const FVector& A, const FVector& B)
	{
		Piece->SetWorldLocationAndRotation((A + B) * 0.5f, FRotationMatrix::MakeFromZ(B - A).Rotator());
		Piece->SetWorldScale3D(FVector(RopeThickness / 100.f, RopeThickness / 100.f, FVector::Dist(A, B) / 100.f));
	}

	/** Where a fist closes: the middle knuckle on a hero, else the hand. */
	FVector Fist(const USkeletalMeshComponent& Mesh, bool bLeft)
	{
		for (const TCHAR* Bone : bLeft ? std::initializer_list<const TCHAR*>{ TEXT("LeftHandMiddle1"), TEXT("LeftHand"), TEXT("hand_l") }
		                               : std::initializer_list<const TCHAR*>{ TEXT("RightHandMiddle1"), TEXT("RightHand"), TEXT("hand_r") })
		{
			if (Mesh.GetBoneIndex(Bone) != INDEX_NONE)
			{
				return Mesh.GetBoneLocation(Bone);
			}
		}
		return Mesh.GetComponentLocation();
	}
}

AShinobiDragonMount::AShinobiDragonMount()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the rider has moved and both have been posed: the reins meet the fists this frame
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
		BreathClip = Clip(TEXT("Breath"));
		if (Model)
		{
			Body->SetSkeletalMeshAsset(Model);
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Body->SetRelativeScale3D(FVector(Look.Scale));
			Body->SetRelativeLocation(-Look.Saddle * Look.Scale); // the saddle is at the actor's origin: the actor leans, the dragon hangs from the seat
			const int32 Skull = Model->GetRefSkeleton().FindBoneIndex(Look.SkullBone);
			if (Skull != INDEX_NONE)
			{
				MouthInSkull = FAnimationRuntime::GetComponentSpaceTransformRefPose(Model->GetRefSkeleton(), Skull).InverseTransformPosition(Look.MouthRest);
			}
			UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
			UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			if (Cylinder && Basic)
			{
				// ponytail: straight engine cylinders; a rope mesh or a cable when the reins need to sag
				UMaterialInstanceDynamic* Hemp = UMaterialInstanceDynamic::Create(Basic, this);
				Hemp->SetVectorParameterValue(TEXT("Color"), RopeColour);
				for (int32 Index = 0; Index < CollarSegments + 2; ++Index)
				{
					UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
					Piece->SetStaticMesh(Cylinder);
					Piece->SetMaterial(0, Hemp);
					Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Piece->SetupAttachment(GetRootComponent());
					Piece->SetUsingAbsoluteLocation(true);
					Piece->SetUsingAbsoluteRotation(true);
					Piece->SetUsingAbsoluteScale(true);
					Piece->RegisterComponent();
					Rope.Add(Piece);
				}
			}
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
		Body->SetPlayRate(Clip == BreathClip ? BreathPlayRate : 1.f);
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
		KeepSeatStill();
		UpdateReins();
	}
	// Breathing fire rears the head back and thrusts it forward (once per breath); otherwise hover with slow beats, and beat the wings
	// hard whenever moving: climbing, diving or flying level.
	if (Rider->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_DragonBreathing) && BreathClip)
	{
		Play(BreathClip, false);
		return;
	}
	Play(Rider->GetVelocity().Size() < HoverSpeed ? IdleClip.Get() : FlapClip.Get(), true);
}

void AShinobiDragonMount::KeepSeatStill()
{
	// Every clip bobs the whole dragon by moving its root bone (14 cm on an idle beat, 42 cm on a hard one) and is unbobbed at its first
	// frame, where the saddle was measured. The rider doesn't bob, so hang the body that much the other way: the back stays under the seat.
	// The pose read is last frame's, a frame behind: too little to see.
	const TArray<FTransform>& Pose = Body->GetComponentSpaceTransforms();
	const int32 Bone = Body->GetBoneIndex(TEXT("Root"));
	if (!Playing || !Body->GetSkeletalMeshAsset() || !Pose.IsValidIndex(Bone))
	{
		return;
	}
	const int32 Parent = Body->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(Bone);
	const int32 SkeletonBone = Playing->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Body->GetBoneName(Bone));
	FTransform First;
	Playing->GetBoneTransform(First, FSkeletonPoseBoneIndex(SkeletonBone), FAnimExtractContext(0.0), false);
	const FVector Unbobbed = (Pose.IsValidIndex(Parent) ? First * Pose[Parent] : First).GetLocation();
	const FDragonLook& Look = DragonLookFor(Kind);
	Body->SetRelativeLocation((-Look.Saddle - (Pose[Bone].GetLocation() - Unbobbed)) * Look.Scale);
}

void AShinobiDragonMount::UpdateReins()
{
	const FDragonLook& Look = DragonLookFor(Kind);
	const USkeletalMeshComponent* Hands = IsValid(Rider) ? Rider->GetBodyMesh() : nullptr;
	if (Rope.Num() != CollarSegments + 2 || !Hands || Body->GetBoneIndex(Look.CollarBone) == INDEX_NONE || Body->GetBoneIndex(Look.CollarNext) == INDEX_NONE)
	{
		return;
	}
	// A ring square to the neck, its first point on top (the dragon's up, as far as the neck's slope allows).
	const FVector From = Body->GetBoneLocation(Look.CollarBone);
	const FVector To = Body->GetBoneLocation(Look.CollarNext);
	const FVector Axis = (To - From).GetSafeNormal();
	const FVector Up = (Body->GetUpVector() - Axis * (Body->GetUpVector() | Axis)).GetSafeNormal();
	const FVector Side = Axis ^ Up;
	const FVector Centre = FMath::Lerp(From, To, Look.CollarAt) - Up * (Look.CollarDrop * Look.Scale);
	const float Radius = Look.CollarRadius * Look.Scale;
	auto Ring = [&](int32 Index) { const float Angle = 2.f * UE_PI * Index / CollarSegments; return Centre + (Up * FMath::Cos(Angle) + Side * FMath::Sin(Angle)) * Radius; };
	for (int32 Index = 0; Index < CollarSegments; ++Index)
	{
		LayRope(Rope[Index], Ring(Index), Ring(Index + 1));
	}
	// Each rein runs from the collar's nearest point to its fist.
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const FVector Grip = Fist(*Hands, Hand == 0);
		int32 Nearest = 0;
		for (int32 Index = 1; Index < CollarSegments; ++Index)
		{
			Nearest = FVector::DistSquared(Ring(Index), Grip) < FVector::DistSquared(Ring(Nearest), Grip) ? Index : Nearest;
		}
		LayRope(Rope[CollarSegments + Hand], Ring(Nearest), Grip);
	}
}

FVector AShinobiDragonMount::GetMouthLocation() const
{
	const int32 Skull = Body->GetBoneIndex(DragonLookFor(Kind).SkullBone);
	if (Skull != INDEX_NONE && !MouthInSkull.IsZero())
	{
		return Body->GetBoneTransform(Skull).TransformPosition(MouthInSkull);
	}
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
