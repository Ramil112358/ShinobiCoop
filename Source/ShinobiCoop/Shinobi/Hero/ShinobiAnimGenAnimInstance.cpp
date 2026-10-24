#include "Shinobi/Hero/ShinobiAnimGenAnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "Shinobi/Hero/ShinobiBlueprintStruct.h"

UClass* UShinobiAnimGenAnimInstance::LoadAnimBlueprintClass()
{
	static const TSoftClassPtr<UAnimInstance> AnimBlueprint(FSoftObjectPath(TEXT("/Game/Shinobi/AnimGen/ABP_AnimGenVillager.ABP_AnimGenVillager_C")));
	return AnimBlueprint.LoadSynchronous();
}

FName UShinobiAnimGenAnimInstance::PickStyle(int32 Seed, TOptional<bool> bWoman)
{
	static const TArray<FName> Either = { TEXT("Hurried"), TEXT("Slow"), TEXT("Strut"), TEXT("Tired"), TEXT("Thinking"), TEXT("SpacedOut") };
	static const TArray<FName> Man = { TEXT("Hurried"), TEXT("Slow"), TEXT("Strut"), TEXT("Tired"), TEXT("Thinking"), TEXT("SpacedOut"),
		TEXT("NeutralMale"), TEXT("NeutralMale"), TEXT("ElderlyMale") };
	static const TArray<FName> Woman = { TEXT("Hurried"), TEXT("Slow"), TEXT("Strut"), TEXT("Tired"), TEXT("Thinking"), TEXT("SpacedOut"),
		TEXT("NeutralFemale"), TEXT("NeutralFemale"), TEXT("ElderlyFemale") };
	const TArray<FName>& Styles = !bWoman.IsSet() ? Either : (*bWoman ? Woman : Man);
	return Styles[FMath::Abs(Seed) % Styles.Num()];
}

void UShinobiAnimGenAnimInstance::UpdateAnimGenInputs(float DeltaSeconds)
{
	using namespace ShinobiBlueprintStruct;
	if (const FStructProperty* Properties = CastField<FStructProperty>(GetClass()->FindPropertyByName(TEXT("Properties"))))
	{
		void* Data = Properties->ContainerPtrToValuePtr<void>(this);
		if (const FNameProperty* StyleName = CastField<FNameProperty>(Find(Properties->Struct, TEXT("StyleName"))))
		{
			StyleName->SetPropertyValue_InContainer(Data, Style);
		}
		if (const FIntProperty* LOD = CastField<FIntProperty>(Find(Properties->Struct, TEXT("LODLevel"))))
		{
			LOD->SetPropertyValue_InContainer(Data, LODLevel);
		}
		SetBool(Find(Properties->Struct, TEXT("LookAt")), Data, false);
		SetBool(Find(Properties->Struct, TEXT("DrawDebug")), Data, false);
	}

	// The example predicts the trajectory with Mover; ours comes from the character movement, as in the sample's CMC AnimBP.
	const FStructProperty* TrajectoryProperty = CastField<FStructProperty>(GetClass()->FindPropertyByName(TEXT("Trajectory")));
	const FNumericProperty* YawProperty = CastField<FNumericProperty>(GetClass()->FindPropertyByName(TEXT("TrajectoryYawLastUpdate")));
	if (TrajectoryProperty && TrajectoryProperty->Struct == FTransformTrajectory::StaticStruct() && YawProperty && YawProperty->IsFloatingPoint())
	{
		FTransformTrajectory& Trajectory = *TrajectoryProperty->ContainerPtrToValuePtr<FTransformTrajectory>(this);
		float Yaw = static_cast<float>(YawProperty->GetFloatingPointPropertyValue(YawProperty->ContainerPtrToValuePtr<void>(this)));
		FTransformTrajectory Unused;
		// The example's sampling: 10 samples back every 0.04 s, 10 ahead every 0.1 s.
		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(this, TrajectoryData, DeltaSeconds, Trajectory, Yaw, Unused, 0.04f, 10, 0.1f, 10);
		YawProperty->SetFloatingPointPropertyValue(YawProperty->ContainerPtrToValuePtr<void>(this), Yaw);
	}
}
