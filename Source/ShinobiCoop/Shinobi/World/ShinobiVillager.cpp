#include "Shinobi/World/ShinobiVillager.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationInvokerComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiLook.h"
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
}

void AShinobiVillager::BeginPlay()
{
	Super::BeginPlay();
	if (!IsNetMode(NM_DedicatedServer))
	{
		for (int32 Slot = 0; Slot < GetMesh()->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = GetMesh()->CreateDynamicMaterialInstance(Slot))
			{
				Material->SetVectorParameterValue(TEXT("Paint Tint"), Kimonos[Variant % UE_ARRAY_COUNT(Kimonos)]);
			}
		}
	}
	ShinobiLook::EnableOutline(GetMesh(), ShinobiLook::StencilFriendly);
	TalkCount = Variant;
	IdleTime = 1.f + (Variant % 4);
}

void AShinobiVillager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
			IdleTime = FMath::FRandRange(2.f, 6.f);
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
			AI->MoveToLocation(Route[NextPoint % Route.Num()], 60.f);
		}
	}
}

FText AShinobiVillager::PickBark(int32 Seed)
{
	return FText::FromString(Barks[FMath::Abs(Seed) % UE_ARRAY_COUNT(Barks)]);
}

int32 AShinobiVillager::NumBarks()
{
	return UE_ARRAY_COUNT(Barks);
}

FText AShinobiVillager::Talk()
{
	const FText Line = PickBark(TalkCount++);
	BarkText->SetNameMode(Line.ToString());
	GetWorldTimerManager().SetTimer(BarkTimer, this, &AShinobiVillager::HideBark, BarkSeconds, false);
	return Line;
}

void AShinobiVillager::HideBark()
{
	BarkText->SetNameMode(FString());
}
