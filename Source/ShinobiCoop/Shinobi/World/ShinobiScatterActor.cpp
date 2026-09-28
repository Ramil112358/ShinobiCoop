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
		Component->RegisterComponent();
		AddInstanceComponent(Component);
		Instances.Add(Component);
	}
	Component->AddInstances(WorldTransforms, false, true);
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
