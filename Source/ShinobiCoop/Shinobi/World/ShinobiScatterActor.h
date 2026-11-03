#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiScatterActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * One chunk of the open world's scattered nature (trees, bushes, grass, rocks): a hierarchical instanced mesh component
 * per mesh. Scripts/world/build_world.py spawns one per 128 m chunk (so World Partition streams them) and fills it from
 * placements.json. The mesh's own collision decides blocking (trunk UCX for trees, none for grass).
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
	void ApplyWindDistance(UHierarchicalInstancedStaticMeshComponent* Component) const;

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instances;
};
