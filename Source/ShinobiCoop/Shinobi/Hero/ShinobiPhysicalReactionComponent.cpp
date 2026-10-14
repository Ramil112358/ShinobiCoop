#include "Shinobi/Hero/ShinobiPhysicalReactionComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Chooser.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsControlAsset.h"
#include "PhysicsControlComponent.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "Shinobi/Hero/ShinobiAnimSample.h"
#include "Shinobi/Hero/ShinobiBlueprintStruct.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiTags.h"
#include "StructUtils/InstancedStruct.h"
#include "TimerManager.h"

namespace
{
	// The sample's assets (Scripts/anim/migrate_gasp.py). Loaded in BeginPlay, never from a constructor: see
	// UShinobiLocomotionAnimInstance::LoadAnimBlueprintClass.
	const TCHAR* ControlAssetPath = TEXT("/Game/Characters/UEFN_Mannequin/Rigs/PCA_SandboxCharacter.PCA_SandboxCharacter");
	const TCHAR* GetUpChooserPath = TEXT("/Game/Characters/UEFN_Mannequin/Animations/Ragdoll/CHT_GetUpMontages.CHT_GetUpMontages");
	const TCHAR* GetUpInputPath = TEXT("/Game/Blueprints/Data/S_CHT_GetUpMontages_IN.S_CHT_GetUpMontages_IN");
	const TCHAR* GetUpOutputPath = TEXT("/Game/Blueprints/Data/S_CHT_GetUpMontages_OUT.S_CHT_GetUpMontages_OUT");

	const FName PhysicalAnimationProfile(TEXT("PhysicalAnimation"));
	const FName KinematicProfile(TEXT("Kinematic"));
	const FName RagdollProfile(TEXT("Ragdoll"));
	const FName ParentSpaceControls(TEXT("ParentSpace"));
	const FName ChestBone(TEXT("spine_03"));
	const FName PelvisBone(TEXT("pelvis"));

	constexpr float MinTumbleSeconds = 0.7f;   // the sample's ragdoll also lies a beat before getting up
	constexpr float MaxTumbleSeconds = 1.2f;
	constexpr float SettledSpeed = 150.f;      // the sample's threshold between rolling and standing getups
	constexpr float DeathSagSeconds = 1.2f;    // strength on death fades from TumbleStrength to nothing over this

	TAutoConsoleVariable<bool> CVarPhysicalReactions(
		TEXT("Shinobi.PhysicalReactions"),
		true,
		TEXT("Physical hit reactions, knockdown tumbles and getups (Game Animation Sample physics control); 0 = animation only. Read when a character spawns."));
}

UShinobiPhysicalReactionComponent::UShinobiPhysicalReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UShinobiPhysicalReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UPhysicsControlAsset* ControlAsset = IsNetMode(NM_DedicatedServer) || !CVarPhysicalReactions.GetValueOnGameThread() ? nullptr
		: LoadObject<UPhysicsControlAsset>(nullptr, ControlAssetPath);
	if (!ControlAsset || !Mesh || !Mesh->GetPhysicsAsset())
	{
		SetComponentTickEnabled(false);
		return;
	}
	GetUpChooser = LoadObject<UChooserTable>(nullptr, GetUpChooserPath);
	Physics = NewObject<UPhysicsControlComponent>(Character, TEXT("PhysicsControl"));
	Physics->PhysicsControlAsset = ControlAsset;
	Physics->RegisterComponent();
	Physics->AddTickPrerequisiteComponent(Mesh); // drive toward this frame's animation, as the sample does after its AnimBP
	// The driver only queried until now. Its object type stays Pawn and Pawn stays ignored, so the bodies never push
	// against their own capsule.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// By default a simulated root body (the pelvis) drives the component itself, which then wandered off the capsule
	// with every tumble and never came back. The capsule places the mesh; the bodies only pose it.
	Mesh->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::ComponentTransformIsKinematic;
	Physics->CreateControlsAndBodyModifiersFromPhysicsControlAsset(Mesh, nullptr, NAME_None);
	for (const FName Name : Physics->GetBodyModifierNamesInSet(TEXT("All")))
	{
		if (Name.ToString().Contains(PelvisBone.ToString()))
		{
			PelvisModifiers.Add(Name);
		}
	}
	for (const FName Name : Physics->GetControlNamesInSet(TEXT("WorldSpace")))
	{
		if (Name.ToString().Contains(PelvisBone.ToString()))
		{
			PelvisWorldControls.Add(Name);
		}
	}
	SetProfile(PhysicalAnimationProfile);
}

void UShinobiPhysicalReactionComponent::SetProfile(FName NewProfile)
{
	if (Physics && Profile != NewProfile)
	{
		Profile = NewProfile;
		Physics->InvokeControlProfile(Profile);
		// The sample's pawn moves its capsule from the ragdoll; ours stays the movement's, and a simulated pelvis would
		// drag the mesh (and root motion) off it. Everything above the pelvis still reacts.
		for (const FName Name : Profile == PhysicalAnimationProfile ? PelvisModifiers : TArray<FName>())
		{
			Physics->SetBodyModifierMovementType(Name, EPhysicsMovementType::Kinematic);
		}
	}
}

void UShinobiPhysicalReactionComponent::SetStrength(float Strength)
{
	FPhysicsControlMultiplier Multiplier;
	Multiplier.LinearStrengthMultiplier = FVector(Strength);
	Multiplier.AngularStrengthMultiplier = Strength;
	Physics->SetControlMultipliersInSet(ParentSpaceControls, Multiplier, true);
}

void UShinobiPhysicalReactionComponent::Hit(const FVector& Direction, float Damage)
{
	MulticastHit(Direction.GetSafeNormal(), Damage);
}

void UShinobiPhysicalReactionComponent::MulticastHit_Implementation(FVector_NetQuantizeNormal Direction, float Damage)
{
	if (Physics && !bDead && !bKnockedDown && Profile == PhysicalAnimationProfile)
	{
		// The profile's controls hold the pose too stiffly for an impulse to show: slacken them, then ease back (Tick).
		HitTime = GetWorld()->GetTimeSeconds();
		SetStrength(HitStrength);
		const float Impulse = FMath::Min(Damage * HitImpulsePerDamage, MaxHitImpulse);
		// The whole upper body (chest, arms, head) as one, or the joints to the pinned pelvis soak it up.
		CastChecked<ACharacter>(GetOwner())->GetMesh()->AddImpulseToAllBodiesBelow(FVector(Direction) * Impulse, ChestBone, true, true);
	}
}

void UShinobiPhysicalReactionComponent::Knockdown(const FVector& Velocity)
{
	MulticastKnockdown(Velocity);
}

void UShinobiPhysicalReactionComponent::MulticastKnockdown_Implementation(FVector_NetQuantize Velocity)
{
	if (bDead)
	{
		return;
	}
	// Held the same time everywhere: the server's AI and the owner's input read the tag.
	if (!bKnockedDown)
	{
		if (UAbilitySystemComponent* ASC = CastChecked<AShinobiCharacterBase>(GetOwner())->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(ShinobiTags::State_Staggered);
		}
	}
	GetWorld()->GetTimerManager().SetTimer(HoldTimer, this, &UShinobiPhysicalReactionComponent::EndKnockdown, KnockdownHoldSeconds, false);
	bKnockedDown = true;
	bGettingUp = false;
	KnockdownTime = GetWorld()->GetTimeSeconds();
	if (!Physics)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = CastChecked<ACharacter>(GetOwner())->GetMesh();
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.1f); // the sample stops what was playing, so the ragdoll isn't fighting an attack
	}
	// The ragdoll profile simulates the bodies; the mesh itself stays on the capsule (SetSimulatePhysics on it let the
	// component follow the ragdoll off, metres from the character, for good). The pelvis is tethered to the capsule
	// across the ground only, so the body flies with the launch and falls freely but can't slide away from it.
	SetProfile(RagdollProfile);
	SetStrength(TumbleStrength);
	FPhysicsControlData Tether;
	Tether.LinearStrength = TetherStrength;
	Tether.AngularStrength = 0.f;
	FPhysicsControlMultiplier AcrossOnly;
	AcrossOnly.LinearStrengthMultiplier = FVector(1.f, 1.f, 0.f);
	for (const FName Name : PelvisWorldControls)
	{
		Physics->SetControlData(Name, Tether);
		Physics->SetControlMultiplier(Name, AcrossOnly, true);
	}
	Mesh->SetAllPhysicsLinearVelocity(Velocity); // fly with the capsule, which the launch carries
}

void UShinobiPhysicalReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Physics)
	{
		return;
	}
	const ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	const double Now = GetWorld()->GetTimeSeconds();
	if (bDead)
	{
		SetStrength(TumbleStrength * FMath::Max(0.f, 1.f - static_cast<float>(Now - DeathTime) / DeathSagSeconds));
		return;
	}
	if (bKnockedDown)
	{
		const float Tumbling = static_cast<float>(Now - KnockdownTime);
		const bool bLanded = !Character->GetCharacterMovement()->IsFalling();
		const bool bSettled = Character->GetMesh()->GetPhysicsLinearVelocity(PelvisBone).Size() < SettledSpeed;
		if (!bGettingUp && Tumbling >= MinTumbleSeconds && ((bLanded && bSettled) || Tumbling >= MaxTumbleSeconds))
		{
			GetUp();
		}
		return;
	}
	// Animation only while the animation moves the capsule (root motion: vaults, mantles), since a simulated pelvis drags
	// the mesh and with it the root motion off course, and on a wall, where the world's gravity would pull the limbs off.
	const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(Character->GetCharacterMovement());
	const bool bAnimationOnly = Character->IsPlayingRootMotion() || (Movement && Movement->IsWallTraversing());
	SetProfile(bAnimationOnly ? KinematicProfile : PhysicalAnimationProfile);
	const float SinceHit = static_cast<float>(Now - HitTime);
	if (Profile == PhysicalAnimationProfile && SinceHit < HitRecoverySeconds + DeltaTime)
	{
		SetStrength(FMath::Lerp(HitStrength, 1.f, FMath::Clamp(SinceHit / HitRecoverySeconds, 0.f, 1.f)));
	}
}

void UShinobiPhysicalReactionComponent::GetUp()
{
	bGettingUp = true;
	USkeletalMeshComponent* Mesh = CastChecked<ACharacter>(GetOwner())->GetMesh();
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	const UScriptStruct* InputType = LoadObject<UScriptStruct>(nullptr, GetUpInputPath);
	const UScriptStruct* OutputType = LoadObject<UScriptStruct>(nullptr, GetUpOutputPath);
	UAnimMontage* Montage = nullptr;
	float StartTime = 0.f;
	if (AnimInstance && GetUpChooser && InputType && OutputType)
	{
		// As the sample does on leaving its ragdoll mode: the pose history takes the ragdoll's pose, so the chooser picks
		// the getup that starts closest to how the body landed (face up, face down, still rolling).
		UPoseSearchLibrary::OverridePoseHistoryFromOwningMesh(AnimInstance, TEXT("PoseHistory"));
		using namespace ShinobiBlueprintStruct;
		FInstancedStruct Input(InputType);
		FInstancedStruct Output(OutputType);
		void* In = Input.GetMutableMemory();
		ShinobiAnimSample::CopyPoseHistory(AnimInstance, Find(InputType, TEXT("PoseHistory")), In);
		SetBool(Find(InputType, TEXT("RollingGetup")), In, Mesh->GetPhysicsLinearVelocity(PelvisBone).Size2D() > SettledSpeed);
		const FProperty* Mode = Find(InputType, TEXT("MovementMode"));
		for (const TCHAR* ModeName : { TEXT("Ragdoll"), TEXT("OnGround") }) // the sample asks while still in its ragdoll mode
		{
			SetEnum(Mode, In, EnumValue(Mode, ModeName));
			Montage = ShinobiAnimSample::ChooseMontage(GetUpChooser, AnimInstance, InputType, In, OutputType, Output.GetMutableMemory());
			if (Montage)
			{
				StartTime = static_cast<float>(GetReal(Find(OutputType, TEXT("MontageStartTime")), Output.GetMemory()));
				break;
			}
		}
	}
	// Back to physical animation, its parent-space controls released for a frame first (as the sample does) so the
	// body eases from where it lies onto the getup instead of snapping.
	FPhysicsControlData Released;
	Released.LinearStrength = 0.f;
	Released.AngularStrength = 0.f;
	Physics->SetControlDatasInSet(ParentSpaceControls, Released);
	Profile = NAME_None;
	// The simulated pelvis dragged the mesh along: put it back where the character keeps it.
	ACharacter* Character = CastChecked<ACharacter>(GetOwner());
	if (Mesh->GetAttachParent() != Character->GetCapsuleComponent())
	{
		Mesh->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);
	}
	Mesh->SetRelativeLocationAndRotation(Character->GetBaseTranslationOffset(), Character->GetBaseRotationOffset());
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bDead)
		{
			SetProfile(PhysicalAnimationProfile);
			SetStrength(1.f);
		}
	}));
	if (Montage && AnimInstance)
	{
		const ERootMotionMode::Type RootMotion = AnimInstance->RootMotionMode;
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion); // cosmetic: the capsule stays the server's
		AnimInstance->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, StartTime);
		FOnMontageEnded Ended = FOnMontageEnded::CreateWeakLambda(this, [this, RootMotion](UAnimMontage*, bool)
		{
			if (UAnimInstance* Anim = CastChecked<ACharacter>(GetOwner())->GetMesh()->GetAnimInstance())
			{
				Anim->SetRootMotionMode(RootMotion);
			}
		});
		AnimInstance->Montage_SetEndDelegate(Ended, Montage);
	}
}

void UShinobiPhysicalReactionComponent::EndKnockdown()
{
	if (bKnockedDown)
	{
		if (UAbilitySystemComponent* ASC = CastChecked<AShinobiCharacterBase>(GetOwner())->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(ShinobiTags::State_Staggered);
		}
	}
	bKnockedDown = false;
	if (Physics && !bGettingUp && !bDead)
	{
		GetUp(); // the hold ended before the body settled (still sliding): get up now
	}
	bGettingUp = false;
}

bool UShinobiPhysicalReactionComponent::Die()
{
	if (!Physics)
	{
		return false;
	}
	bDead = true;
	DeathTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
	SetProfile(RagdollProfile);
	CastChecked<ACharacter>(GetOwner())->GetMesh()->SetSimulatePhysics(true);
	SetStrength(TumbleStrength);
	return true;
}
