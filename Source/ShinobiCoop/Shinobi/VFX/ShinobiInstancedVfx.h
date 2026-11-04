#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShinobiInstancedVfx.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/** A piece drawn by UShinobiInstancedVfxSubsystem: which mesh's pool, which instance in it. */
struct FShinobiVfxInstance
{
	int32 Pool = INDEX_NONE;
	int32 Index = INDEX_NONE;

	bool IsValid() const { return Pool != INDEX_NONE && Index != INDEX_NONE; }
};

/**
 * Draws many short-lived copies of the same effect mesh (the Ice Meteor shower's 60 shards, their chips and frost trails)
 * as one instanced static mesh per mesh, instead of a mesh component each: one draw and one render-state update per
 * mesh, not per piece. A piece takes a slot (Add), moves it every frame (Update) and gives it back (Remove); freed slots
 * are hidden (zero scale) and reused, so the instance count only grows to the busiest moment's.
 *
 * Every frame, after the actors have ticked and moved their pieces, each pool is committed in one batch with last
 * frame's transforms as the previous ones, so moving pieces write motion vectors (TSR, DLSS and frame generation need
 * them to rebuild fast motion without ghosting). Not on a dedicated server (nothing to draw).
 */
UCLASS()
class SHINOBICOOP_API UShinobiInstancedVfxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Shows Mesh at WorldTransform; the returned piece is moved with Update and given back with Remove. Mesh's own
	 * materials are used (they need the Instanced Static Meshes usage: Scripts/setup_motion_vectors.py). */
	FShinobiVfxInstance Add(UStaticMesh* Mesh, const FTransform& WorldTransform);
	/** Moves the piece (drawn from the next commit, this frame). */
	void Update(const FShinobiVfxInstance& Instance, const FTransform& WorldTransform);
	/** Hides the piece and frees its slot; Instance becomes invalid. */
	void Remove(FShinobiVfxInstance& Instance);

	/** Pieces shown now, over every pool (tests). */
	int32 GetShownCount() const;
	/** Instanced mesh components: one per mesh drawn so far (tests). */
	int32 GetPoolCount() const { return Pools.Num(); }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FPool
	{
		TArray<FTransform> Current;
		TArray<FTransform> Previous;
		TArray<bool> Used;
		TArray<int32> Free;
		/** Moved, added or removed since the last commit. */
		bool bDirty = false;
		/** The last commit had changes: commit once more (previous = current). */
		bool bSettle = false;
	};

	int32 FindOrMakePool(UStaticMesh* Mesh);

	/** Holds the instanced components (transient, not replicated). */
	UPROPERTY()
	TObjectPtr<AActor> Holder;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Components;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	TArray<FPool> Pools;
};
