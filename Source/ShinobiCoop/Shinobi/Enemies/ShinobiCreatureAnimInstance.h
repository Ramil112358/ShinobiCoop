#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "ShinobiCreatureAnimInstance.generated.h"

class UAnimSequence;

/** The clips a summoned creature moves with. Missing ones are skipped (a creature with no Run walks faster instead). */
USTRUCT()
struct FShinobiCreatureClips
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Run;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Attack;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Hit;

	/** Null for a creature that ragdolls instead. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Death;

	/** World speed (cm/s) at which Walk and Run cover the ground at play rate 1, the actor's scale included. */
	float WalkSpeed = 150.f;
	float RunSpeed = 450.f;

	/** Seconds the whole attack lasts (wind-up and recovery): the Attack clip is stretched or squeezed to fit. 0 = natural length. */
	float AttackSeconds = 0.f;
};

/** One clip in the blend: its own time and rate, and how much of the pose it has. */
struct FShinobiCreatureLayer
{
	UAnimSequence* Clip = nullptr;
	float Time = 0.f;
	float Rate = 1.f;
	bool bLoop = true;
	float Weight = 0.f;
};

/** Evaluates the layers as one pose: the newest clip blends in while the older ones blend out. Also drives the Susanoo figure. */
USTRUCT()
struct FShinobiCreatureAnimProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FShinobiCreatureAnimProxy() {}
	FShinobiCreatureAnimProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override {}
	virtual bool Evaluate(FPoseContext& Output) override;

	/** Starts Clip on top (the older layers start fading out). Game thread. */
	void Play(UAnimSequence* Clip, bool bLoop, float Rate);

	/** Moves every layer's time on, fades the newest in and the rest out over BlendSeconds, drops faded ones. Game thread. */
	void Advance(float DeltaSeconds, float BlendSeconds);

	/** Oldest first; the last is the clip that is meant to be playing. */
	TArray<FShinobiCreatureLayer> Layers;
};

/**
 * Drives a summoned creature (AShinobiSummon) without an animation blueprint: it picks the clip from what the creature is doing
 * (standing, walking, running, attacking, flinching, dying) and cross-fades between clips over BlendSeconds instead of cutting.
 * Walking and running play at the speed the creature actually moves so the paws do not slide; the attack clip is fitted to the length
 * of the attack ability so its blow lands with the ability's. Runs on every machine from replicated state (velocity, health and the
 * State.SummonAttacking tag), so everyone sees the same animation.
 */
UCLASS(Transient)
class SHINOBICOOP_API UShinobiCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	static constexpr float BlendSeconds = 0.2f;

	void Setup(const FShinobiCreatureClips& InClips);

	/** The clip meant to be playing (the newest layer), or null before the first update. */
	UAnimSequence* GetCurrentClip() const;
	float GetCurrentRate() const;

	/** Clips still in the blend (more than one while cross-fading). */
	int32 GetLayerCount() const;
	TArray<float> GetLayerWeights() const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FShinobiCreatureAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FShinobiCreatureAnimProxy*>(InProxy); }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	enum class EState : uint8 { None, Idle, Walk, Run, Attack, Hit, Dead };

	/** Starts Clip (the old ones start fading out). */
	void Play(UAnimSequence* Clip, bool bLoop, float Rate);

	UPROPERTY()
	FShinobiCreatureClips Clips;

	EState State = EState::None;
	float LastHealth = -1.f;
	float HitRemaining = 0.f;
	float HitCooldown = 0.f;
};
