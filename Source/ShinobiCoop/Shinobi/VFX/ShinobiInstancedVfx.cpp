#include "Shinobi/VFX/ShinobiInstancedVfx.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** A free slot: drawn at zero scale. */
	const FTransform VfxHiddenTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
}

bool UShinobiInstancedVfxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UShinobiInstancedVfxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UShinobiInstancedVfxSubsystem::Deinitialize()
{
	if (Holder)
	{
		Holder->Destroy();
		Holder = nullptr;
	}
	Components.Reset();
	Meshes.Reset();
	Pools.Reset();
	Super::Deinitialize();
}

TStatId UShinobiInstancedVfxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UShinobiInstancedVfxSubsystem, STATGROUP_Tickables);
}

int32 UShinobiInstancedVfxSubsystem::FindOrMakePool(UStaticMesh* Mesh)
{
	const int32 Found = Meshes.IndexOfByKey(Mesh);
	if (Found != INDEX_NONE)
	{
		return Found;
	}
	UWorld* World = GetWorld();
	if (!Holder)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Holder)
		{
			return INDEX_NONE;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Holder);
	Component->SetStaticMesh(Mesh);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false); // (as the separate shard components were)
	Component->SetupAttachment(Holder->GetRootComponent());
	Component->RegisterComponent();
	Meshes.Add(Mesh);
	Components.Add(Component);
	Pools.AddDefaulted();
	return Pools.Num() - 1;
}

FShinobiVfxInstance UShinobiInstancedVfxSubsystem::Add(UStaticMesh* Mesh, const FTransform& WorldTransform)
{
	FShinobiVfxInstance Result;
	const int32 PoolIndex = Mesh && GetWorld() ? FindOrMakePool(Mesh) : INDEX_NONE;
	UInstancedStaticMeshComponent* Component = Components.IsValidIndex(PoolIndex) ? Components[PoolIndex].Get() : nullptr;
	if (!Component)
	{
		return Result;
	}
	FPool& Pool = Pools[PoolIndex];
	int32 Index = INDEX_NONE;
	if (Pool.Free.Num() > 0)
	{
		Index = Pool.Free.Pop(EAllowShrinking::No);
	}
	else
	{
		Index = Component->AddInstance(WorldTransform, true);
		Pool.Current.SetNum(Index + 1);
		Pool.Previous.SetNum(Index + 1);
		Pool.Used.SetNum(Index + 1);
		// Per-instance previous transforms (motion vectors) are seeded from the instances there are when they're turned on.
		Component->SetHasPerInstancePrevTransforms(false);
		Component->SetHasPerInstancePrevTransforms(true);
	}
	Pool.Current[Index] = WorldTransform;
	Pool.Previous[Index] = WorldTransform; // (no motion on its first frame: it appears, it didn't fly in from the last user's spot)
	Pool.Used[Index] = true;
	Pool.bDirty = true;
	Result.Pool = PoolIndex;
	Result.Index = Index;
	return Result;
}

void UShinobiInstancedVfxSubsystem::Update(const FShinobiVfxInstance& Instance, const FTransform& WorldTransform)
{
	if (Pools.IsValidIndex(Instance.Pool) && Pools[Instance.Pool].Used.IsValidIndex(Instance.Index) && Pools[Instance.Pool].Used[Instance.Index])
	{
		FPool& Pool = Pools[Instance.Pool];
		Pool.Current[Instance.Index] = WorldTransform;
		Pool.bDirty = true;
	}
}

void UShinobiInstancedVfxSubsystem::Remove(FShinobiVfxInstance& Instance)
{
	if (Pools.IsValidIndex(Instance.Pool) && Pools[Instance.Pool].Used.IsValidIndex(Instance.Index) && Pools[Instance.Pool].Used[Instance.Index])
	{
		FPool& Pool = Pools[Instance.Pool];
		Pool.Current[Instance.Index] = VfxHiddenTransform;
		Pool.Previous[Instance.Index] = VfxHiddenTransform;
		Pool.Used[Instance.Index] = false;
		Pool.Free.Add(Instance.Index);
		Pool.bDirty = true;
	}
	Instance = FShinobiVfxInstance();
}

int32 UShinobiInstancedVfxSubsystem::GetShownCount() const
{
	int32 Shown = 0;
	for (const FPool& Pool : Pools)
	{
		for (const bool bUsed : Pool.Used)
		{
			Shown += bUsed ? 1 : 0;
		}
	}
	return Shown;
}

void UShinobiInstancedVfxSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// After the actors' ticks: one batch per mesh, with last frame's transforms as the previous ones (motion vectors).
	for (int32 PoolIndex = 0; PoolIndex < Pools.Num(); ++PoolIndex)
	{
		FPool& Pool = Pools[PoolIndex];
		UInstancedStaticMeshComponent* Component = Components[PoolIndex];
		if (!Component || (!Pool.bDirty && !Pool.bSettle) || Pool.Current.IsEmpty())
		{
			continue;
		}
		Component->BatchUpdateInstancesTransforms(0, Pool.Current, Pool.Previous, true, true, false);
		Pool.Previous = Pool.Current;
		// After a change, committed once more next frame even if nothing moves, so the previous transforms catch up and a
		// piece that stopped doesn't keep last frame's motion.
		Pool.bSettle = Pool.bDirty;
		Pool.bDirty = false;
	}
}
