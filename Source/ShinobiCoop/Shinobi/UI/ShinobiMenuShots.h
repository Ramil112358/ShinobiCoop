#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShinobiMenuShots.generated.h"

/**
 * Scripted screenshots for the main menu (Scripts/menu_shots.ps1), run by command-line flags then quit:
 *   -ShinobiMenuTour (Lvl_MainMenu): every tab, an arena in 2v2 and the empty Find Games list -> ShinobiMenuTour_<n>.png
 */
UCLASS()
class SHINOBICOOP_API UShinobiMenuShotSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& World) override;

private:
	/** Runs Steps one after another, Interval seconds apart, then quits. */
	void RunSteps(TArray<TFunction<void()>> Steps, float Interval);

	FTimerHandle Timer;
	TArray<TFunction<void()>> Pending;
};
