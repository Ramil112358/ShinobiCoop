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
#include "Shinobi/ShinobiCrossfadeAnimInstance.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
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
		float GroundClearance;   // the rider's lowest above the ground (UShinobiMovementComponent::MountedGroundClearance): the body and legs hang below the seat
		float ArmLength;         // the camera boom while riding (AShinobiPlayerCharacter::MountedArmLength): the wings in view
		const TCHAR* SaddleBone; // the spine bone the saddle rides on (nearest it in the flying pose)
		float StandHeight;       // the seat above the feet standing, model centimetres
		float WalkClipSpeed;     // how fast the Walk clip carries the body at rate 1, model centimetres a second
		float WalkSpeed;         // on the ground (UShinobiMovementComponent::MountedWalkSpeed); the Walk clip is sped up to match
		float RunClipSpeed;      // the Run clip (a trot) at rate 1, model centimetres a second
		float RunSpeed;          // on the ground holding Shift (UShinobiMovementComponent::MountedRunSpeed)
		float TurnRate;          // degrees a second on the ground (UShinobiMovementComponent::MountedTurnRate): the bigger, the slower
		int32 RoomParts;         // the boxes round the model about the seat over every clip (body, wings, tail; or one round it all),
		FVector Room[3][2];      // lowest and highest corners, model centimetres (Y flipped from Blender)
	};

	// The mouths and necks were measured on the meshes in Blender (the crimson rest pose has its head turned, hence the mouth's Y); the
	// saddle bones, standing heights, walk speeds and rooms are printed by each build script ("[build_dragon] saddle bone", "walk", "room").
	const FDragonLook& DragonLookFor(EShinobiDragonKind Kind)
	{
		static const FDragonLook Tairn = { TEXT("Tairn"), 14.f, FVector(18.f, 0.f, 37.f), 784.f, -196.f, // his long neck holds the head low
			TEXT("bone_9"), FVector(50.f, 0.f, 56.f), TEXT("bone_4"), TEXT("bone_5"), 0.7f, 5.7f, -3.6f, 965.f, 2100.f, // ~21 m long, 23 m across the wings
			TEXT("bone_2"), 34.5f, 19.43f, 350.f, 50.53f, 700.f, 75.f, 3, {
				{ FVector(-32.f, -17.4f, -45.f), FVector(59.4f, 15.9f, 35.9f) },      // body, head, neck and legs
				{ FVector(-30.5f, -87.6f, -59.1f), FVector(26.1f, 87.6f, 57.9f) },    // wings
				{ FVector(-96.8f, -16.f, -37.6f), FVector(-16.9f, 15.f, -10.7f) } } }; // tail and club
		static const FDragonLook Crimson = { TEXT("CrimsonDragon"), 3.5f, FVector(22.f, 0.f, 50.f), 247.f, -117.f, // its head reaches forward, below the seat
			TEXT("Head_5"), FVector(68.3f, -21.f, 63.1f), TEXT("Head_1"), TEXT("Head_2"), 0.3f, 7.5f, 0.f, 310.f, 900.f,
			TEXT("Spine_1"), 50.f, 25.45f, 200.f, 66.16f, 400.f, 140.f, 1, { { FVector(-169.7f, -62.6f, -60.5f), FVector(74.1f, 77.f, 34.2f) } } };
		return Kind == EShinobiDragonKind::Tairn ? Tairn : Crimson;
	}

	// The rider kneels on the back (AM_Mount_Sit, a seiza posed on the floor), so the back is at the capsule's floor.
	constexpr float SeatBelowCapsuleCentre = 96.f;
	constexpr float BankPerYawRate = 0.12f; // degrees of roll per degree a second of turn: a 300 degree a second turn banks 36 degrees
	constexpr float MaxBank = 35.f;
	constexpr float GroundBankPerYawRate = 0.06f; // on the ground: a slight lean into the turn (Tairn's 75 degrees a second leans 4.5)
	constexpr float MaxGroundBank = 6.f;
	constexpr float MaxPitch = 25.f;        // at full flight speed straight up or down
	constexpr float ForwardTilt = 8.f;      // nose down at full flight speed ahead: leaning into it
	constexpr float BackwardTilt = 15.f;    // nose up flying backwards: back-winging, the belly ahead
	constexpr float SideTilt = 12.f;        // the wing on the side it slides to drops
	constexpr float BankSpeed = 4.f;        // how quickly the lean follows
	constexpr float HoverSpeed = 150.f;
	constexpr float GlideSinkSpeed = 200.f; // sinking faster than this, the wings hold out instead of beating
	constexpr float OnGroundSlack = 60.f;   // the seat within this of its standing height: on the ground
	constexpr float FullPullYawRate = 35.f; // a turn this fast (degrees a second) draws its rein all the way back
	constexpr float ClimbPull = 0.7f;       // both reins' pull climbing at full pitch (eased forward diving)
	constexpr float BracePull = 0.5f;       // both reins' pull breathing fire
	constexpr float LeanPerBank = 1.5f;     // the rider leans into a turn past the dragon's bank (35 degrees: 52, 17 more than its back)
	constexpr float LeanPerPitch = 1.5f;    // and forward past its dive, back past its climb (25 degrees: 37, 12 more than its back)
	constexpr float BraceLean = 30.f;       // degrees forward breathing fire
	constexpr float RiderPoseSpeed = 8.f;   // how quickly the rider's pull and brace follow
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
		for (const TCHAR* Name : { TEXT("Idle"), TEXT("Flap"), TEXT("Glide"), TEXT("Breath"), TEXT("BreathStand"), TEXT("Stand"), TEXT("Walk"), TEXT("Run"), TEXT("Land"), TEXT("TakeOff") })
		{
			Clips.Add(Name, Clip(Name));
		}
		if (Model)
		{
			Body->SetSkeletalMeshAsset(Model);
			Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Body->SetAnimInstanceClass(UShinobiCrossfadeAnimInstance::StaticClass()); // the clips fade into each other
			Body->SetRelativeScale3D(FVector(Look.Scale));
			Body->SetRelativeLocation(-Look.Saddle * Look.Scale); // the saddle is at the actor's origin: the actor leans, the dragon hangs from the seat
			const FReferenceSkeleton& Bones = Model->GetRefSkeleton();
			const int32 Skull = Bones.FindBoneIndex(Look.SkullBone);
			if (Skull != INDEX_NONE)
			{
				MouthInSkull = FAnimationRuntime::GetComponentSpaceTransformRefPose(Bones, Skull).InverseTransformPosition(Look.MouthRest);
			}
			// The saddle in its bone's frame, from the first frame of Idle (the flying pose, where the saddle was measured).
			const UAnimSequence* Idle = Clips.FindRef(TEXT("Idle"));
			const int32 Saddle = Bones.FindBoneIndex(Look.SaddleBone);
			if (Idle && Idle->GetSkeleton() && Saddle != INDEX_NONE)
			{
				FTransform Flying = FTransform::Identity;
				for (int32 Index = Saddle; Index != INDEX_NONE; Index = Bones.GetParentIndex(Index))
				{
					FTransform Local;
					const int32 SkeletonIndex = Idle->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bones.GetBoneName(Index));
					Idle->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonIndex), FAnimExtractContext(0.0), false);
					Flying = Flying * Local;
				}
				SaddleBone = Saddle;
				SaddleInBone = Flying.InverseTransformPosition(Look.Saddle);
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
	const FDragonLook& Look = DragonLookFor(Kind); // on every machine: the server and the rider's client must agree on the clearance
	UShinobiMovementComponent* Movement = Rider->GetCharacterMovement<UShinobiMovementComponent>();
	Movement->MountedGroundClearance = Look.GroundClearance;
	Movement->MountedStandHeight = Look.StandHeight * Look.Scale;
	Movement->MountedWalkSpeed = Look.WalkSpeed;
	Movement->MountedRunSpeed = Look.RunSpeed;
	Movement->MountedTurnRate = Look.TurnRate;
	Movement->MountedRoomStanding = Room(Kind, false);
	Movement->MountedRoomFlying = Room(Kind, true);
	if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Rider))
	{
		Player->MountedArmLength = Look.ArmLength;
	}
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
	YawRate = bHaveRiderYaw ? FRotator::NormalizeAxis(Yaw - LastRiderYaw) / DeltaSeconds : 0.f; // degrees a second, positive turning right
	LastRiderYaw = Yaw;
	bHaveRiderYaw = true;
	const bool bFlying = Gait == EGait::Fly; // landing and taking off: level; on the ground: a slight lean (the standing room allows it)
	float TargetRoll = bFlying ? FMath::Clamp(YawRate * BankPerYawRate, -MaxBank, MaxBank)
		: Gait == EGait::Ground ? FMath::Clamp(YawRate * GroundBankPerYawRate, -MaxGroundBank, MaxGroundBank) : 0.f;
	// Leaning with its flight: nose up climbing and down diving; nose down flying ahead and up flying backwards; rolled toward a side it
	// slides to (on top of the bank into a turn).
	const FVector Velocity = Rider->GetVelocity() / UShinobiMovementComponent::FlightSpeed;
	const FVector Ahead = Rider->GetActorForwardVector().GetSafeNormal2D();
	const float Forward = Velocity | Ahead;
	const float Sideways = Velocity | FVector::CrossProduct(FVector::UpVector, Ahead);
	const float TargetPitch = bFlying ? FMath::Clamp(Velocity.Z * MaxPitch - Forward * (Forward > 0.f ? ForwardTilt : BackwardTilt), -MaxPitch, MaxPitch) : 0.f;
	TargetRoll = bFlying ? FMath::Clamp(TargetRoll + Sideways * SideTilt, -MaxBank, MaxBank) : TargetRoll;
	BankRoll = FMath::FInterpTo(BankRoll, TargetRoll, DeltaSeconds, BankSpeed);
	BankPitch = FMath::FInterpTo(BankPitch, TargetPitch, DeltaSeconds, BankSpeed);
	SetActorRelativeTransform(FTransform(FRotator(BankPitch, 0.f, BankRoll), FVector(0.f, 0.f, -SeatBelowCapsuleCentre)) * MeshDefaultInverse);
}

void AShinobiDragonMount::Play(UAnimSequence* Clip, bool bLoop, float Rate, float BlendSeconds)
{
	if (UShinobiCrossfadeAnimInstance* Anim = Cast<UShinobiCrossfadeAnimInstance>(Body->GetAnimInstance()); Anim && Clip)
	{
		Anim->Play(Clip, bLoop, Rate, BlendSeconds);
	}
}

void AShinobiDragonMount::UpdateClip()
{
	const UShinobiCrossfadeAnimInstance* Anim = Cast<UShinobiCrossfadeAnimInstance>(Body->GetAnimInstance());
	if (!Anim)
	{
		return;
	}
	const FDragonLook& Look = DragonLookFor(Kind);
	const float StandHeight = Look.StandHeight * Look.Scale;
	const FVector Seat = Rider->GetActorLocation() - FVector(0.f, 0.f, Rider->GetSimpleCollisionHalfHeight());
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonGait), false, Rider);
	const float Below = GetWorld()->LineTraceSingleByChannel(Hit, Seat, Seat - FVector(0.f, 0.f, Look.GroundClearance * 2.f), ECC_WorldStatic, Params) ? Hit.Distance : UE_BIG_NUMBER;
	const FVector Velocity = Rider->GetVelocity();
	const bool bOnGround = Below < StandHeight + OnGroundSlack;
	const bool bNearGround = Below < Look.GroundClearance;
	switch (Gait)
	{
	case EGait::Fly:
		Gait = bOnGround ? EGait::Ground : bNearGround && Velocity.Z < -10.f ? EGait::Land : EGait::Fly;
		break;
	case EGait::Land:
		Gait = Velocity.Z > 10.f && !bOnGround ? EGait::TakeOff : !bNearGround ? EGait::Fly : bOnGround && Anim->IsFinished() ? EGait::Ground : EGait::Land;
		break;
	case EGait::Ground:
		Gait = bOnGround ? EGait::Ground : Velocity.Z > 10.f ? EGait::TakeOff : !bNearGround ? EGait::Fly : EGait::Ground; // up, or off an edge
		break;
	case EGait::TakeOff:
		Gait = !Anim->IsFinished() ? EGait::TakeOff : bOnGround ? EGait::Ground : EGait::Fly;
		break;
	}
	auto Get = [this](const TCHAR* Name) { return Clips.FindRef(Name).Get(); };
	// Breathing fire rears the head back and thrusts it forward (once per breath); standing, the feet stay on the ground.
	if (Rider->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_DragonBreathing))
	{
		Play(Gait == EGait::Ground && Get(TEXT("BreathStand")) ? Get(TEXT("BreathStand")) : Get(TEXT("Breath")), false, BreathPlayRate, 0.15f);
		return;
	}
	const float GroundSpeed = Velocity.Size2D();
	switch (Gait)
	{
	case EGait::Land:
		Play(Get(TEXT("Land")), false, 1.f, 0.25f);
		break;
	case EGait::TakeOff:
		Play(Get(TEXT("TakeOff")), false, 1.f, 0.25f);
		break;
	case EGait::Ground:
		if (GroundSpeed > (Look.WalkSpeed + Look.RunSpeed) * 0.5f && Get(TEXT("Run")))
		{
			Play(Get(TEXT("Run")), true, FMath::Clamp(GroundSpeed / (Look.RunClipSpeed * Look.Scale), 0.3f, 3.f), 0.25f); // Shift: a trot
		}
		else if (GroundSpeed > 30.f && Look.WalkClipSpeed > 0.f)
		{
			Play(Get(TEXT("Walk")), true, FMath::Clamp(GroundSpeed / (Look.WalkClipSpeed * Look.Scale), 0.3f, 3.f)); // the feet keep pace with the ground
		}
		else if (FMath::Abs(YawRate) > 15.f && Look.TurnRate > 0.f)
		{
			Play(Get(TEXT("Walk")), true, FMath::Clamp(FMath::Abs(YawRate) / Look.TurnRate, 0.4f, 1.f)); // turning on the spot: it steps round
		}
		else
		{
			Play(Get(TEXT("Stand")), true);
		}
		break;
	default:
		// Hover with slow beats; beat the wings hard when moving (climbing, diving or flying level), hold them out when sinking.
		Play(Velocity.Size() < HoverSpeed ? Get(TEXT("Idle")) : Velocity.Z < -GlideSinkSpeed ? Get(TEXT("Glide")) : Get(TEXT("Flap")), true);
		break;
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
		UpdateClip();
		UpdateBank(DeltaSeconds);
		UpdateRiderPose(DeltaSeconds);
		KeepSeatStill();
		UpdateReins();
	}
}

void AShinobiDragonMount::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Rider))
	{
		if (UShinobiBodyComponent* Hero = Cast<UShinobiBodyComponent>(Rider->GetBodyMesh()))
		{
			Hero->SetRidePose(FShinobiRidePose()); // back to sitting plain
		}
	}
	Super::EndPlay(Reason);
}

void AShinobiDragonMount::UpdateRiderPose(float DeltaSeconds)
{
	UShinobiBodyComponent* Hero = DeltaSeconds > 0.f ? Cast<UShinobiBodyComponent>(Rider->GetBodyMesh()) : nullptr;
	if (!Hero)
	{
		return;
	}
	const bool bBreathing = Rider->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_DragonBreathing);
	Brace = FMath::FInterpTo(Brace, bBreathing ? 1.f : 0.f, DeltaSeconds, RiderPoseSpeed);
	const float Turn = FMath::Clamp(YawRate / FullPullYawRate, -1.f, 1.f);
	const float Both = BankPitch / MaxPitch * ClimbPull + Brace * BracePull;
	const float Target[2] = { FMath::Max(-Turn, 0.f) + Both, FMath::Max(Turn, 0.f) + Both }; // a right turn draws the right rein
	FShinobiRidePose Pose;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		ReinPull[Side] = FMath::FInterpTo(ReinPull[Side], FMath::Clamp(Target[Side], -1.f, 1.f), DeltaSeconds, RiderPoseSpeed);
		Pose.Pull[Side] = ReinPull[Side];
	}
	Pose.Roll = BankRoll * LeanPerBank;
	Pose.Forward = -BankPitch * LeanPerPitch + Brace * BraceLean; // nose up leans back, down leans forward
	Hero->SetRidePose(Pose);
}

void AShinobiDragonMount::KeepSeatStill()
{
	// The pose read is last frame's, a frame behind: too little to see.
	const TArray<FTransform>& Pose = Body->GetComponentSpaceTransforms();
	if (Pose.IsValidIndex(SaddleBone))
	{
		Body->SetRelativeLocation(-Pose[SaddleBone].TransformPosition(SaddleInBone) * DragonLookFor(Kind).Scale);
	}
}

TArray<FBox> AShinobiDragonMount::Room(EShinobiDragonKind Kind, bool bFlying)
{
	const FDragonLook& Look = DragonLookFor(Kind);
	TArray<FBox> Boxes;
	for (int32 Part = 0; Part < Look.RoomParts; ++Part)
	{
		const FBox Model(Look.Room[Part][0] * Look.Scale, Look.Room[Part][1] * Look.Scale);
		FBox Box(ForceInit);
		for (const float Pitch : { -MaxPitch, 0.f, MaxPitch })
		{
			for (const float Roll : { -MaxBank, -MaxGroundBank, 0.f, MaxGroundBank, MaxBank })
			{
				// UpdateBank leans the dragon about the seat: far in flight, a little on the ground
				if (bFlying ? FMath::Abs(Roll) != MaxGroundBank : Pitch == 0.f && FMath::Abs(Roll) != MaxBank)
				{
					Box += Model.TransformBy(FTransform(FRotator(Pitch, 0.f, Roll)));
				}
			}
		}
		Box.Min.Z = FMath::Max(Box.Min.Z, UShinobiMovementComponent::MountedStepClearance - Look.StandHeight * Look.Scale);
		Boxes.Add(Box);
	}
	return Boxes;
}

bool AShinobiDragonMount::HasRoom(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind)
{
	const UWorld* World = Rider.GetWorld();
	if (!World)
	{
		return false;
	}
	const FDragonLook& Look = DragonLookFor(Kind);
	const float StandHeight = Look.StandHeight * Look.Scale;
	const FVector Feet = Rider.GetActorLocation() - FVector(0.f, 0.f, Rider.GetSimpleCollisionHalfHeight());
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonSummonRoom), false, &Rider);
	// Summoned over the ground (or a roof) it lands there, the rider on its back; with nothing near below it flies where the rider is.
	const bool bStanding = World->LineTraceSingleByChannel(Ground, Feet + FVector(0.f, 0.f, 10.f), Feet - FVector(0.f, 0.f, Look.GroundClearance), ECC_WorldStatic, Params);
	const FVector Seat = bStanding ? Ground.ImpactPoint + FVector(0.f, 0.f, StandHeight) : Feet;
	FHitResult Hit;
	return !UShinobiMovementComponent::FindRoomHit(*World, Room(Kind, !bStanding), Seat, Seat, FRotator(0.f, Rider.GetActorRotation().Yaw, 0.f).Quaternion(),
		bStanding ? StandHeight : UE_BIG_NUMBER, &Rider, Hit);
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
	const bool bSideIsRight = (Side | Body->GetRightVector()) > 0.f;
	const FVector Centre = FMath::Lerp(From, To, Look.CollarAt) - Up * (Look.CollarDrop * Look.Scale);
	const float Radius = Look.CollarRadius * Look.Scale;
	auto Ring = [&](int32 Index) { const float Angle = 2.f * UE_PI * Index / CollarSegments; return Centre + (Up * FMath::Cos(Angle) + Side * FMath::Sin(Angle)) * Radius; };
	for (int32 Index = 0; Index < CollarSegments; ++Index)
	{
		LayRope(Rope[Index], Ring(Index), Ring(Index + 1));
	}
	// Each rein runs from its own side of the collar, one point off the top, to its fist. (Not the nearest point: the neck's sway moved the
	// nearest one back and forth between two points and the rein jumped a metre with it.)
	for (int32 Hand = 0; Hand < 2; ++Hand)
	{
		const bool bRight = Hand == 1;
		const int32 Anchor = bRight == bSideIsRight ? 1 : CollarSegments - 1;
		LayRope(Rope[CollarSegments + Hand], Ring(Anchor), Fist(*Hands, Hand == 0));
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
