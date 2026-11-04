#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiScatterActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * One chunk of the open world's scattered nature (trees, bushes, grass, rocks) or London's street furniture: an instanced
 * mesh component per mesh. Scripts/world/build_world.py (and Scripts/london/build_london.py) spawn one per 128 m chunk (so
 * World Partition streams them) and fill it. The mesh's own collision decides blocking (trunk UCX for trees, none for grass).
 *
 * A Nanite mesh gets a plain instanced static mesh component: Nanite culls and picks detail per instance on the GPU, so
 * the hierarchical component's cluster tree only costs build time and memory. A mesh without Nanite keeps the hierarchical
 * one (its tree culls and LODs the instances on the CPU). ConvertNaniteToInstanced switches chunks built before this.
 */
UCLASS()
class SHINOBICOOP_API AShinobiScatterActor : public AActor
{
	GENERATED_BODY()

public:
	AShinobiScatterActor();

	/** World-space transforms; adds (or extends) the component for Mesh. CullDistance 0 = never culled. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	void AddInstances(UStaticMesh* Mesh, const TArray<FTransform>& WorldTransforms, float CullDistance = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	int32 GetInstanceCount() const;

	/** Editor (Scripts/world/convert_scatter_ism.py): moves each Nanite mesh's instances from a hierarchical component to a
	 * plain instanced one with the same settings. Returns how many components were switched (0: nothing to do). */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	int32 ConvertNaniteToInstanced();

	/** Components drawing Nanite meshes that are still hierarchical (tests, the conversion script). */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|World")
	int32 CountHierarchicalNanite() const;

	virtual void PostRegisterAllComponents() override;

	/**
	 * Beyond this distance (cm) the foliage material's wind (World Position Offset) stops. Moving Nanite geometry
	 * invalidates its cached Virtual Shadow Map pages every frame and needs the costlier programmable raster path, so
	 * far-off sway nobody can make out still costs shadow and raster time. Ground cover with a short cull distance
	 * stops at half its cull distance when that is nearer. 0 keeps the wind at every distance.
	 */
	UPROPERTY(EditAnywhere, Category = "Shinobi|Performance", meta = (ClampMin = "0", Units = "cm"))
	float WindDisableDistance = 8000.f;

private:
	void ApplyWindDistance(UInstancedStaticMeshComponent* Component) const;
	/** A registered component for Mesh with these settings: plain instanced for a Nanite mesh, hierarchical otherwise. */
	UInstancedStaticMeshComponent* MakeComponent(UStaticMesh* Mesh, float CullDistance);

	/** (Older chunks saved these as hierarchical components; a hierarchical component is an instanced one, so they load.) */
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instances;
};
