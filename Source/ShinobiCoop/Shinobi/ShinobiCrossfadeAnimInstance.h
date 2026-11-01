#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ShinobiCrossfadeAnimInstance.generated.h"

class UAnimSequence;

/**
 * Plays one clip at a time like a single-node instance, but a new clip fades in over the old one instead of snapping to it (the
 * dragons go from flying to landing to standing to walking; their clips start from different poses). No graph asset: the proxy
 * samples the two clips and blends them.
 */
UCLASS(Transient, NotBlueprintable)
class SHINOBICOOP_API UShinobiCrossfadeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Fades Clip in over BlendSeconds (from its start); the same clip again only changes its rate. */
	void Play(UAnimSequence* Clip, bool bLoop, float Rate = 1.f, float BlendSeconds = 0.35f);

	UAnimSequence* GetClip() const { return Current.Clip; }
	/** A clip played once has reached its end. */
	bool IsFinished() const { return Current.Clip && !Current.bLoop && Current.Time >= Current.Length(); }

	struct FTrack
	{
		UAnimSequence* Clip = nullptr; // kept alive by whoever plays it
		float Time = 0.f;
		float Rate = 1.f;
		bool bLoop = false;
		float Length() const;
		void Advance(float DeltaSeconds);
	};

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	friend struct FShinobiCrossfadeProxy;
	FTrack Current;
	FTrack Previous;
	float Blend = 1.f; // the current clip's weight
	float BlendRate = 0.f;
};
