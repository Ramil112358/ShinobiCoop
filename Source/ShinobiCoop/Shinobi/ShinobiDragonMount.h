#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShinobiDragonMount.generated.h"

class AShinobiCharacterBase;

/** The dragons a rider can summon: Tairn (Fourth Wing's black Morningstartail) and the crimson dragon that came first. */
UENUM()
enum class EShinobiDragonKind : uint8
{
	Tairn,
	Crimson,
};
class UAnimSequence;
class USkeletalMeshComponent;
class UStaticMeshComponent;

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
	static AShinobiDragonMount* Spawn(AShinobiCharacterBase* Rider, EShinobiDragonKind Kind = EShinobiDragonKind::Tairn);

	/** The dragon Rider is sitting on, or null. */
	static AShinobiDragonMount* Find(const AShinobiCharacterBase& Rider);

	EShinobiDragonKind GetKind() const { return Kind; }

	AShinobiCharacterBase* GetRider() const { return Rider; }
	USkeletalMeshComponent* GetBody() const { return Body; }

	/** Where the fire is drawn from: the mouth on the animated skull (bank, bob and breath included); MouthLocation where there is no skeleton. */
	FVector GetMouthLocation() const;
	/** Where the breath's damage comes from: the mouth in the breath's pose, at a fixed offset from the rider (the same on every machine). */
	static FVector MouthLocation(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind = EShinobiDragonKind::Tairn);

	/** The breath clip opens the jaws by its 10th frame (of 30 a second) and holds them open to its 40th; played at this rate the opening
	 * takes BreathOpenSeconds and the hold lasts the breath's pulses (UGA_DragonBreath, 5 x 0.3 s), so the fire comes out of an open mouth. */
	static constexpr float BreathOpenSeconds = 0.5f;
	static constexpr float BreathPlayRate = (10.f / 30.f) / BreathOpenSeconds;

	/** How far the fire reaches from the mouth. */
	static constexpr float BreathRange = 1500.f;

	/** How far the fire is tipped down from level, in degrees: enough to bring it onto things below a flying dragon. */
	static constexpr float BreathPitch = 10.f;

	/** The way the fire goes: straight ahead of the dragon, tipped down by BreathPitch. The rider faces the camera's yaw while mounted, so you aim it by turning. */
	static FVector BreathDirection(const AShinobiCharacterBase& Rider);

	/** Where the fire ends: BreathRange ahead of the mouth, or the first thing it meets. The HUD's crosshair marks this point. */
	static FVector BreathTarget(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind = EShinobiDragonKind::Tairn);

	/** The dragon's boxes about the seat (the rider's capsule floor), in the rider's frame, for UShinobiMovementComponent: the reach of each
	 * part of the model (Tairn: body, wings, tail) in every clip, leaning as far as it banks and pitches (flying) or leans into a turn
	 * (on the ground), their floors MountedStepClearance above its feet when standing. */
	static TArray<FBox> Room(EShinobiDragonKind Kind, bool bFlying);

	/** Whether Kind fits where Rider is: standing on the ground below, or flying if there is none near. The summon fails without it. */
	static bool HasRoom(const AShinobiCharacterBase& Rider, EShinobiDragonKind Kind);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UFUNCTION()
	void OnRep_Rider();

private:
	/** Flying, landing (once), on the ground (standing or walking), taking off (once). Each machine decides from the ground below the
	 * seat and the rider's velocity. */
	enum class EGait : uint8 { Fly, Land, Ground, TakeOff };

	void Play(UAnimSequence* Clip, bool bLoop, float Rate = 1.f, float BlendSeconds = 0.35f);

	/** Picks the gait and its clip. */
	void UpdateClip();

	/** Rides the rider's mesh (smoothed on clients). Retried each tick until the rider is known: its channel can open after ours. */
	void AttachToRider();

	/** Leans the dragon about the seat: the right wing drops in a right turn, the nose rises in a climb. Cosmetic, computed on each machine. */
	void UpdateBank(float DeltaSeconds);

	/** Hangs the dragon from the seat by its back (the saddle on its spine bone), so the rider stays on it whatever the clip: the body's
	 * bob, landing, the raised chest standing. Cosmetic, on each machine with a mesh. */
	void KeepSeatStill();

	/** Lays the reins: a rope collar round the neck and a line from it to each of the rider's fists. Cosmetic, on each machine with a mesh. */
	void UpdateReins();

	/** The rider answers the dragon: the torso leans into a turn and with the climb or dive, the rein on the turning side is drawn back (both
	 * back climbing, eased forward diving), and breathing fire braces them (forward, both fists drawn in). Cosmetic, on each machine with a mesh. */
	void UpdateRiderPose(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Set before FinishSpawning, so clients have it before BeginPlay. */
	UPROPERTY(Replicated)
	EShinobiDragonKind Kind = EShinobiDragonKind::Tairn;

	UPROPERTY(ReplicatedUsing = OnRep_Rider)
	TObjectPtr<AShinobiCharacterBase> Rider;

	/** The clips, by name (Idle, Flap, Glide, Breath, BreathStand, Stand, Walk, Run, Land, TakeOff); a missing one is null. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UAnimSequence>> Clips;

	EGait Gait = EGait::Fly;

	/** The collar's segments, then the left and right reins. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Rope;

	/** The saddle in its spine bone's frame, from the flying pose; INDEX_NONE: hang by the fixed Saddle. */
	int32 SaddleBone = INDEX_NONE;
	FVector SaddleInBone = FVector::ZeroVector;

	/** The mouth in the skull bone's frame, from the reference pose. */
	FVector MouthInSkull = FVector::ZeroVector;

	FTransform MeshDefaultInverse = FTransform::Identity;
	float LastRiderYaw = 0.f;
	bool bHaveRiderYaw = false;
	float BankRoll = 0.f;
	float YawRate = 0.f; // the rider's turn, degrees a second (UpdateBank)
	float BankPitch = 0.f;
	float ReinPull[2] = { 0.f, 0.f }; // the rider's left and right rein, as UpdateRiderPose last set them
	float Brace = 0.f;                // 0..1: the rider braced against the breath
};
