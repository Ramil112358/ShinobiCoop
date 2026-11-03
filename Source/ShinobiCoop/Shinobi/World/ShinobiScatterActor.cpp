#include "Shinobi/World/ShinobiScatterActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"

AShinobiScatterActor::AShinobiScatterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
}

void AShinobiScatterActor::AddInstances(UStaticMesh* Mesh, const TArray<FTransform>& WorldTransforms, float CullDistance)
{
	if (!Mesh || WorldTransforms.IsEmpty())
	{
		return;
	}
	UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
	for (UHierarchicalInstancedStaticMeshComponent* Existing : Instances)
	{
		if (Existing && Existing->GetStaticMesh() == Mesh)
		{
			Component = Existing;
		}
	}
	if (!Component)
	{
		Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Mesh->GetFName()));
		Component->SetMobility(EComponentMobility::Static);
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(Mesh);
		Component->SetCullDistances(0, static_cast<int32>(CullDistance));
		Component->bAffectDistanceFieldLighting = false;
		Component->SetCastShadow(CullDistance <= 0.f || CullDistance > 10000.f); // ground cover (short cull) casts no shadow
		ApplyWindDistance(Component);
		Component->RegisterComponent();
		AddInstanceComponent(Component);
		Instances.Add(Component);
	}
	Component->AddInstances(WorldTransforms, false, true);
}

void AShinobiScatterActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// Chunks built before WindDisableDistance existed (and any later change to it) pick it up on load.
	for (UHierarchicalInstancedStaticMeshComponent* Component : Instances)
	{
		ApplyWindDistance(Component);
	}
}

void AShinobiScatterActor::ApplyWindDistance(UHierarchicalInstancedStaticMeshComponent* Component) const
{
	if (!Component)
	{
		return;
	}
	const float Cull = static_cast<float>(Component->InstanceEndCullDistance);
	const float Distance = Cull > 0.f && WindDisableDistance > 0.f ? FMath::Min(WindDisableDistance, Cull * 0.5f) : WindDisableDistance;
	const int32 Wanted = FMath::RoundToInt(Distance);
	if (Component->WorldPositionOffsetDisableDistance != Wanted)
	{
		Component->SetWorldPositionOffsetDisableDistance(Wanted);
	}
}

int32 AShinobiScatterActor::GetInstanceCount() const
{
	int32 Count = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Instances)
	{
		Count += Component ? Component->GetInstanceCount() : 0;
	}
	return Count;
}
