#include "Shinobi/VFX/ShinobiBodyFlameComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	/** Where the tongues burn: the same ten bones on Manny and on a Mixamo-rigged hero. */
	const TCHAR* const MannyBones[] = { TEXT("spine_01"), TEXT("spine_03"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r") };
	const TCHAR* const MixamoBones[] = { TEXT("Spine"), TEXT("Spine2"), TEXT("LeftArm"), TEXT("RightArm"), TEXT("LeftForeArm"), TEXT("RightForeArm"), TEXT("LeftUpLeg"), TEXT("RightUpLeg"), TEXT("LeftLeg"), TEXT("RightLeg") };
	const FVector TongueScale(0.55f, 1.1f, 1.f); // the engine plane is 100 units across: a 55 x 110 card, the flame fills about 40% of its width
	constexpr float SwapTime = 0.12f;
}

UShinobiBodyFlameComponent::UShinobiBodyFlameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while burning
}

int32 UShinobiBodyFlameComponent::NumVisibleTongues() const
{
	int32 Count = 0;
	for (const UStaticMeshComponent* Tongue : Tongues)
	{
		Count += Tongue->IsVisible() ? 1 : 0;
	}
	return Count;
}

void UShinobiBodyFlameComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (!IsNetMode(NM_DedicatedServer))
	{
		const AShinobiCharacterBase* OwnerCharacter = Cast<AShinobiCharacterBase>(GetOwner());
		USkeletalMeshComponent* Body = OwnerCharacter ? OwnerCharacter->GetBodyMesh() : nullptr;
		const bool bManny = Body && Body->GetBoneIndex(MannyBones[0]) != INDEX_NONE;
		UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		using ShinobiVfx::ESprite;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(MannyBones); ++Index)
		{
			UStaticMeshComponent* Tongue = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("BlackFlameTongue")));
			Tongue->SetStaticMesh(Plane);
			Tongue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tongue->SetCastShadow(false);
			const int32 Variant = FMath::RandRange(0, 7);
			FireMaterials.Add(ShinobiVfx::MakeSprite(this, ESprite::Flame, Variant, FLinearColor::White, 1.f));
			BlackMaterials.Add(ShinobiVfx::MakeSprite(this, ESprite::BlackFlame, Variant, ShinobiVfx::Colors::BlackFlame, 4.f));
			Variants.Add(Variant);
			NextSwap.Add(0.f);
			Tongue->SetMaterial(0, FireMaterials.Last());
			Tongue->SetVisibility(false);
			// Upright and world-sized whatever the bone's rotation and scale (a Mixamo rig can carry 100x bones).
			Tongue->SetUsingAbsoluteRotation(true);
			Tongue->SetUsingAbsoluteScale(true);
			if (Body)
			{
				Tongue->SetupAttachment(Body, bManny ? MannyBones[Index] : MixamoBones[Index]); // a bone name works as a socket
			}
			else
			{
				Tongue->SetupAttachment(this);
			}
			Tongue->RegisterComponent();
			Tongues.Add(Tongue);
		}
		Light = NewObject<UPointLightComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UPointLightComponent::StaticClass(), TEXT("BlackFlameLight")));
		Light->SetupAttachment(this);
		Light->SetIntensity(400.f);
		Light->SetAttenuationRadius(300.f);
		Light->SetCastShadows(false);
		Light->SetSpecularScale(0.f);
		Light->SetVisibility(false);
		Light->RegisterComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		BurningHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Burning, EGameplayTagEventType::NewOrRemoved) // either burn
			.AddUObject(this, &UShinobiBodyFlameComponent::OnTagChanged);
		DeadHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UShinobiBodyFlameComponent::OnTagChanged);
		Refresh();
	}
}

void UShinobiBodyFlameComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(BurningHandle, ShinobiTags::State_Burning, EGameplayTagEventType::NewOrRemoved);
		ASC->UnregisterGameplayTagEvent(DeadHandle, ShinobiTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
	}
	for (UStaticMeshComponent* Tongue : Tongues)
	{
		Tongue->DestroyComponent();
	}
	Tongues.Reset();
	if (Light)
	{
		Light->DestroyComponent();
		Light = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UShinobiBodyFlameComponent::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	Refresh();
}

void UShinobiBodyFlameComponent::Refresh()
{
	const UAbilitySystemComponent* ASC = AbilitySystem.Get();
	const bool bOn = ASC && ASC->HasMatchingGameplayTag(ShinobiTags::State_Burning) && !ASC->HasMatchingGameplayTag(ShinobiTags::State_Dead);
	bBurning = bOn;
	bBlack = bOn && ASC->HasMatchingGameplayTag(ShinobiTags::State_Burning_Black);
	for (int32 Index = 0; Index < Tongues.Num(); ++Index)
	{
		Tongues[Index]->SetMaterial(0, bBlack ? BlackMaterials[Index] : FireMaterials[Index]);
	}
	if (Light)
	{
		Light->SetLightColor(bBlack ? ShinobiVfx::Colors::BlackFlame : ShinobiVfx::Colors::Fire);
	}
	for (UStaticMeshComponent* Tongue : Tongues)
	{
		Tongue->SetVisibility(bOn);
	}
	if (Light)
	{
		Light->SetVisibility(bOn);
	}
	SetComponentTickEnabled(bOn && Tongues.Num() > 0);
}

void UShinobiBodyFlameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Each card flickers on its own: it switches flame shape every SwapTime or so, sways in size every frame, and turns
	// about the vertical to face the camera.
	Age += DeltaTime;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
	for (int32 Index = 0; Index < Tongues.Num(); ++Index)
	{
		UStaticMeshComponent* Tongue = Tongues[Index];
		if (Age >= NextSwap[Index])
		{
			Variants[Index] = (Variants[Index] + 1 + FMath::RandRange(0, 2)) % 8;
			ShinobiVfx::SetSpriteVariant(FireMaterials[Index], ShinobiVfx::ESprite::Flame, Variants[Index]);
			ShinobiVfx::SetSpriteVariant(BlackMaterials[Index], ShinobiVfx::ESprite::BlackFlame, Variants[Index]);
			NextSwap[Index] = Age + SwapTime * FMath::FRandRange(0.7f, 1.3f);
		}
		const FVector Here = Tongue->GetComponentLocation();
		const float FaceYaw = Camera ? FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - Here.Y, CameraAt.X - Here.X)) - 90.f : 0.f;
		Tongue->SetWorldRotation(FRotator(0.f, FaceYaw, 90.f));
		Tongue->SetWorldScale3D(TongueScale * FVector(1.f, FMath::FRandRange(0.9f, 1.15f), 1.f));
	}
	if (Light)
	{
		Light->SetIntensity((bBlack ? 400.f : 1500.f) * FMath::FRandRange(0.7f, 1.2f));
	}
}
