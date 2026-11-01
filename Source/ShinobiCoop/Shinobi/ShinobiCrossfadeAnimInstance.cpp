#include "Shinobi/ShinobiCrossfadeAnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"

float UShinobiCrossfadeAnimInstance::FTrack::Length() const
{
	return Clip ? Clip->GetPlayLength() : 0.f;
}

void UShinobiCrossfadeAnimInstance::FTrack::Advance(float DeltaSeconds)
{
	const float End = Length();
	Time += DeltaSeconds * Rate;
	Time = bLoop && End > 0.f ? FMath::Fmod(Time, End) : FMath::Min(Time, End);
}

void UShinobiCrossfadeAnimInstance::Play(UAnimSequence* Clip, bool bLoop, float Rate, float BlendSeconds)
{
	if (Clip == Current.Clip)
	{
		Current.Rate = Rate;
		return;
	}
	Previous = Current;
	Current = FTrack{ Clip, 0.f, Rate, bLoop };
	Blend = Previous.Clip && BlendSeconds > 0.f ? 0.f : 1.f;
	BlendRate = BlendSeconds > 0.f ? 1.f / BlendSeconds : 0.f;
}

void UShinobiCrossfadeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	Current.Advance(DeltaSeconds);
	Previous.Advance(DeltaSeconds);
	Blend = FMath::Min(1.f, Blend + DeltaSeconds * BlendRate);
}

struct FShinobiCrossfadeProxy : FAnimInstanceProxy
{
	using FAnimInstanceProxy::FAnimInstanceProxy;

	UShinobiCrossfadeAnimInstance::FTrack Current;
	UShinobiCrossfadeAnimInstance::FTrack Previous;
	float Blend = 1.f;

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		const UShinobiCrossfadeAnimInstance* Owner = CastChecked<UShinobiCrossfadeAnimInstance>(InAnimInstance);
		Current = Owner->Current;
		Previous = Owner->Previous;
		Blend = Owner->Blend;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (!Current.Clip)
		{
			return false; // the reference pose
		}
		Sample(Current, Output);
		if (Blend < 1.f && Previous.Clip)
		{
			FPoseContext Old(Output);
			Sample(Previous, Old);
			FAnimationPoseData Mixed(Output);
			FAnimationRuntime::BlendTwoPosesTogetherInPlace(Mixed, FAnimationPoseData(Old), Blend);
		}
		return true;
	}

	static void Sample(const UShinobiCrossfadeAnimInstance::FTrack& Track, FPoseContext& Pose)
	{
		FAnimationPoseData Data(Pose);
		Track.Clip->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Track.Time), false, {}, Track.bLoop));
	}
};

FAnimInstanceProxy* UShinobiCrossfadeAnimInstance::CreateAnimInstanceProxy()
{
	return new FShinobiCrossfadeProxy(this);
}

void UShinobiCrossfadeAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
