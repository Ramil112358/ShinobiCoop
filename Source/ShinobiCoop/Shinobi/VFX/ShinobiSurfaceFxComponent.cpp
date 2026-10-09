#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	// The Manny drives every pose (the hero mesh copies it) and keeps animating while hidden, so its bones are always current.
	const FName FootBones[] = { TEXT("foot_l"), TEXT("foot_r") };
	const FName BallBones[] = { TEXT("ball_l"), TEXT("ball_r") };
}

UShinobiSurfaceFxComponent::UShinobiSurfaceFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UShinobiSurfaceFxComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (IsNetMode(NM_DedicatedServer) || !Owner || !Owner->GetMesh() || !Sphere)
	{
		SetComponentTickEnabled(false); // purely cosmetic
		return;
	}
	Glow = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Chakra, 2.5f, 0.f); // Core 0: a rim, so it reads as a thin layer
	if (!Glow)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Glow->SetScalarParameterValue(TEXT("Fade"), 0.f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FootBones); ++Index)
	{
		UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(Owner);
		Shell->SetStaticMesh(Sphere);
		Shell->SetMaterial(0, Glow);
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shell->SetCastShadow(false);
		Shell->SetupAttachment(this);
		Shell->SetVisibility(false);
		Shell->RegisterComponent();
		Shells.Add(Shell);
	}
	Light = NewObject<UPointLightComponent>(Owner);
	Light->SetupAttachment(this);
	Light->SetLightColor(ShinobiVfx::Colors::Chakra);
	Light->SetAttenuationRadius(LightRadius);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetIntensity(0.f);
	Light->RegisterComponent();
}

void UShinobiSurfaceFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	const UShinobiMovementComponent* Movement = Owner ? Cast<UShinobiMovementComponent>(Owner->GetCharacterMovement()) : nullptr;
	if (!Movement || !Glow)
	{
		return;
	}
	Fade = Movement->CanSurfaceTraverse()
		? FMath::FInterpConstantTo(Fade, Movement->IsSurfaceTraversing() ? 1.f : 0.f, DeltaTime, 1.f / FadeTime)
		: 0.f; // death, downed and mounting hide it at once
	Age += DeltaTime;
	Glow->SetScalarParameterValue(TEXT("Fade"), Fade * (0.85f + 0.15f * FMath::Sin(Age * 6.f)));
	if (Light)
	{
		Light->SetIntensity(LightIntensity * Fade);
	}
	const USkeletalMeshComponent* Body = Owner->GetMesh();
	for (int32 Index = 0; Index < Shells.Num(); ++Index)
	{
		UStaticMeshComponent* Shell = Shells[Index];
		Shell->SetVisibility(Fade > 0.f);
		if (Fade <= 0.f)
		{
			continue;
		}
		// From the ankle to the ball of the foot, a little toward the sole: placed from bone positions, so it does not
		// depend on how each bone's axes are oriented.
		const FVector Ankle = Body->GetSocketLocation(FootBones[Index]);
		const FVector Ball = Body->GetSocketLocation(BallBones[Index]);
		const FVector Up = Owner->GetActorUpVector();
		Shell->SetWorldLocationAndRotation((Ankle + Ball) * 0.5f - Up * 3.f, FRotationMatrix::MakeFromXZ(Ball - Ankle, Up).Rotator());
		Shell->SetWorldScale3D(ShellSize / 100.f); // the engine sphere is 100 uu across
	}
}
