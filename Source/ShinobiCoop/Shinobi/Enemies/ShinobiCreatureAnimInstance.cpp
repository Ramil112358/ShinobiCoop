#include "Shinobi/Enemies/ShinobiCreatureAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiTags.h"

namespace
{
	constexpr float CreatureStandBelow = 15.f;     // moving slower than this from a walk: stand
	constexpr float CreatureMoveAbove = 40.f;      // moving faster than this from a stand: walk
	constexpr float CreatureMinRate = 0.5f;
	constexpr float CreatureMaxRate = 1.5f;
	constexpr float CreatureHitSeconds = 0.45f;    // how long a flinch holds the body
	constexpr float CreatureHitCooldown = 0.8f;    // so a burn or a flurry does not keep it flinching
}

bool FShinobiCreatureAnimProxy::Evaluate(FPoseContext& Output)
{
	Output.ResetToRefPose();
	FAnimationPoseData Result(Output);
	float Total = 0.f;
	for (const FShinobiCreatureLayer& Layer : Layers)
	{
		if (!Layer.Clip || Layer.Weight <= 0.f)
		{
			continue;
		}
		const FAnimExtractContext Extract(static_cast<double>(Layer.Time), false, FDeltaTimeRecord(), Layer.bLoop);
		if (Total <= 0.f)
		{
			Layer.Clip->GetAnimationPose(Result, Extract);
			Total = Layer.Weight;
			continue;
		}
		FCompactPose Pose;
		FBlendedCurve Curve;
		UE::Anim::FStackAttributeContainer Attributes;
		Pose.SetBoneContainer(&Output.Pose.GetBoneContainer());
		Curve.InitFrom(Output.Curve);
		FAnimationPoseData Other(Pose, Curve, Attributes);
		Layer.Clip->GetAnimationPose(Other, Extract);
		FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result, Other, Total / (Total + Layer.Weight));
		Total += Layer.Weight;
	}
	return true;
}

void UShinobiCreatureAnimInstance::Setup(const FShinobiCreatureClips& InClips)
{
	Clips = InClips;
}

UAnimSequence* UShinobiCreatureAnimInstance::GetCurrentClip() const
{
	const TArray<FShinobiCreatureLayer>& Layers = const_cast<UShinobiCreatureAnimInstance*>(this)->GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers;
	return Layers.Num() > 0 ? Layers.Last().Clip : nullptr;
}

float UShinobiCreatureAnimInstance::GetCurrentRate() const
{
	const TArray<FShinobiCreatureLayer>& Layers = const_cast<UShinobiCreatureAnimInstance*>(this)->GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers;
	return Layers.Num() > 0 ? Layers.Last().Rate : 0.f;
}

int32 UShinobiCreatureAnimInstance::GetLayerCount() const
{
	return const_cast<UShinobiCreatureAnimInstance*>(this)->GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers.Num();
}

TArray<float> UShinobiCreatureAnimInstance::GetLayerWeights() const
{
	TArray<float> Weights;
	for (const FShinobiCreatureLayer& Layer : const_cast<UShinobiCreatureAnimInstance*>(this)->GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers)
	{
		Weights.Add(Layer.Weight);
	}
	return Weights;
}

void UShinobiCreatureAnimInstance::Play(UAnimSequence* Clip, bool bLoop, float Rate)
{
	TArray<FShinobiCreatureLayer>& Layers = GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers;
	FShinobiCreatureLayer Layer;
	Layer.Clip = Clip;
	Layer.bLoop = bLoop;
	Layer.Rate = Rate;
	Layer.Weight = Layers.Num() == 0 ? 1.f : 0.f; // the first clip is simply there; later ones fade in over the old
	Layers.Add(Layer);
}

void UShinobiCreatureAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	TArray<FShinobiCreatureLayer>& Layers = GetProxyOnGameThread<FShinobiCreatureAnimProxy>().Layers;

	// Time and blend weights move on: the newest clip fades in, the others out, and a clip that has faded out is dropped.
	for (int32 Index = 0; Index < Layers.Num(); ++Index)
	{
		FShinobiCreatureLayer& Layer = Layers[Index];
		const float Length = Layer.Clip ? Layer.Clip->GetPlayLength() : 0.f;
		Layer.Time += DeltaSeconds * Layer.Rate * (Layer.Clip ? Layer.Clip->RateScale : 1.f);
		Layer.Time = Layer.bLoop && Length > 0.f ? FMath::Fmod(Layer.Time, Length) : FMath::Min(Layer.Time, Length);
		const bool bNewest = Index == Layers.Num() - 1;
		Layer.Weight = bNewest ? FMath::Min(Layer.Weight + DeltaSeconds / BlendSeconds, 1.f) : FMath::Max(Layer.Weight - DeltaSeconds / BlendSeconds, 0.f);
	}
	for (int32 Index = Layers.Num() - 2; Index >= 0; --Index)
	{
		if (Layers[Index].Weight <= 0.f)
		{
			Layers.RemoveAt(Index);
		}
	}

	const AShinobiCharacterBase* Owner = Cast<AShinobiCharacterBase>(GetOwningActor());
	if (!Owner || !Clips.Idle || !Owner->GetAttributes())
	{
		return;
	}

	// What the creature is doing, from state every machine has.
	const bool bDead = !Owner->IsAlive();
	const bool bAttacking = Owner->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_SummonAttacking);
	const float Health = Owner->GetAttributes()->GetHealth();
	bool bFlinch = false;
	HitRemaining -= DeltaSeconds;
	HitCooldown -= DeltaSeconds;
	if (LastHealth >= 0.f && Health < LastHealth - 0.5f && !bDead && !bAttacking && Clips.Hit && HitCooldown <= 0.f)
	{
		bFlinch = true;
		HitRemaining = FMath::Min(Clips.Hit->GetPlayLength(), CreatureHitSeconds);
		HitCooldown = CreatureHitCooldown;
	}
	LastHealth = Health;

	const float Speed = Owner->GetVelocity().Size2D();
	EState Want = State;
	if (bDead)
	{
		Want = Clips.Death ? EState::Dead : State; // without a death clip the body ragdolls and this stops mattering
	}
	else if (bAttacking && Clips.Attack)
	{
		Want = EState::Attack;
	}
	else if (bFlinch || (State == EState::Hit && HitRemaining > 0.f))
	{
		Want = EState::Hit;
	}
	else
	{
		const float RunFrom = Clips.Run && Clips.Walk ? 0.5f * (Clips.WalkSpeed + Clips.RunSpeed) : TNumericLimits<float>::Max();
		const bool bRunning = State == EState::Run;
		const bool bStanding = State == EState::Idle || State == EState::None || State == EState::Hit || State == EState::Attack;
		if (Speed < (bStanding ? CreatureMoveAbove : CreatureStandBelow))
		{
			Want = EState::Idle;
		}
		else if (Clips.Run && (!Clips.Walk || Speed > RunFrom * (bRunning ? 0.9f : 1.1f)))
		{
			Want = EState::Run;
		}
		else
		{
			Want = Clips.Walk ? EState::Walk : EState::Idle;
		}
	}

	if (State == EState::Dead)
	{
		return; // lying where it fell
	}
	if (Want != State || Layers.Num() == 0)
	{
		State = Want;
		switch (State)
		{
		case EState::Attack:
			Play(Clips.Attack, false, Clips.AttackSeconds > 0.f ? Clips.Attack->GetPlayLength() / Clips.AttackSeconds : 1.f);
			break;
		case EState::Hit:
			Play(Clips.Hit, false, 1.f);
			break;
		case EState::Dead:
			Play(Clips.Death, false, 1.f);
			break;
		case EState::Run:
			Play(Clips.Run, true, 1.f);
			break;
		case EState::Walk:
			Play(Clips.Walk, true, 1.f);
			break;
		default:
			State = EState::Idle;
			Play(Clips.Idle, true, 1.f);
			break;
		}
	}

	// Paws meet the ground at the speed the clip was made for.
	if (Layers.Num() > 0 && (State == EState::Walk || State == EState::Run))
	{
		const float Nominal = State == EState::Run ? Clips.RunSpeed : Clips.WalkSpeed;
		Layers.Last().Rate = FMath::Clamp(Speed / FMath::Max(Nominal, 1.f), CreatureMinRate, CreatureMaxRate);
	}
}
