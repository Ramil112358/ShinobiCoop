#include "Shinobi/Traversal/ShinobiTraversalComponent.h"
#include "ShinobiCoop.h"
#include "AnimationWarpingLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Chooser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "Shinobi/Hero/ShinobiBlueprintStruct.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "TimerManager.h"

/** Returns false, saying why a traversal was not taken (LogShinobiCoop Verbose). */
static bool RejectTraversal(const TCHAR* Why)
{
	UE_LOG(LogShinobiCoop, Verbose, TEXT("Traversal: %s"), Why);
	return false;
}
#define TRAVERSAL_REJECT(Why) RejectTraversal(TEXT(Why))

namespace
{
	// The sample's assets (Scripts/anim/migrate_gasp.py). Loaded in BeginPlay, never from a constructor: see
	// AShinobiCharacterBase::BeginPlay.
	const TCHAR* ChooserPath = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Traversal/CHT_TraversalMontages_CMC.CHT_TraversalMontages_CMC");
	const TCHAR* ChooserInputPath = TEXT("/Game/Blueprints/Data/S_CHT_TraversalMontages_IN.S_CHT_TraversalMontages_IN");
	const TCHAR* ChooserOutputPath = TEXT("/Game/Blueprints/Data/S_CHT_TraversalMontages_OUT.S_CHT_TraversalMontages_OUT");

	const FName FrontLedgeTarget(TEXT("FrontLedge"));
	const FName BackLedgeTarget(TEXT("BackLedge"));
	const FName BackFloorTarget(TEXT("BackFloor"));
	const FName DistanceFromLedgeCurve(TEXT("Distance_From_Ledge"));

	/** Where the warp window toward Target ends, read off the montage's Distance_From_Ledge curve. */
	float AnimatedDistanceFromLedge(const UAnimMontage* Montage, FName Target)
	{
		TArray<FMotionWarpingWindowData> Windows;
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, Target, Windows);
		float Distance = 0.f;
		if (Windows.Num() > 0)
		{
			UAnimationWarpingLibrary::GetCurveValueFromAnimation(Montage, DistanceFromLedgeCurve, Windows[0].EndTime, Distance);
		}
		return Distance;
	}

	bool HasWarpWindow(const UAnimMontage* Montage, FName Target)
	{
		TArray<FMotionWarpingWindowData> Windows;
		UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, Target, Windows);
		return Windows.Num() > 0;
	}

	/** Copies one Blueprint field into another of the same type (the AnimBP's gait into the chooser's input, ...). */
	void CopyField(const FProperty* To, void* ToContainer, const FProperty* From, const void* FromContainer)
	{
		if (To && From && To->SameType(From))
		{
			To->CopyCompleteValue(To->ContainerPtrToValuePtr<void>(ToContainer), From->ContainerPtrToValuePtr<void>(FromContainer));
		}
	}

	/** Calls the AnimBP's Get_PoseHistory and copies its result into Field (the chooser's pose-match column reads it). */
	void CopyPoseHistory(UAnimInstance* AnimInstance, const FProperty* Field, void* Container)
	{
		UFunction* Function = Field ? AnimInstance->FindFunction(TEXT("Get_PoseHistory")) : nullptr;
		if (!Function)
		{
			return;
		}
		uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(Params, Function->ParmsSize);
		Function->InitializeStruct(Params);
		AnimInstance->ProcessEvent(Function, Params);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
			{
				CopyField(Field, Container, *It, Params);
				break;
			}
		}
		Function->DestroyStruct(Params);
	}
}

UShinobiTraversalComponent::UShinobiTraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UShinobiTraversalComponent::BeginPlay()
{
	Super::BeginPlay();
	Chooser = LoadObject<UChooserTable>(nullptr, ChooserPath);
}

bool UShinobiTraversalComponent::TryTraversal()
{
	const ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(Character->GetCharacterMovement());
	if (bTraversing || !Movement || Movement->IsSurfaceTraversing() || !(Movement->IsMovingOnGround() || Movement->IsFalling()))
	{
		return TRAVERSAL_REJECT("already traversing, on a wall or water, or neither walking nor falling");
	}
	FShinobiTraversalAction Action;
	float ObstacleHeight = 0.f, ObstacleDepth = 0.f, BackLedgeHeight = 0.f;
	bool bHasBackLedge = false, bHasBackFloor = false;
	if (!FindLedges(Action, ObstacleHeight, ObstacleDepth, BackLedgeHeight, bHasBackLedge, bHasBackFloor)
		|| !ChooseMontage(Action, ObstacleHeight, ObstacleDepth, BackLedgeHeight, bHasBackLedge, bHasBackFloor))
	{
		return false;
	}
	Perform(Action);
	if (GetOwner()->HasAuthority())
	{
		MulticastPerform(Action);
	}
	else
	{
		ServerPerform(Action);
	}
	return true;
}

bool UShinobiTraversalComponent::FindLedges(FShinobiTraversalAction& Action, float& OutObstacleHeight, float& OutObstacleDepth,
	float& OutBackLedgeHeight, bool& bOutHasBackLedge, bool& bOutHasBackFloor) const
{
	const ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = Character->GetActorLocation();
	const float FeetZ = Location.Z - HalfHeight;
	UWorld* World = GetWorld();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiTraversal), false, Character);
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore); // other characters are never ledges
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FCollisionShape Body = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	auto Sweep = [&](FHitResult& Hit, const FVector& From, const FVector& To)
	{
		return World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, Channel, Body, Params, Response);
	};

	// What is ahead: the sample's reach, longer the faster the character moves. Lifted a little so kerbs and slopes
	// underfoot don't count.
	FVector Forward = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	if (Forward.IsZero())
	{
		Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	}
	const float Reach = Movement->IsFalling() ? 75.f : FMath::GetMappedRangeValueClamped(FVector2f(0.f, 500.f), FVector2f(75.f, 350.f), Movement->Velocity.Size2D());
	const FVector Start = Location + FVector::UpVector * 10.f;
	FHitResult Front;
	if (!World->SweepSingleByChannel(Front, Start, Start + Forward * Reach, FQuat::Identity, Channel,
			FCollisionShape::MakeCapsule(Radius, HalfHeight - 10.f), Params, Response)
		|| !Front.IsValidBlockingHit() || Front.bStartPenetrating || FMath::Abs(Front.ImpactNormal.Z) > 0.3f
		|| (Front.GetActor() && Front.GetActor()->ActorHasTag(UShinobiMovementComponent::NoTraversalTag)))
	{
		UE_LOG(LogShinobiCoop, Verbose, TEXT("Traversal: no wall ahead (from %s along %s for %.0f: hit %d %s normal %s, start penetrating %d)"),
			*Start.ToCompactString(), *Forward.ToCompactString(), Reach, Front.bBlockingHit, *GetNameSafe(Front.GetActor()),
			*Front.ImpactNormal.ToCompactString(), Front.bStartPenetrating);
		return false;
	}
	const FVector FrontNormal = Front.ImpactNormal.GetSafeNormal2D();

	// The top of the obstacle just past its face: a walkable surface within reach of the feet.
	const FVector Probe = Front.ImpactPoint - FrontNormal * 5.f;
	FHitResult Top;
	if (!World->LineTraceSingleByChannel(Top, FVector(Probe.X, Probe.Y, FeetZ + MaxLedgeHeight + 5.f), FVector(Probe.X, Probe.Y, FeetZ),
			Channel, Params, Response)
		|| Top.bStartPenetrating || Top.ImpactNormal.Z < Movement->GetWalkableFloorZ())
	{
		return TRAVERSAL_REJECT("no walkable top within reach");
	}
	Action.FrontLedgeLocation = FVector(Front.ImpactPoint.X, Front.ImpactPoint.Y, Top.ImpactPoint.Z);
	Action.FrontLedgeNormal = FrontNormal;
	Action.Obstacle = Front.GetComponent();

	// The far edge: looking back toward us from the deepest obstacle that still counts as one to go over, just under the top.
	const FVector Far = Action.FrontLedgeLocation - FrontNormal * MaxObstacleDepth - FVector::UpVector * 5.f;
	FHitResult Back;
	bOutHasBackLedge = World->LineTraceSingleByChannel(Back, Far, Action.FrontLedgeLocation - FrontNormal - FVector::UpVector * 5.f, Channel, Params, Response)
		&& !Back.bStartPenetrating && (Back.ImpactNormal.GetSafeNormal2D() | FrontNormal) < -0.5f;
	Action.BackLedgeLocation = bOutHasBackLedge ? FVector(Back.ImpactPoint.X, Back.ImpactPoint.Y, Action.FrontLedgeLocation.Z) : Far + FVector::UpVector * 5.f;
	Action.BackLedgeNormal = bOutHasBackLedge ? Back.ImpactNormal.GetSafeNormal2D() : -FrontNormal;

	// From here on, the sample's room checks (AC_TraversalLogic::TryTraversalAction): room to rise to the front ledge...
	const FVector FrontRoom = Action.FrontLedgeLocation + FrontNormal * (Radius + 2.f) + FVector::UpVector * (HalfHeight + 2.f);
	FHitResult Rise;
	if (Sweep(Rise, Location, FrontRoom))
	{
		return TRAVERSAL_REJECT("no room to rise to the ledge");
	}
	OutObstacleHeight = FMath::Abs(FeetZ - Action.FrontLedgeLocation.Z);

	// ...and to stand on the top (where a mantle ends; a wall that carries on above the ledge fails here).
	const FVector OnTop = Action.FrontLedgeLocation - FrontNormal * (Radius + 2.f) + FVector::UpVector * (HalfHeight + 2.f);
	FHitResult Stand;
	if (Sweep(Stand, OnTop, OnTop))
	{
		return TRAVERSAL_REJECT("no room on top");
	}

	// ...then across the top to the far side, and down to the floor beyond it.
	const FVector BackRoom = Action.BackLedgeLocation + Action.BackLedgeNormal * (Radius + 2.f) + FVector::UpVector * (HalfHeight + 2.f);
	FHitResult Across;
	if (!bOutHasBackLedge || Sweep(Across, FrontRoom, BackRoom))
	{
		OutObstacleDepth = bOutHasBackLedge ? FVector::Dist2D(Across.ImpactPoint, Action.FrontLedgeLocation) : MaxObstacleDepth;
		bOutHasBackLedge = false;
		bOutHasBackFloor = false;
		return true;
	}
	OutObstacleDepth = FVector::Dist2D(Action.FrontLedgeLocation, Action.BackLedgeLocation);
	FHitResult Floor;
	bOutHasBackFloor = Sweep(Floor, BackRoom, Action.BackLedgeLocation + Action.BackLedgeNormal * (Radius + 2.f) - FVector::UpVector * 50.f);
	if (bOutHasBackFloor)
	{
		Action.BackFloorLocation = Floor.ImpactPoint;
		OutBackLedgeHeight = FMath::Abs(Floor.ImpactPoint.Z - Action.BackLedgeLocation.Z);
	}
	return true;
}

bool UShinobiTraversalComponent::ChooseMontage(FShinobiTraversalAction& Action, float ObstacleHeight, float ObstacleDepth,
	float BackLedgeHeight, bool bHasBackLedge, bool bHasBackFloor) const
{
	const ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	const UScriptStruct* InputType = LoadObject<UScriptStruct>(nullptr, ChooserInputPath);
	const UScriptStruct* OutputType = LoadObject<UScriptStruct>(nullptr, ChooserOutputPath);
	if (!Chooser || !AnimInstance || !InputType || !OutputType)
	{
		return TRAVERSAL_REJECT("the sample's chooser assets or the AnimBP are missing");
	}

	using namespace ShinobiBlueprintStruct;
	FInstancedStruct Input(InputType);
	FInstancedStruct Output(OutputType);
	void* In = Input.GetMutableMemory();
	const FProperty* InputAction = Find(InputType, TEXT("ActionType"));
	SetEnum(InputAction, In, EnumValue(InputAction, TEXT("None")));
	SetBool(Find(InputType, TEXT("HasFrontLedge")), In, true);
	SetBool(Find(InputType, TEXT("HasBackLedge")), In, bHasBackLedge);
	SetBool(Find(InputType, TEXT("HasBackFloor")), In, bHasBackFloor);
	SetReal(Find(InputType, TEXT("ObstacleHeight")), In, ObstacleHeight);
	SetReal(Find(InputType, TEXT("ObstacleDepth")), In, ObstacleDepth);
	SetReal(Find(InputType, TEXT("BackLedgeHeight")), In, BackLedgeHeight);
	SetReal(Find(InputType, TEXT("DistanceToLedge")), In, FVector::Dist(Action.FrontLedgeLocation, Character->GetMesh()->GetComponentLocation()));
	SetReal(Find(InputType, TEXT("Speed")), In, Character->GetCharacterMovement()->Velocity.Size2D());
	// Gait and movement mode as the AnimBP sees them (UShinobiLocomotionAnimInstance fills its CharacterProperties).
	if (const FStructProperty* Properties = CastField<FStructProperty>(AnimInstance->GetClass()->FindPropertyByName(TEXT("CharacterProperties"))))
	{
		const void* From = Properties->ContainerPtrToValuePtr<void>(AnimInstance);
		CopyField(Find(InputType, TEXT("Gait")), In, Find(Properties->Struct, TEXT("Gait")), From);
		CopyField(Find(InputType, TEXT("MovementMode")), In, Find(Properties->Struct, TEXT("MovementMode")), From);
	}
	CopyPoseHistory(AnimInstance, Find(InputType, TEXT("PoseHistory")), In);

	// The chooser's context, in the order its table declares: the AnimBP, the inputs, the outputs.
	FChooserEvaluationContext Context(AnimInstance);
	Context.AddStructViewParam(FStructView(InputType, Input.GetMutableMemory()));
	Context.AddStructViewParam(FStructView(OutputType, Output.GetMutableMemory()));
	UChooserTable::EvaluateChooser(Context, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Action](UObject* Result)
	{
		Action.Montage = Cast<UAnimMontage>(Result);
		return Action.Montage ? FObjectChooserBase::EIteratorStatus::Stop : FObjectChooserBase::EIteratorStatus::Failed;
	}));

	const FProperty* OutputAction = Find(OutputType, TEXT("ActionType"));
	const FString ActionName = GetEnumDisplayName(OutputAction, Output.GetMemory());
	UE_LOG(LogShinobiCoop, Verbose, TEXT("Traversal: height %.0f depth %.0f back ledge %d floor %d (%.0f) -> %s %s"), ObstacleHeight, ObstacleDepth,
		bHasBackLedge, bHasBackFloor, BackLedgeHeight, *ActionName, *GetNameSafe(Action.Montage));
	if (!Action.Montage || ActionName.IsEmpty() || ActionName == TEXT("None"))
	{
		return TRAVERSAL_REJECT("the chooser has no montage for it");
	}
	Action.StartTime = static_cast<float>(GetReal(Find(OutputType, TEXT("MontageStartTime")), Output.GetMemory()));
	Action.PlayRate = 1.f;
	// The sample's SetWarpTargets: hurdles and vaults go over the far edge, hurdles also land on the floor beyond it.
	Action.bWarpBackLedge = ActionName == TEXT("Hurdle") || ActionName == TEXT("Vault");
	Action.bWarpBackFloor = ActionName == TEXT("Hurdle");
	return true;
}

void UShinobiTraversalComponent::Perform(const FShinobiTraversalAction& Action)
{
	ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!Action.Montage || !AnimInstance)
	{
		return;
	}
	// The AnimBP's Set_InteractionTransform just stores this.
	ShinobiBlueprintStruct::SetStruct(AnimInstance->GetClass()->FindPropertyByName(TEXT("InteractionTransform")), AnimInstance,
		FTransform(FRotationMatrix::MakeFromZ(Action.FrontLedgeNormal).Rotator(), Action.FrontLedgeLocation));
	if (AnimInstance->Montage_Play(Action.Montage, Action.PlayRate, EMontagePlayReturnType::MontageLength, Action.StartTime) <= 0.f)
	{
		return;
	}
	bTraversing = true;
	PlayingMontage = Action.Montage;
	FOnMontageBlendingOutStarted BlendingOut = FOnMontageBlendingOutStarted::CreateUObject(this, &UShinobiTraversalComponent::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOut, Action.Montage);

	SetWarpTargets(Action);
	IgnoredObstacle = Action.Obstacle;
	Character->GetCapsuleComponent()->IgnoreComponentWhenMoving(IgnoredObstacle, true);
	Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying); // the montage's root motion carries the capsule
	GetWorld()->GetTimerManager().ClearTimer(ReplicationTimer);
	SetClientAuthoritative(true);
}

void UShinobiTraversalComponent::SetWarpTargets(const FShinobiTraversalAction& Action) const
{
	UMotionWarpingComponent* Warping = GetOwner()->FindComponentByClass<UMotionWarpingComponent>();
	if (!Warping)
	{
		return;
	}
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(FrontLedgeTarget, Action.FrontLedgeLocation + FVector(0.f, 0.f, 0.5f), (-Action.FrontLedgeNormal).Rotation());
	const bool bBackLedge = Action.bWarpBackLedge && HasWarpWindow(Action.Montage, BackLedgeTarget);
	if (bBackLedge)
	{
		Warping->AddOrUpdateWarpTargetFromLocation(BackLedgeTarget, Action.BackLedgeLocation);
	}
	else
	{
		Warping->RemoveWarpTarget(BackLedgeTarget);
	}
	if (bBackLedge && Action.bWarpBackFloor && HasWarpWindow(Action.Montage, BackFloorTarget))
	{
		const float Beyond = FMath::Abs(AnimatedDistanceFromLedge(Action.Montage, BackLedgeTarget) - AnimatedDistanceFromLedge(Action.Montage, BackFloorTarget));
		const FVector Floor = Action.BackLedgeLocation + Action.BackLedgeNormal * Beyond;
		Warping->AddOrUpdateWarpTargetFromLocation(BackFloorTarget, FVector(Floor.X, Floor.Y, Action.BackFloorLocation.Z));
	}
	else
	{
		Warping->RemoveWarpTarget(BackFloorTarget);
	}
}

void UShinobiTraversalComponent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != PlayingMontage.Get())
	{
		return;
	}
	bTraversing = false;
	ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	Character->GetCapsuleComponent()->IgnoreComponentWhenMoving(IgnoredObstacle, false);
	IgnoredObstacle = nullptr;
	if (Character->GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking); // falls on its own where there is no floor (vaults)
	}
	// As the sample does: keep trusting the owner's position a moment longer, while the corrections catch up.
	GetWorld()->GetTimerManager().SetTimer(ReplicationTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SetClientAuthoritative(false); }), 0.2f, false);
}

void UShinobiTraversalComponent::SetClientAuthoritative(bool bClientAuthoritative) const
{
	UCharacterMovementComponent* Movement = CastChecked<ACharacter>(GetOwner())->GetCharacterMovement();
	Movement->bIgnoreClientMovementErrorChecksAndCorrection = bClientAuthoritative;
	Movement->bServerAcceptClientAuthoritativePosition = bClientAuthoritative;
}

void UShinobiTraversalComponent::ServerPerform_Implementation(const FShinobiTraversalAction& Action)
{
	Perform(Action);
	MulticastPerform(Action);
}

void UShinobiTraversalComponent::MulticastPerform_Implementation(const FShinobiTraversalAction& Action)
{
	const ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	if (!Character->HasAuthority() && !Character->IsLocallyControlled())
	{
		Perform(Action); // the owner and the server already play it
	}
}
