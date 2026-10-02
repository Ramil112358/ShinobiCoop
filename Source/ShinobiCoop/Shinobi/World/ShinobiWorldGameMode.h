#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShinobiWorldGameMode.generated.h"

class AShinobiCamp;
class AShinobiGameState;

/**
 * The open world (sub-project #3). Players start in the village; camps are spawned at BeginPlay from the level's camp
 * markers (TargetPoints tagged ShinobiCampSite with Camp:<id>, Difficulty:<n>, Radius:<cm>, Waves:<n,t;n,t> or Boss). A dead
 * player respawns in the village RespawnSeconds later; if every player is downed or dead (a wipe), everyone respawns
 * in the village at once and camps in progress reset. Clearing the boss camp ends the session in victory
 * ("VALLEY CLEARED"). Cleared camps stay cleared until the host restarts.
 */
UCLASS()
class SHINOBICOOP_API AShinobiWorldGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShinobiWorldGameMode();

	static constexpr float RespawnSeconds = 10.f;

	virtual void Tick(float DeltaSeconds) override;

	const TArray<TObjectPtr<AShinobiCamp>>& GetCamps() const { return Camps; }

	/** Parses a marker's tags into camp settings; false if the actor isn't a camp marker. Public for tests. */
	static bool ReadMarker(const AActor& Marker, AShinobiCamp& Camp);

	/** Parses a villager marker (tagged ShinobiVillager, Route:x,y,z|x,y,z|...); false if it isn't one. */
	static bool ReadVillagerMarker(const AActor& Marker, TArray<FVector>& OutRoute);

	/** The value of an actor's Key:Value tag (Hub, Victory, Barks), or empty. */
	static FString TagValue(const AActor& Actor, const TCHAR* Key);

protected:
	virtual void BeginPlay() override;

private:
	AShinobiGameState* GetWorldState() const;
	void OnCampCleared(AShinobiCamp* Camp);
	void Respawn(APlayerController* Controller);

	UPROPERTY()
	TArray<TObjectPtr<AShinobiCamp>> Camps;

	/** Server time each player's pawn was first seen dead. */
	TMap<TWeakObjectPtr<APlayerController>, float> DeadSince;
};
