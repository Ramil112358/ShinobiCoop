#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiOwl.generated.h"

class AShinobiCharacterBase;
class UAnimSequence;
class USkeletalMeshComponent;

/**
 * The summoned owl (UGA_SummonOwl): the user's snowy-owl plush, she/her. She hovers at her caster's left shoulder for Lifetime
 * seconds and calls lightning down from the sky onto the nearest targetable hostile in her caster's sight within StrikeRange: Damage
 * that neither staggers nor shoves. She is no combatant: not a character, no health, no collision, so nothing targets or hits
 * her. Server authoritative. Her movement is not replicated: every machine flies her to GetPost of her replicated caster, so she
 * keeps to the shoulder smoothly; only her caster, her target and a strike counter (which plays Strike on clients) replicate.
 */
UCLASS()
class SHINOBICOOP_API AShinobiOwl : public AActor
{
	GENERATED_BODY()

public:
	AShinobiOwl();

	/** Server only: calls Caster's owl to their shoulder, dismissing the one they had out. Null on a client or with no caster. */
	static AShinobiOwl* Summon(AShinobiCharacterBase* Caster);

	/** Where her body's middle hovers beside Caster: PostLeft to their left, PostBehind behind, PostHeight above their feet, in the
	 * caster's own frame (on a wall-run, "above" is away from the wall). */
	static FVector GetPost(const AShinobiCharacterBase& Caster);

	AShinobiCharacterBase* GetSummoner() const { return Summoner; }
	AShinobiCharacterBase* GetTarget() const { return Target; }
	USkeletalMeshComponent* GetBody() const { return Body; }

	/** Server: leaves in a puff of smoke now. */
	void Dismiss();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float Lifetime = 15.f;
	static constexpr float FirstStrikeDelay = 0.5f;
	static constexpr float StrikeInterval = 2.f;
	static constexpr float WindUp = 0.4f;       // from the start of a strike to the bolt landing: the Strike clip's down-beat
	static constexpr float Damage = 55.f;
	static constexpr float StrikeRange = 2000.f; // from the caster
	static constexpr float Scale = 0.45f;        // the 1 m Tripo model at the plush's 45 cm
	static constexpr float PostBehind = 30.f;
	static constexpr float PostLeft = 60.f;      // the left: the camera looks over the caster's right shoulder
	static constexpr float PostHeight = 160.f;
	static constexpr float FollowSpeed = 8.f;    // VInterpTo speed towards the post
	static constexpr float SnapDistance = 1500.f; // farther from the post than this (a teleport, a dragon at speed) she is simply there

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** Server: picks the nearest hostile in sight and starts a strike at it; the bolt lands WindUp later. */
	void StartStrike();
	void LandStrike();
	AShinobiCharacterBase* FindTarget() const;

	UFUNCTION()
	void OnRep_Strikes();
	void PlayClip(UAnimSequence* Clip, bool bLoop);

	UPROPERTY(Replicated)
	TObjectPtr<AShinobiCharacterBase> Summoner;

	UPROPERTY(Replicated)
	TObjectPtr<AShinobiCharacterBase> Target;

	UPROPERTY(ReplicatedUsing = OnRep_Strikes)
	uint8 Strikes = 0;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UAnimSequence> HoverClip;

	UPROPERTY()
	TObjectPtr<UAnimSequence> StrikeClip;

	FTimerHandle LifetimeTimer;
	FTimerHandle StrikeTimer;
	FTimerHandle LandTimer;
};
