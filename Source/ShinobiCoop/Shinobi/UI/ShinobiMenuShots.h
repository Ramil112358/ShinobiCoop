#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShinobiMenuShots.generated.h"

/**
 * Scripted screenshots for the main menu (Scripts/menu_shots.ps1), run by command-line flags then quit:
 *   -ShinobiMenuTour (Lvl_MainMenu): every tab, an arena in 2v2 and the empty Find Games list -> ShinobiMenuTour_<n>.png
 *   -ShinobiMenuPortraits (Lvl_MainMenu, a 3:4 window): each roster character's head and shoulders, no UI
 *     -> ShinobiMenuPortrait_<Name>.png (the SHINOBI tab's cards)
 *   -ShinobiMapShot (any level): the level from a camera tagged ShinobiMenuShot, else above and behind the first
 *     PlayerStart, pawn and UI hidden -> ShinobiMapShot_<Id>.png (the PLAY tab's map cards)
 */
UCLASS()
class SHINOBICOOP_API UShinobiMenuShotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;

private:
	void StartPortraits(UWorld& World);
	void StartMapShot(UWorld& World);
	/** Runs Steps one after another, Interval seconds apart, then quits. */
	void RunSteps(TArray<TFunction<void()>> Steps, float Interval);

	FTimerHandle Timer;
	TArray<TFunction<void()>> Pending;
};
