#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Shinobi/Enemies/ShinobiCreatureAnimInstance.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ShinobiPerfectSusanooComponent.generated.h"

class UAbilitySystemComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/** The Susanoo figure's pose: the legs (stance or stride, cross-fading) and, over them from the spine up, the current slash. */
USTRUCT()
struct FShinobiSusanooAnimProxy : public FShinobiCreatureAnimProxy
{
	GENERATED_BODY()

	FShinobiSusanooAnimProxy() {}
	FShinobiSusanooAnimProxy(UAnimInstance* InAnimInstance) : FShinobiCreatureAnimProxy(InAnimInstance) {}

	virtual bool Evaluate(FPoseContext& Output) override;

	/** The slash, on UpperRoot and every bone under it. */
	FShinobiCreatureLayer Upper;
	FName UpperRoot;
};

/** The clips the figure moves with. Walk and Run are optional (without them it stands while its wearer moves). */
USTRUCT()
struct FShinobiSusanooClips
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Run;
};

/**
 * Plays the Susanoo figure's clips without an animation blueprint. Its legs follow its wearer: the stance while they stand, the
 * walk (or, fast, the run) while they move, at a rate that matches the stride to their speed. A slash plays on the upper body
 * only, sped up to the combo's rhythm, so the swing never drags the feet off their stride or the stance.
 */
UCLASS(Transient)
class SHINOBICOOP_API UShinobiSusanooAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	static constexpr float BlendSeconds = 0.25f;
	static constexpr float SlashBlendSeconds = 0.12f;
	/** A slash (its strike window, see GetSlashWindow) is played no slower than this many seconds from start to end. */
	static constexpr float SlashSeconds = 1.1f;
	/** World speeds (cm/s, the figure's 6.7x scale included) at which the walk and run cover the ground at rate 1. */
	static constexpr float WalkSpeed = 850.f;
	static constexpr float RunSpeed = 2200.f;
	/** Above this wearer speed it runs (if it has a run). */
	static constexpr float RunAbove = 900.f;

	void Setup(const FShinobiSusanooClips& InClips);

	/**
	 * Swings Clip with the upper body, then lets go. While a slash swings, Clip waits and takes over as that one lets go, so a
	 * mashed combo plays every slash through instead of restarting wind-ups. One waits at most: false if that place is taken.
	 */
	bool Slash(UAnimSequence* Clip);

	/** The legs' clip (the newest base layer), or null before the first update. */
	UAnimSequence* GetCurrentClip() const;

	/** The slash being swung, or null. */
	UAnimSequence* GetSlashClip() const;

	/** The clip seconds a slash swings: a wind-up before its strike (the hands' fastest moment) to just after. Found once per clip. */
	FVector2f GetSlashWindow(const UAnimSequence* Clip);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void StartSlash(UAnimSequence* Clip);

	UPROPERTY()
	FShinobiSusanooClips Clips;

	/** The slash waiting for the current one to finish, or null. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Queued;

	/** Where the current slash's window ends, in clip seconds. */
	float SlashEnd = 0.f;

	/** Each slash clip's window, keyed by clips the Susanoo component keeps loaded. */
	TMap<const UAnimSequence*, FVector2f> Windows;
};

/**
 * Susanoo's look: while its owner carries State.Susanoo, Madara's Perfect Susanoo (Ultimate Ninja Storm 4) stands around them,
 * about 12 m tall: the near-solid royal-blue armoured figure (SK_Susanoo in M_PerfectSusanoo, Scripts/setup_susanoo.py) with a chakra sword
 * (M_SusanooBlade) in each fist, a white spark flaring at each hilt, cyan sparks drifting up around it. It fades in over a moment and out when the armour ends; while it stands, only it
 * is seen: the wearer's own body (every mesh but the name tag) is hidden from halfway through the fade in until the armour ends.
 * It holds a sword stance and strides along as its wearer moves, and every time its owner swings (a melee combo montage starts) it swings too: the first and second hits with the right and
 * left blade, the finisher with the remaining slashes in turn. The tag and the montages replicate, so every machine shows it.
 * Cosmetic only; the armour itself is UGE_Susanoo and the damage exec.
 */
UCLASS(ClassGroup = (Shinobi))
class SHINOBICOOP_API UShinobiPerfectSusanooComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** The figure's height in world units. */
	static constexpr float FigureHeight = 1200.f;

	UShinobiPerfectSusanooComponent();

	/** True while the armour is up (the figure may still be fading out after it ends). */
	bool IsShown() const { return bWanted; }

	/** The figure; null until the armour first goes up, and on a dedicated server. */
	USkeletalMeshComponent* GetFigure() const { return Figure; }

	/** The swords in its right and left hand (empty until the figure is built). */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetSwords() const { return Swords; }

	UShinobiSusanooAnimInstance* GetFigureAnim() const;

	/** The slash clips, Slash1 first. */
	const TArray<TObjectPtr<UAnimSequence>>& GetSlashes() const { return Slashes; }

	/** Swings the next slash in turn. Called when the owner's melee swing starts. */
	void Slash();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSpark
	{
		FVector Offset = FVector::ZeroVector; // from the figure's feet
		float Size = 80.f;
		float Speed = 100.f;
		float NextFlicker = 0.f;
		int32 Variant = 0;
	};

	void OnSusanooChanged(const FGameplayTag Tag, int32 Count);

	/** Builds the figure, its swords and sparks the first time they are needed. Does nothing if the model is not imported. */
	void EnsureFigure();

	/** Starts a slash when the owner starts a melee combo montage. */
	void FollowOwnerSwing();

	void UpdateSparks(float DeltaTime);

	/** Hides (or shows again) the owner's own meshes, leaving the figure, its swords and sparks and any widget. */
	void HideOwner(bool bHide);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Figure;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Swords;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SparkCards;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> FlareCards; // a star burst at each sword's hilt, as the swords

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials; // the armour, the blades, the flares and every spark (last): all fade together

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Slashes;

	TArray<FSpark> Sparks;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenParts; // the owner's parts this component hid, to show again
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle Handle;
	bool bWanted = false;
	float Fade = 0.f;
	float Age = 0.f;
	int32 LastSwing = INDEX_NONE; // the montage instance the figure last followed
	int32 NextSlash = 0;
};
