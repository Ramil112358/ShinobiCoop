#include "Shinobi/World/ShinobiVillager.h"
#include "ShinobiCoop.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationInvokerComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Math/RandomStream.h"
#include "UObject/UnrealType.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/Hero/ShinobiLocomotionAnimInstance.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"

namespace
{
	// ponytail: a fixed bank of lines; per-villager data arrives with quests in sub-project #4
	const TCHAR* Barks[] = {
		TEXT("The camps in the forest have been restless lately."),
		TEXT("Welcome to Kazemori! Mind the bandits past the gate."),
		TEXT("My brother went east over the bridge. He hasn't come back."),
		TEXT("They say something old sleeps in the ruins up on the ridge."),
		TEXT("Fresh dango! ...Oh, you're off to fight? Later, then."),
		TEXT("Train hard. The posts by the south wall never hit back."),
		TEXT("The shrine keeps us safe. Mostly."),
		TEXT("I saw kunai glinting in the trees near the river."),
		TEXT("If you clear the camps, the traders might return."),
		TEXT("Careful with fire jutsu near the maples, please."),
	};
	const TCHAR* LondonBarks[] = {
		TEXT("Mind the gap... and the ninja hiding in it."),
		TEXT("That lot in Covent Garden scared off all the street performers."),
		TEXT("Tourists keep asking me for the Leaf Village. It's not on the Tube map, love."),
		TEXT("Someone set the Thames on fire last night. Again."),
		TEXT("Big Ben's been off since those bandits moved into the Tower."),
		TEXT("Fancy a cuppa before you sort out Camden Lock?"),
		TEXT("My cab took a fireball on the Embankment. Insurance won't believe a word."),
		TEXT("They say their boss is holed up in the Tower of London with the ravens."),
		TEXT("Bit of drizzle never stopped a shinobi, did it?"),
		TEXT("Queue's that way. Even for hero business."),
	};
	const FLinearColor Kimonos[] = {
		{ 0.10f, 0.20f, 0.45f }, { 0.45f, 0.12f, 0.10f }, { 0.12f, 0.35f, 0.15f },
		{ 0.40f, 0.32f, 0.12f }, { 0.30f, 0.12f, 0.35f }, { 0.35f, 0.35f, 0.38f },
	};
}

AShinobiVillager::AShinobiVillager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->InitCapsuleSize(38.f, 92.f);
	GetCharacterMovement()->MaxWalkSpeed = 160.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// Path following sets velocity directly with zero acceleration, and the walk animation only plays while
	// accelerating: without this, navmesh walkers glide along in their idle pose.
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 300.f, 0.f);
	bUseControllerRotationYaw = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> Walk(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (Quinn.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(Quinn.Object);
	}
	if (Walk.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(Walk.Class);
	}
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetRelativeScale3D(FVector(0.95f));
	Body = CreateDefaultSubobject<UShinobiBodyComponent>(TEXT("Body"));
	Body->SetupAttachment(GetMesh());

	BarkText = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("BarkText"));
	BarkText->SetupAttachment(RootComponent);
	BarkText->SetWorldSize(22.f);
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(2000.f, 2500.f);
}

void AShinobiVillager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiVillager, Variant, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiVillager, bLondon, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AShinobiVillager, bCrowd, COND_InitialOnly);
}

void AShinobiVillager::BeginPlay()
{
	Super::BeginPlay();
	if (bCrowd && WearCrowdLook())
	{
		Body->DestroyComponent();
		Body = nullptr;
		GetMesh()->SetVisibility(false, true);
		TalkCount = Variant;
		IdleTime = 1.f + (Variant % 4);
		return;
	}
	// Motion-matched walking from the Game Animation Sample; the constructor's ABP_Unarmed until it's migrated. Not for the
	// crowd above: City Sample characters play their own loops, and motion matching per crowd member would cost too much.
	if (UClass* Locomotion = UShinobiLocomotionAnimInstance::LoadAnimBlueprintClass())
	{
		GetMesh()->SetAnimInstanceClass(Locomotion);
	}
	if (Body->WearCharacter(Bodies[FMath::Abs(Variant) % UE_ARRAY_COUNT(Bodies)]))
	{
		Body->HideDriver();
	}
	else
	{
		Body->DestroyComponent();
		Body = nullptr;
		for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials() && !IsNetMode(NM_DedicatedServer); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = GetMesh()->CreateDynamicMaterialInstance(Slot))
			{
				Material->SetVectorParameterValue(TEXT("Paint Tint"), Kimonos[Variant % UE_ARRAY_COUNT(Kimonos)]);
			}
		}
	}
	ShinobiLook::EnableOutline(GetBodyMesh(), ShinobiLook::StencilFriendly);
	TalkCount = Variant;
	IdleTime = 1.f + (Variant % 4);
}

namespace
{
	/** Sets an int or enum (byte) variable of a Blueprint actor by name; false if it has no such variable. */
	bool SetBlueprintNumber(AActor* Actor, const TCHAR* Name, int32 Value)
	{
		FProperty* Property = Actor->GetClass()->FindPropertyByName(FName(Name));
		if (const FIntProperty* Int = CastField<FIntProperty>(Property))
		{
			Int->SetPropertyValue_InContainer(Actor, Value);
			return true;
		}
		if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			Byte->SetPropertyValue_InContainer(Actor, static_cast<uint8>(Value));
			return true;
		}
		if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
		{
			Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Actor), static_cast<int64>(Value));
			return true;
		}
		return false;
	}
}

void AShinobiVillager::EndPlay(const EEndPlayReason::Type Reason)
{
	if (CrowdLook)
	{
		CrowdLook->Destroy();
	}
	Super::EndPlay(Reason);
}

bool AShinobiVillager::WearCrowdLook()
{
	if (IsNetMode(NM_DedicatedServer))
	{
		return false;
	}
	UClass* LookClass = LoadClass<AActor>(nullptr, CrowdLookClass);
	CrowdIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySampleCrowd/Character/Anims/Loco/MTN_N_Idle.MTN_N_Idle"));
	CrowdWalk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/CitySampleCrowd/Character/Anims/Loco/MTN_N_Walk_InPlace.MTN_N_Walk_InPlace"));
	if (!LookClass || !CrowdIdle || !CrowdWalk)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("Villager %s: City Sample crowd character not found, using the villager body"), *GetName());
		return false;
	}
	// Deterministic from Variant: the same person on every machine. The Blueprint wraps every index into its lists.
	FRandomStream Pick(Variant * 7919 + 13);
	const FTransform Where = GetActorTransform();
	AActor* Look = GetWorld()->SpawnActorDeferred<AActor>(LookClass, Where, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Look)
	{
		return false;
	}
	SetBlueprintNumber(Look, TEXT("Skeleton"), Pick.RandRange(0, 1));  // A = man, B = woman
	const float Build = Pick.FRand();
	SetBlueprintNumber(Look, TEXT("Body Shape"), Build < 0.6f ? 0 : (Build < 0.82f ? 1 : 2));  // normal, over, under weight
	for (const TCHAR* Index : { TEXT("HeadIndex"), TEXT("OutfitIndex"), TEXT("OutfitMaterialIndex"), TEXT("HairIndex"),
		TEXT("HairColorIndex"), TEXT("SkinTextureIndex"), TEXT("PatternColorIndex"), TEXT("BeardIndex"), TEXT("MustacheIndex"),
		TEXT("EyebrowsIndex"), TEXT("AccessoryIndex") })
	{
		SetBlueprintNumber(Look, Index, Pick.RandRange(0, 63));
	}
	Look->FinishSpawning(Where);
	Look->SetActorEnableCollision(false);
	Look->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Look->SetActorRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Look->SetActorRelativeRotation(FRotator(0.f, -90.f, 0.f));
	CrowdLook = Look;
	if (const ASkeletalMeshActor* Skeletal = Cast<ASkeletalMeshActor>(Look))
	{
		CrowdRoot = Skeletal->GetSkeletalMeshComponent();
	}
	if (!CrowdRoot)
	{
		return true;
	}
	// The outfit pieces follow the base skeleton; the face keeps its own head Anim Blueprint. These are MetaHuman-grade
	// characters (morph-target faces, strand hair): 45 of them across the city cost ~100 ms of GPU, so they only draw
	// near players (hair nearer still), animate only while seen, and the faces skip their morph targets.
	for (UActorComponent* Component : Look->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			const bool bHair = !Primitive->IsA<USkinnedMeshComponent>() && !Primitive->IsA<UStaticMeshComponent>();
			Primitive->SetCachedMaxDrawDistance(bHair ? CrowdHairDrawDistance : CrowdDrawDistance);
			Primitive->SetCastShadow(!bHair);
		}
		USkeletalMeshComponent* Part = Cast<USkeletalMeshComponent>(Component);
		if (!Part)
		{
			continue;
		}
		Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		if (Part->GetFName() == TEXT("Face"))
		{
			Part->bDisableMorphTarget = true;
		}
		else if (Part != CrowdRoot)
		{
			Part->SetLeaderPoseComponent(CrowdRoot);
		}
	}
	CrowdRoot->PlayAnimation(CrowdIdle, true);
	CrowdRoot->SetPosition(Pick.FRand() * CrowdIdle->GetPlayLength());
	return true;
}

void AShinobiVillager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CrowdRoot)
	{
		const float Speed = GetVelocity().Size2D();
		const bool bMoving = Speed > 30.f;
		if (bMoving != bCrowdWalking)
		{
			bCrowdWalking = bMoving;
			CrowdRoot->PlayAnimation(bMoving ? CrowdWalk.Get() : CrowdIdle.Get(), true);
		}
		if (bMoving)
		{
			CrowdRoot->SetPlayRate(FMath::Clamp(Speed / 140.f, 0.7f, 1.4f));  // the walk cycle covers ~1.4 m/s
		}
	}
	if (!HasAuthority() || Route.Num() == 0)
	{
		return;
	}
	AAIController* AI = Cast<AAIController>(GetController());
	const FVector Target = Route[NextPoint % Route.Num()];
	const bool bArrived = FVector::Dist2D(Target, GetActorLocation()) < 120.f;
	if (bWalking)
	{
		const bool bPathDone = AI && AI->GetMoveStatus() == EPathFollowingStatus::Idle;
		if (bArrived || bPathDone)
		{
			bWalking = false;
			// Pause where the route turns back (the end of a pavement walk) or bends sharply, and now and then to
			// look in a shop window; otherwise walk straight on to the next point.
			const FVector Here = Route[NextPoint % Route.Num()];
			const FVector In = (Here - Route[(NextPoint + Route.Num() - 1) % Route.Num()]).GetSafeNormal2D();
			const FVector Out = (Route[(NextPoint + 1) % Route.Num()] - Here).GetSafeNormal2D();
			const bool bTurn = FVector::DotProduct(In, Out) < 0.3f;
			IdleTime = bTurn ? FMath::FRandRange(2.f, 6.f) : (FMath::FRand() < 0.1f ? FMath::FRandRange(1.f, 3.f) : 0.f);
			++NextPoint;
		}
		else if (!AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			AddMovementInput((Target - GetActorLocation()).GetSafeNormal2D()); // no navmesh: walk straight there
		}
		return;
	}
	IdleTime -= DeltaSeconds;
	if (IdleTime <= 0.f)
	{
		bWalking = true;
		if (AI)
		{
			const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Route[NextPoint % Route.Num()], 60.f);
			if (NextPoint == 0)  // once per villager: is there a navmesh to walk on?
			{
				UE_LOG(LogShinobiCoop, Log, TEXT("Villager %s: first move %s"), *GetName(),
					Result == EPathFollowingRequestResult::Failed ? TEXT("FAILED (no path)") : TEXT("ok"));
			}
		}
	}
}

FText AShinobiVillager::PickBark(int32 Seed, bool bLondonLines)
{
	return FText::FromString(bLondonLines ? LondonBarks[FMath::Abs(Seed) % UE_ARRAY_COUNT(LondonBarks)]
		: Barks[FMath::Abs(Seed) % UE_ARRAY_COUNT(Barks)]);
}

int32 AShinobiVillager::NumBarks(bool bLondonLines)
{
	return bLondonLines ? UE_ARRAY_COUNT(LondonBarks) : UE_ARRAY_COUNT(Barks);
}

FText AShinobiVillager::Talk()
{
	const FText Line = PickBark(TalkCount++, bLondon);
	BarkText->SetNameMode(Line.ToString());
	GetWorldTimerManager().SetTimer(BarkTimer, this, &AShinobiVillager::HideBark, BarkSeconds, false);
	return Line;
}

void AShinobiVillager::HideBark()
{
	BarkText->SetNameMode(FString());
}
