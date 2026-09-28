#include "Shinobi/VFX/ShinobiBlackFlameComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	/** Where the tongues burn: the same ten bones on Manny and on a Mixamo-rigged hero. */
	const TCHAR* const MannyBones[] = { TEXT("spine_01"), TEXT("spine_03"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("lowerarm_l"), TEXT("lowerarm_r"), TEXT("thigh_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("calf_r") };
	const TCHAR* const MixamoBones[] = { TEXT("Spine"), TEXT("Spine2"), TEXT("LeftArm"), TEXT("RightArm"), TEXT("LeftForeArm"), TEXT("RightForeArm"), TEXT("LeftUpLeg"), TEXT("RightUpLeg"), TEXT("LeftLeg"), TEXT("RightLeg") };
	const FVector TongueScale(0.12f, 0.12f, 0.45f); // the engine sphere is 100 units across: a 12 x 45 tongue
}

UShinobiBlackFlameComponent::UShinobiBlackFlameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while burning
}

int32 UShinobiBlackFlameComponent::NumVisibleTongues() const
{
	int32 Count = 0;
	for (const UStaticMeshComponent* Tongue : Tongues)
	{
		Count += Tongue->IsVisible() ? 1 : 0;
	}
	return Count;
}

void UShinobiBlackFlameComponent::BeginPlay()
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
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInstanceDynamic* Flame = ShinobiVfx::MakeFlame(this, ShinobiVfx::Colors::BlackFlame, 3.f, true);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(MannyBones); ++Index)
		{
			UStaticMeshComponent* Tongue = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), TEXT("BlackFlameTongue")));
			Tongue->SetStaticMesh(Sphere);
			Tongue->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tongue->SetCastShadow(false);
			Tongue->SetMaterial(0, Flame);
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
		Light->SetLightColor(ShinobiVfx::Colors::BlackFlame);
		Light->SetIntensity(400.f);
		Light->SetAttenuationRadius(300.f);
		Light->SetCastShadows(false);
		Light->SetSpecularScale(0.f);
		Light->SetVisibility(false);
		Light->RegisterComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		TagHandle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Burning_Black, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UShinobiBlackFlameComponent::OnBurningTagChanged);
		SetBurning(ASC->HasMatchingGameplayTag(ShinobiTags::State_Burning_Black));
	}
}

void UShinobiBlackFlameComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(TagHandle, ShinobiTags::State_Burning_Black, EGameplayTagEventType::NewOrRemoved);
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

void UShinobiBlackFlameComponent::OnBurningTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetBurning(NewCount > 0);
}

void UShinobiBlackFlameComponent::SetBurning(bool bOn)
{
	bBurning = bOn;
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

void UShinobiBlackFlameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Each tongue flickers on its own: a random stretch and a small sway every frame.
	for (UStaticMeshComponent* Tongue : Tongues)
	{
		Tongue->SetWorldRotation(FRotator(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(0.f, 360.f), 0.f));
		Tongue->SetWorldScale3D(TongueScale * FVector(1.f, 1.f, FMath::FRandRange(0.8f, 1.25f)));
	}
	if (Light)
	{
		Light->SetIntensity(400.f * FMath::FRandRange(0.7f, 1.2f));
	}
}
