#include "Shinobi/Pvp/ShinobiPickup.h"
#include "AbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/UI/ShinobiOverheadTextComponent.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float OrbHeight = 70.f;  // above the point it stands on
	constexpr float BobHeight = 12.f;
	constexpr float BobPerSecond = 0.6f;
	constexpr float SpinDegreesPerSecond = 90.f;

	struct FPickupLook
	{
		const TCHAR* Name;
		FLinearColor Tint;
	};

	FPickupLook LookOf(EShinobiPickupType Type)
	{
		switch (Type)
		{
		case EShinobiPickupType::Attack: return { TEXT("Attack"), FLinearColor(1.f, 0.12f, 0.03f) };
		case EShinobiPickupType::Speed: return { TEXT("Speed"), FLinearColor(0.95f, 0.75f, 0.03f) };
		case EShinobiPickupType::Shield: return { TEXT("Shield"), ShinobiVfx::Colors::Chakra };
		default: return { TEXT("Health"), FLinearColor(0.05f, 0.9f, 0.2f) };
		}
	}
}

AShinobiPickup::AShinobiPickup()
{
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true; // a handful per arena, and players should see them from across it
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(100.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AShinobiPickup::OnTriggerOverlap);
	RootComponent = Trigger;

	Orb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orb"));
	Orb->SetupAttachment(Trigger);
	Orb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Orb->SetCastShadow(false);
	// A slightly egg-shaped orb (the engine sphere is 100 units across), so the spin shows.
	Orb->SetRelativeScale3D(FVector(0.5f, 0.38f, 0.42f));
	Orb->SetRelativeLocation(FVector(0.f, 0.f, OrbHeight));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Orb->SetStaticMesh(Sphere.Object);
	}

	Label = CreateDefaultSubobject<UShinobiOverheadTextComponent>(TEXT("Label"));
	Label->SetupAttachment(Trigger);
	Label->SetRelativeLocation(FVector(0.f, 0.f, OrbHeight + 70.f));
}

AShinobiPickup* AShinobiPickup::Spawn(UWorld* World, EShinobiPickupType Type, const FVector& Location)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTransform At(Location);
	AShinobiPickup* Pickup = World->SpawnActorDeferred<AShinobiPickup>(StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Pickup)
	{
		Pickup->Type = Type;
		Pickup->FinishSpawning(At);
	}
	return Pickup;
}

void AShinobiPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AShinobiPickup, Type, COND_InitialOnly);
	DOREPLIFETIME(AShinobiPickup, bTaken);
}

void AShinobiPickup::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		SetActorTickEnabled(false); // the bob and spin are all the tick does
		return;
	}
	// The project's shared glow (M_ChakraBodyAura, Core 1: lit all over), tinted per type, as the orbs of the jutsu are.
	const FPickupLook Look = LookOf(Type);
	Orb->SetMaterial(0, ShinobiVfx::MakeGlow(this, Look.Tint, 4.f, 1.f));
	Label->SetNameMode(Look.Name);
	Label->SetTextRenderColor(Look.Tint.ToFColor(true));
	Age = FMath::FRandRange(0.f, 10.f); // pickups side by side don't bob in step
}

void AShinobiPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTaken)
	{
		return;
	}
	Age += DeltaSeconds;
	Orb->SetRelativeLocationAndRotation(FVector(0.f, 0.f, OrbHeight + BobHeight * FMath::Sin(Age * BobPerSecond * UE_TWO_PI)),
		FRotator(0.f, FMath::Fmod(Age * SpinDegreesPerSecond, 360.f), 0.f));
}

bool AShinobiPickup::Apply(AShinobiCharacterBase& Target, EShinobiPickupType Type)
{
	UAbilitySystemComponent* ASC = Target.GetAbilitySystemComponent();
	const UShinobiAttributeSet* Stats = Target.GetAttributes();
	if (!ASC || !Stats || !Target.HasAuthority() || !Target.IsAlive())
	{
		return false;
	}
	switch (Type)
	{
	case EShinobiPickupType::Health:
		// PreAttributeBaseChange clamps it to MaxHealth.
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), Stats->GetHealth() + HealFraction * Stats->GetMaxHealth());
		break;
	case EShinobiPickupType::Attack:
		ASC->ApplyGameplayEffectToSelf(GetDefault<UGE_AttackBoost>(), 1.f, ASC->MakeEffectContext());
		break;
	case EShinobiPickupType::Speed:
		ASC->ApplyGameplayEffectToSelf(GetDefault<UGE_SpeedBoost>(), 1.f, ASC->MakeEffectContext());
		break;
	case EShinobiPickupType::Shield:
	{
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetChakraAttribute(), Stats->GetMaxChakra());
		ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetShieldAttribute(), UGE_Shield::Absorb);
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(GetDefault<UGE_Shield>(), 1.f, ASC->MakeEffectContext());
		// However it ends (spent, run out, a round reset), no shield value outlives the tag. A refresh returns the same
		// handle and adds another of these; they all do the same.
		if (FOnActiveGameplayEffectRemoved_Info* Removed = ASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
		{
			Removed->AddLambda([WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC)](const FGameplayEffectRemovalInfo&)
			{
				if (UAbilitySystemComponent* Owner = WeakASC.Get())
				{
					Owner->SetNumericAttributeBase(UShinobiAttributeSet::GetShieldAttribute(), 0.f);
				}
			});
		}
		break;
	}
	}
	return true;
}

bool AShinobiPickup::TryTake(AActor* Taker)
{
	// One winner: the first taker sets bTaken before anyone else is looked at. Summons (they share their caster's side)
	// don't pick things up.
	AShinobiCharacterBase* Character = Cast<AShinobiCharacterBase>(Taker);
	if (bTaken || !HasAuthority() || !Character || Character->IsA<AShinobiSummon>() || !Apply(*Character, Type))
	{
		return false;
	}
	SetTaken(true);
	return true;
}

void AShinobiPickup::OnTriggerOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	TryTake(Other);
}

void AShinobiPickup::SetTaken(bool bNowTaken)
{
	bTaken = bNowTaken;
	if (HasAuthority())
	{
		if (bTaken)
		{
			GetWorldTimerManager().SetTimer(RespawnTimer, this, &AShinobiPickup::ResetNow, RespawnSeconds);
		}
		else
		{
			GetWorldTimerManager().ClearTimer(RespawnTimer);
		}
	}
	// Last: turning collision back on updates overlaps, so whoever stands on the spot takes it again right here (a
	// nested SetTaken(true), which must find the timer work above already done).
	ApplyTakenLook();
}

void AShinobiPickup::ResetNow()
{
	SetTaken(false);
}

void AShinobiPickup::OnRep_Taken()
{
	ApplyTakenLook();
}

void AShinobiPickup::ApplyTakenLook()
{
	SetActorHiddenInGame(bTaken);
	SetActorEnableCollision(!bTaken);
}
