#include "Shinobi/VFX/ShinobiPerfectSusanooComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Enemies/ShinobiCreatureAnimInstance.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	constexpr float SusanooFadeInSeconds = 0.8f;
	constexpr float SusanooFadeOutSeconds = 0.5f;
	constexpr int32 SusanooSparkCount = 18;
	constexpr float SusanooSparkRadius = 450.f;
	const FLinearColor SusanooBlade(0.08f, 0.45f, 1.f);
	const FLinearColor SusanooSpark(0.3f, 0.75f, 1.f);
	const TCHAR* SusanooDir = TEXT("/Game/Shinobi/Jutsu/Susanoo/");

	template <typename T>
	T* LoadSusanoo(const FString& Name)
	{
		return LoadObject<T>(nullptr, *(SusanooDir + Name + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The bone whose name ends with Suffix as a whole word ("mixamorig:RightHand", not "RightHandIndex1"), or INDEX_NONE. */
	int32 FindBone(const FReferenceSkeleton& Skeleton, const FString& Suffix)
	{
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			const FString Name = Skeleton.GetBoneName(Index).ToString();
			if (Name.EndsWith(Suffix) && (Name.Len() == Suffix.Len() || !FChar::IsAlnum(Name[Name.Len() - Suffix.Len() - 1])))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Where a bone stands in the figure's rest pose (component space, before the figure is scaled), or the origin. */
	FTransform RestPose(const FReferenceSkeleton& Skeleton, int32 Bone)
	{
		return Bone == INDEX_NONE ? FTransform::Identity : FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone);
	}
}

bool FShinobiSusanooAnimProxy::Evaluate(FPoseContext& Output)
{
	FShinobiCreatureAnimProxy::Evaluate(Output); // the legs: stance or stride
	if (!Upper.Clip || Upper.Weight <= 0.f || UpperRoot.IsNone())
	{
		return true;
	}
	FCompactPose Pose;
	FBlendedCurve Curve;
	UE::Anim::FStackAttributeContainer Attributes;
	Pose.SetBoneContainer(&Output.Pose.GetBoneContainer());
	Curve.InitFrom(Output.Curve);
	FAnimationPoseData Swing(Pose, Curve, Attributes);
	Upper.Clip->GetAnimationPose(Swing, FAnimExtractContext(static_cast<double>(Upper.Time), false, FDeltaTimeRecord(), false));
	// The spine and everything above it take the swing; the hips and legs keep theirs. Parents come before children.
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	TArray<bool, TInlineAllocator<96>> bUpper;
	bUpper.SetNumZeroed(Output.Pose.GetNumBones());
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		bUpper[Index.GetInt()] = RefSkeleton.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt()) == UpperRoot || (Parent.IsValid() && bUpper[Parent.GetInt()]);
		if (bUpper[Index.GetInt()])
		{
			Output.Pose[Index].BlendWith(Pose[Index], Upper.Weight);
		}
	}
	return true;
}

FAnimInstanceProxy* UShinobiSusanooAnimInstance::CreateAnimInstanceProxy()
{
	return new FShinobiSusanooAnimProxy(this);
}

void UShinobiSusanooAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FShinobiSusanooAnimProxy*>(InProxy);
}

void UShinobiSusanooAnimInstance::Setup(const FShinobiSusanooClips& InClips)
{
	Clips = InClips;
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (const USkeletalMesh* Model = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr)
	{
		const int32 Spine = FindBone(Model->GetRefSkeleton(), TEXT("Spine"));
		GetProxyOnGameThread<FShinobiSusanooAnimProxy>().UpperRoot = Spine == INDEX_NONE ? NAME_None : Model->GetRefSkeleton().GetBoneName(Spine);
	}
}

void UShinobiSusanooAnimInstance::Slash(UAnimSequence* Clip)
{
	if (!Clip)
	{
		return;
	}
	FShinobiCreatureLayer& Upper = GetProxyOnGameThread<FShinobiSusanooAnimProxy>().Upper;
	const bool bAlreadySwinging = Upper.Clip && Upper.Weight > 0.f;
	Upper.Clip = Clip;
	Upper.Time = 0.f;
	Upper.bLoop = false;
	Upper.Rate = FMath::Max(1.f, Clip->GetPlayLength() / SlashSeconds);
	Upper.Weight = bAlreadySwinging ? Upper.Weight : 0.f; // a new hit takes over the arms from wherever the last one had them
}

UAnimSequence* UShinobiSusanooAnimInstance::GetCurrentClip() const
{
	const TArray<FShinobiCreatureLayer>& Layers = const_cast<UShinobiSusanooAnimInstance*>(this)->GetProxyOnGameThread<FShinobiSusanooAnimProxy>().Layers;
	return Layers.Num() > 0 ? Layers.Last().Clip : nullptr;
}

UAnimSequence* UShinobiSusanooAnimInstance::GetSlashClip() const
{
	const FShinobiCreatureLayer& Upper = const_cast<UShinobiSusanooAnimInstance*>(this)->GetProxyOnGameThread<FShinobiSusanooAnimProxy>().Upper;
	return Upper.Weight > 0.f ? Upper.Clip : nullptr;
}

void UShinobiSusanooAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	FShinobiSusanooAnimProxy& Proxy = GetProxyOnGameThread<FShinobiSusanooAnimProxy>();
	Proxy.Advance(DeltaSeconds, BlendSeconds);

	// The slash: in quickly, out as it nears its end.
	FShinobiCreatureLayer& Upper = Proxy.Upper;
	if (Upper.Clip)
	{
		const float Length = Upper.Clip->GetPlayLength();
		Upper.Time = FMath::Min(Upper.Time + DeltaSeconds * Upper.Rate, Length);
		const bool bEnding = Upper.Time >= Length - SlashBlendSeconds * Upper.Rate;
		Upper.Weight = FMath::Clamp(Upper.Weight + (bEnding ? -1.f : 1.f) * DeltaSeconds / SlashBlendSeconds, 0.f, 1.f);
		if (bEnding && Upper.Weight <= 0.f)
		{
			Upper.Clip = nullptr;
		}
	}

	// The legs follow the wearer: stand, walk or run, the stride matched to their speed.
	if (!Clips.Idle)
	{
		return;
	}
	const AActor* Owner = GetOwningActor();
	const float Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
	UAnimSequence* Current = GetCurrentClip();
	const bool bMoving = Speed > (Current == Clips.Idle ? 60.f : 30.f);
	UAnimSequence* Want = !bMoving || !Clips.Walk ? Clips.Idle.Get() : (Clips.Run && Speed > RunAbove ? Clips.Run.Get() : Clips.Walk.Get());
	if (Want != Current)
	{
		Proxy.Play(Want, true, 1.f);
	}
	if (Want != Clips.Idle && Proxy.Layers.Num() > 0)
	{
		Proxy.Layers.Last().Rate = FMath::Clamp(Speed / (Want == Clips.Run ? RunSpeed : WalkSpeed), 0.4f, 1.5f);
	}
}

UShinobiPerfectSusanooComponent::UShinobiPerfectSusanooComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only while the figure shows or fades
}

void UShinobiPerfectSusanooComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsNetMode(NM_DedicatedServer))
	{
		return;
	}
	if (const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwner()))
	{
		AbilitySystem = Owner->GetAbilitySystemComponent();
	}
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		Handle = ASC->RegisterGameplayTagEvent(ShinobiTags::State_Susanoo).AddUObject(this, &UShinobiPerfectSusanooComponent::OnSusanooChanged);
		bWanted = ASC->HasMatchingGameplayTag(ShinobiTags::State_Susanoo); // a client that joins mid-armour
		if (bWanted)
		{
			EnsureFigure();
		}
		SetComponentTickEnabled(bWanted);
	}
}

UShinobiSusanooAnimInstance* UShinobiPerfectSusanooComponent::GetFigureAnim() const
{
	return Figure ? Cast<UShinobiSusanooAnimInstance>(Figure->GetAnimInstance()) : nullptr;
}

void UShinobiPerfectSusanooComponent::EnsureFigure()
{
	if (Figure)
	{
		return;
	}
	// Built the first time the armour goes up, not at spawn: most characters (every enemy and villager) never wear it.
	// SK_Susanoo is 180 cm with its feet at its origin (Scripts/character/build_susanoo.py); it is scaled up to FigureHeight.
	USkeletalMesh* Model = LoadSusanoo<USkeletalMesh>(TEXT("SK_Susanoo"));
	FShinobiSusanooClips Clips;
	Clips.Idle = LoadSusanoo<UAnimSequence>(TEXT("A_Susanoo_Idle"));
	Clips.Walk = LoadSusanoo<UAnimSequence>(TEXT("A_Susanoo_Walk"));
	Clips.Run = LoadSusanoo<UAnimSequence>(TEXT("A_Susanoo_Run"));
	if (!Model || !Clips.Idle)
	{
		return;
	}
	for (int32 Index = 1; UAnimSequence* Clip = LoadSusanoo<UAnimSequence>(FString::Printf(TEXT("A_Susanoo_Slash%d"), Index)); ++Index)
	{
		Slashes.Add(Clip);
	}
	AActor* Owner = GetOwner();
	const FReferenceSkeleton& Skeleton = Model->GetRefSkeleton();

	// Which way the model faces: from its heels to its toes.
	const FVector Toes = RestPose(Skeleton, FindBone(Skeleton, TEXT("LeftToeBase"))).GetLocation() - RestPose(Skeleton, FindBone(Skeleton, TEXT("LeftFoot"))).GetLocation();
	const FVector Forward = FVector(Toes.X, Toes.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector::YAxisVector);
	const float Scale = FigureHeight / FMath::Max(2.f * Model->GetImportedBounds().BoxExtent.Z, 1.f);

	Figure = NewObject<USkeletalMeshComponent>(Owner, MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), TEXT("SusanooFigure")));
	Figure->SetSkeletalMesh(Model);
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetCastShadow(false);
	Figure->SetVisibility(false, true);
	Figure->SetAnimInstanceClass(UShinobiSusanooAnimInstance::StaticClass());
	Figure->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones; // the swords follow the hands even off screen
	UMaterialInstanceDynamic* Armour = ShinobiVfx::MakeSusanoo(this, 0.f);
	for (int32 Slot = 0; Slot < Figure->GetNumMaterials(); ++Slot)
	{
		Figure->SetMaterial(Slot, Armour);
	}
	Materials.Add(Armour);
	Figure->SetupAttachment(Owner->GetRootComponent());
	const ACharacter* Character = Cast<ACharacter>(Owner);
	Figure->SetRelativeLocation(FVector(0.f, 0.f, Character ? -Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f));
	Figure->SetRelativeRotation(FRotator(0.f, -Forward.Rotation().Yaw, 0.f)); // face the owner's forward
	Figure->SetRelativeScale3D(FVector(Scale));
	Figure->RegisterComponent();
	if (UShinobiSusanooAnimInstance* Anim = GetFigureAnim())
	{
		Anim->Setup(Clips);
	}

	// A sword in each fist, gripped across the palm with the blade pointing forward out of the thumb side (in the rest A-pose
	// the hands hang at the sides, thumbs forward). Attached to the hand bones, so they swing with every clip.
	UStaticMesh* Blade = LoadSusanoo<UStaticMesh>(TEXT("SM_SusanooSword"));
	UMaterialInstanceDynamic* Glow = Blade ? ShinobiVfx::MakeGlow(this, SusanooBlade, 2.5f, 1.f) : nullptr;
	if (Glow)
	{
		Glow->SetScalarParameterValue(TEXT("Fade"), 0.f);
		Materials.Add(Glow);
	}
	for (const TCHAR* Side : {TEXT("Right"), TEXT("Left")})
	{
		const int32 Hand = FindBone(Skeleton, FString(Side) + TEXT("Hand"));
		int32 Knuckle = FindBone(Skeleton, FString(Side) + TEXT("HandMiddle1"));
		Knuckle = Knuckle == INDEX_NONE ? FindBone(Skeleton, FString(Side) + TEXT("HandIndex1")) : Knuckle;
		if (!Blade || Hand == INDEX_NONE)
		{
			continue;
		}
		const FTransform HandPose = RestPose(Skeleton, Hand);
		const FVector Grip = FMath::Lerp(HandPose.GetLocation(), Knuckle == INDEX_NONE ? HandPose.GetLocation() : RestPose(Skeleton, Knuckle).GetLocation(), 0.55f);
		const FTransform SwordPose(FRotationMatrix::MakeFromZX(Forward, FVector::UpVector).ToQuat(), Grip);
		UStaticMeshComponent* Sword = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("SusanooSword")));
		Sword->SetStaticMesh(Blade);
		Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sword->SetCastShadow(false);
		Sword->SetMaterial(0, Glow);
		Sword->SetupAttachment(Figure, Skeleton.GetBoneName(Hand));
		Sword->SetRelativeTransform(SwordPose.GetRelativeTransform(HandPose));
		Sword->RegisterComponent();
		Sword->SetVisibility(false);
		Swords.Add(Sword);
	}

	// Sparks: small star bursts drifting up through and around the figure, flickering between the sheet's variants.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FRandomStream Random(GetTypeHash(Owner->GetFName()));
	for (int32 Index = 0; Plane && Index < SusanooSparkCount; ++Index)
	{
		FSpark Spark;
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const float Radius = SusanooSparkRadius * FMath::Sqrt(Random.FRand());
		Spark.Offset = FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Random.FRandRange(0.f, FigureHeight));
		Spark.Size = Random.FRandRange(35.f, 90.f);
		Spark.Speed = Random.FRandRange(60.f, 160.f);
		Spark.Variant = Random.RandRange(0, ShinobiVfx::SpriteVariants(ShinobiVfx::ESprite::Burst) - 1);
		UMaterialInstanceDynamic* Look = ShinobiVfx::MakeSprite(this, ShinobiVfx::ESprite::Burst, Spark.Variant, SusanooSpark, 2.f);
		if (!Look)
		{
			break;
		}
		Look->SetScalarParameterValue(TEXT("Fade"), 0.f);
		UStaticMeshComponent* Card = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("SusanooSpark")));
		Card->SetStaticMesh(Plane);
		Card->SetMaterial(0, Look);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->SetCastShadow(false);
		Card->SetUsingAbsoluteLocation(true);
		Card->SetUsingAbsoluteRotation(true);
		Card->SetUsingAbsoluteScale(true);
		Card->SetupAttachment(Owner->GetRootComponent());
		Card->RegisterComponent();
		Card->SetVisibility(false);
		Materials.Add(Look);
		SparkCards.Add(Card);
		Sparks.Add(Spark);
	}
}

void UShinobiPerfectSusanooComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAbilitySystemComponent* ASC = AbilitySystem.Get())
	{
		ASC->UnregisterGameplayTagEvent(Handle, ShinobiTags::State_Susanoo);
	}
	HideOwner(false);
	for (UStaticMeshComponent* Part : Swords)
	{
		Part->DestroyComponent();
	}
	for (UStaticMeshComponent* Part : SparkCards)
	{
		Part->DestroyComponent();
	}
	if (Figure)
	{
		Figure->DestroyComponent();
	}
	Swords.Reset();
	SparkCards.Reset();
	Sparks.Reset();
	Figure = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UShinobiPerfectSusanooComponent::OnSusanooChanged(const FGameplayTag Tag, int32 Count)
{
	bWanted = Count > 0;
	if (bWanted)
	{
		EnsureFigure();
	}
	SetComponentTickEnabled(true); // fade in or out
}

void UShinobiPerfectSusanooComponent::Slash(int32 ComboIndex)
{
	UShinobiSusanooAnimInstance* Anim = GetFigureAnim();
	if (!Anim || Slashes.Num() == 0)
	{
		return;
	}
	// Hit 1: right blade, hit 2: left blade (a mirrored clip), the finisher: whatever slashes are left, taking turns.
	const int32 Fixed = FMath::Min(2, Slashes.Num());
	const int32 Pick = ComboIndex < Fixed ? ComboIndex : (Slashes.Num() > Fixed ? Fixed + Finishers++ % (Slashes.Num() - Fixed) : Slashes.Num() - 1);
	Anim->Slash(Slashes[Pick]);
}

void UShinobiPerfectSusanooComponent::FollowOwnerSwing()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Body = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* Playing = Body ? Body->GetActiveMontageInstance() : nullptr;
	if (!Playing || !Playing->Montage)
	{
		LastSwing = INDEX_NONE;
		return;
	}
	if (Playing->GetInstanceID() == LastSwing)
	{
		return;
	}
	LastSwing = Playing->GetInstanceID();
	const TArray<TSoftObjectPtr<UAnimMontage>>& Combo = GetDefault<UGA_Melee>()->ComboMontages;
	for (int32 Index = 0; Index < Combo.Num(); ++Index)
	{
		if (Combo[Index].Get() == Playing->Montage)
		{
			Slash(Index);
			return;
		}
	}
}

void UShinobiPerfectSusanooComponent::UpdateSparks(float DeltaTime)
{
	if (!Figure || SparkCards.Num() == 0)
	{
		return;
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const FVector Feet = Figure->GetComponentLocation();
	const FVector CameraAt = Camera ? Camera->GetCameraLocation() : Feet - 2000.f * GetOwner()->GetActorForwardVector();
	for (int32 Index = 0; Index < Sparks.Num(); ++Index)
	{
		FSpark& Spark = Sparks[Index];
		Spark.Offset.Z += Spark.Speed * DeltaTime;
		if (Spark.Offset.Z > FigureHeight)
		{
			Spark.Offset.Z -= FigureHeight; // round again from the feet
		}
		if (Age >= Spark.NextFlicker)
		{
			Spark.NextFlicker = Age + 0.12f;
			Spark.Variant = (Spark.Variant + 1) % ShinobiVfx::SpriteVariants(ShinobiVfx::ESprite::Burst);
			ShinobiVfx::SetSpriteVariant(Materials[Materials.Num() - Sparks.Num() + Index], ShinobiVfx::ESprite::Burst, Spark.Variant);
		}
		const FVector At = Feet + Spark.Offset;
		const float FaceYaw = FMath::RadiansToDegrees(FMath::Atan2(CameraAt.Y - At.Y, CameraAt.X - At.X)) - 90.f;
		UStaticMeshComponent* Card = SparkCards[Index];
		Card->SetWorldLocationAndRotation(At, FRotator(0.f, FaceYaw, 90.f));
		Card->SetWorldScale3D(FVector(Spark.Size / 100.f, Spark.Size / 100.f, 1.f));
	}
}

void UShinobiPerfectSusanooComponent::HideOwner(bool bHide)
{
	if (!bHide)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Part : HiddenParts)
		{
			if (UPrimitiveComponent* Shown = Part.Get())
			{
				Shown->SetHiddenInGame(false);
			}
		}
		HiddenParts.Reset();
		return;
	}
	// The hidden-in-game flag, not visibility: the character's own code switches its meshes' visibility (outfits, the hidden
	// animation driver), and the two must not undo each other. Parts already hidden stay as they are and are not restored.
	TInlineComponentArray<UPrimitiveComponent*> Parts(GetOwner());
	for (UPrimitiveComponent* Part : Parts)
	{
		const bool bOurs = Part == Figure || Part->GetAttachParent() == Figure || SparkCards.Contains(Part);
		if (!bOurs && !Part->IsA<UWidgetComponent>() && !Part->bHiddenInGame)
		{
			Part->SetHiddenInGame(true);
			HiddenParts.Add(Part);
		}
	}
}

void UShinobiPerfectSusanooComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Age += DeltaTime;
	Fade = bWanted ? FMath::Min(Fade + DeltaTime / SusanooFadeInSeconds, 1.f) : FMath::Max(Fade - DeltaTime / SusanooFadeOutSeconds, 0.f);
	if (Figure)
	{
		Figure->SetVisibility(Fade > 0.f, true); // the swords too (attached to it)
		for (UStaticMeshComponent* Card : SparkCards)
		{
			Card->SetVisibility(Fade > 0.f);
		}
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
		}
		FollowOwnerSwing();
		UpdateSparks(DeltaTime);
	}
	const bool bHideOwner = Figure && bWanted && Fade >= 0.5f; // the body vanishes into the rising figure, and is back as it ends
	if (bHideOwner != (HiddenParts.Num() > 0))
	{
		HideOwner(bHideOwner);
	}
	if (!bWanted && Fade <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}
