#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiDragonMount.generated.h"

class AShinobiCharacterBase;
class UAnimSequence;
class USkeletalMeshComponent;

/**
 * The dragon a rider sits on (Summon Dragon, page 5). A purely visual, replicated actor: the rider keeps its own character and
 * movement (UShinobiMovementComponent flies it while State.Mounted lasts); the dragon rides under it, attached to the rider's mesh so
 * it is smoothed with it on clients, and flaps, hovers or glides by how the rider moves. It is destroyed when the rider is no longer
 * mounted, gone, or when a newer dragon replaces it. Hits on the dragon are hits on nobody: it has no collision or health.
 */
UCLASS()
class SHINOBICOOP_API AShinobiDragonMount : public AActor
{
	GENERATED_BODY()

public:
	AShinobiDragonMount();

	/** Server only: a dragon for Rider, replacing the one it had. Null on a client. */
	static AShinobiDragonMount* Spawn(AShinobiCharacterBase* Rider);

	AShinobiCharacterBase* GetRider() const { return Rider; }
	USkeletalMeshComponent* GetBody() const { return Body; }

	/** Where the breath comes from: the dragon's mouth, or in front of the rider before the model is loaded. */
	FVector GetMouthLocation() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	void Play(UAnimSequence* Clip);

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Set before FinishSpawning, so clients have it before BeginPlay. */
	UPROPERTY(Replicated)
	TObjectPtr<AShinobiCharacterBase> Rider;

	UPROPERTY()
	TObjectPtr<UAnimSequence> FlapClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> GlideClip;
	UPROPERTY()
	TObjectPtr<UAnimSequence> Playing;
};
