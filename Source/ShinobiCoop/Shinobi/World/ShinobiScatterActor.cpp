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
	UInstancedStaticMeshComponent* Component = nullptr;
	for (UInstancedStaticMeshComponent* Existing : Instances)
	{
		if (Existing && Existing->GetStaticMesh() == Mesh)
		{
			Component = Existing;
		}
	}
	if (!Component)
	{
		Component = MakeComponent(Mesh, CullDistance);
	}
	Component->AddInstances(WorldTransforms, false, true);
}

UInstancedStaticMeshComponent* AShinobiScatterActor::MakeComponent(UStaticMesh* Mesh, float CullDistance)
{
	// Nanite culls and LODs per instance on the GPU: a plain instanced component. Without Nanite, the hierarchical one's tree does it.
	const bool bNanite = Mesh->HasValidNaniteData();
	const UClass* Class = bNanite ? UInstancedStaticMeshComponent::StaticClass() : UHierarchicalInstancedStaticMeshComponent::StaticClass();
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Class, MakeUniqueObjectName(this, Class, Mesh->GetFName()));
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
	return Component;
}

int32 AShinobiScatterActor::ConvertNaniteToInstanced()
{
	int32 Switched = 0;
	const TArray<TObjectPtr<UInstancedStaticMeshComponent>> Before = Instances;
	for (UInstancedStaticMeshComponent* Old : Before)
	{
		UStaticMesh* Mesh = Old ? Old->GetStaticMesh() : nullptr;
		if (!Mesh || !Old->IsA<UHierarchicalInstancedStaticMeshComponent>() || !Mesh->HasValidNaniteData())
		{
			continue;
		}
		TArray<FTransform> Transforms;
		Transforms.Reserve(Old->GetInstanceCount());
		for (int32 Index = 0; Index < Old->GetInstanceCount(); ++Index)
		{
			FTransform World;
			if (Old->GetInstanceTransform(Index, World, true))
			{
				Transforms.Add(World);
			}
		}
		const bool bShadow = Old->CastShadow;
		const int32 Cull = Old->InstanceEndCullDistance;
		const int32 WindOff = Old->WorldPositionOffsetDisableDistance;
		Modify();
		Instances.Remove(Old);
		RemoveInstanceComponent(Old);
		Old->DestroyComponent();
		UInstancedStaticMeshComponent* New = MakeComponent(Mesh, static_cast<float>(Cull));
		New->SetCastShadow(bShadow);
		New->SetWorldPositionOffsetDisableDistance(WindOff);
		New->AddInstances(Transforms, false, true);
		++Switched;
	}
	return Switched;
}

int32 AShinobiScatterActor::CountHierarchicalNanite() const
{
	int32 Count = 0;
	for (const UInstancedStaticMeshComponent* Component : Instances)
	{
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		Count += Mesh && Mesh->HasValidNaniteData() && Component->IsA<UHierarchicalInstancedStaticMeshComponent>() ? 1 : 0;
	}
	return Count;
}

void AShinobiScatterActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// Chunks built before WindDisableDistance existed (and any later change to it) pick it up on load.
	for (UInstancedStaticMeshComponent* Component : Instances)
	{
		ApplyWindDistance(Component);
	}
}

void AShinobiScatterActor::ApplyWindDistance(UInstancedStaticMeshComponent* Component) const
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
	for (const UInstancedStaticMeshComponent* Component : Instances)
	{
		Count += Component ? Component->GetInstanceCount() : 0;
	}
	return Count;
}
