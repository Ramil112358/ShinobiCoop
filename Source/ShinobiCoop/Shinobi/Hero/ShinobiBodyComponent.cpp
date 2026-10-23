#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "ShinobiCoop.h"
#include "Components/LODSyncComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiRetargetAnimInstance.h"

UShinobiBodyComponent::UShinobiBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision); // the capsule and the driver do the colliding
}

bool UShinobiBodyComponent::Wear(const TCHAR* MeshPath, const TCHAR* RetargeterPath, const TCHAR* MetaHumanClassPath)
{
	// Quiet: a character whose model isn't made yet is expected.
	USkeletalMesh* Body = LoadObject<USkeletalMesh>(nullptr, MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UIKRetargeter* Found = Body ? LoadObject<UIKRetargeter>(nullptr, RetargeterPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Found)
	{
		return false;
	}
	// Retargeter first: a new mesh re-initialises the anim instance, which reads it.
	Retargeter = Found;
	SetSkeletalMeshAsset(Body);
	SetAnimInstanceClass(UShinobiRetargetAnimInstance::StaticClass());
	WearMetaHumanParts(MetaHumanClassPath);
	return true;
}

void UShinobiBodyComponent::WearMetaHumanParts(const TCHAR* ClassPath)
{
	for (UActorComponent* Part : MetaHumanParts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	MetaHumanParts.Reset();
	const UBlueprintGeneratedClass* Class = ClassPath ? LoadObject<UBlueprintGeneratedClass>(nullptr, ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	if (!Class || !Class->SimpleConstructionScript || !GetOwner())
	{
		UE_CLOG(ClassPath != nullptr, LogShinobiCoop, Warning, TEXT("%s: no MetaHuman blueprint %s"), *GetNameSafe(GetOwner()), ClassPath);
		return;
	}
	static const FName Body(TEXT("Body")), GroomAsset(TEXT("GroomAsset"));
	UBlueprintGeneratedClass* MutableClass = const_cast<UBlueprintGeneratedClass*>(Class);
	// A copy of the blueprint's component, under its blueprint name (LODSync finds its components by name), not registered yet.
	auto Copy = [this](const UActorComponent* Template, FName Var) -> UActorComponent*
	{
		const FName Name = FindObjectFast<UObject>(GetOwner(), Var) ? MakeUniqueObjectName(GetOwner(), Template->GetClass(), Var) : Var;
		UActorComponent* Part = NewObject<UActorComponent>(GetOwner(), Template->GetClass(), Name, RF_Transient, const_cast<UActorComponent*>(Template));
		MetaHumanParts.Add(Part);
		return Part;
	};
	USkinnedMeshComponent* StrandsFace = nullptr; // what a ShinobiStrands groom is bound to (see below)
	// The blueprint's tree (Root > Body > Face > grooms): this stands in for Root and Body.
	TFunction<void(const USCS_Node*, USceneComponent*)> Wear = [&](const USCS_Node* Node, USceneComponent* Parent)
	{
		const FName Var = Node->GetVariableName();
		const USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(MutableClass));
		USceneComponent* Placed = Parent;
		if (Var == Body)
		{
			Placed = this;
		}
		else if (Template)
		{
			// Grooms left empty (the slots the quality level bakes into the face texture: stubble, lashes, fuzz) are skipped.
			const FObjectProperty* Groom = FindFProperty<FObjectProperty>(Template->GetClass(), GroomAsset);
			if (!Groom || Groom->GetObjectPropertyValue_InContainer(Template))
			{
				Placed = CastChecked<USceneComponent>(Copy(Template, Var));
				Placed->SetupAttachment(Parent, Node->AttachToName);
				// Hair and brows draw their cards even up close: their strands render thin and washed out in our outdoor light
				// (the brows came out light brown), the cards dark and full. Stubble keeps its strands (it has no cards), and so
				// does a groom tagged ShinobiStrands (a Fab hair with untextured cards, Scripts/setup_metahuman.py fab_hair).
				if (Placed->ComponentHasTag(TEXT("ShinobiStrands")))
				{
					StrandsFace = Cast<USkinnedMeshComponent>(Parent);
				}
				else if (Var == TEXT("Hair") || Var == TEXT("Eyebrows"))
				{
					if (FBoolProperty* UseCards = FindFProperty<FBoolProperty>(Placed->GetClass(), TEXT("bUseCards")))
					{
						UseCards->SetPropertyValue_InContainer(Placed, true);
					}
				}
				// Grooms follow a skinned mesh through the GPU skin cache, which this project only gives meshes that ask
				// (r.SkinCache.DefaultBehavior=0): the face asks for every LOD.
				if (USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Placed))
				{
					Skinned->SkinCacheUsage.Init(ESkinCacheUsage::Enabled, 8);
				}
				// The outline is drawn where custom depth ends: the face and hair write the body's, so no seam shows at the neck.
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Placed))
				{
					Primitive->SetCustomDepthStencilValue(CustomDepthStencilValue);
					Primitive->SetRenderCustomDepth(bRenderCustomDepth);
				}
				Placed->RegisterComponent();
				UE_LOG(LogShinobiCoop, Log, TEXT("%s wears MetaHuman part %s (%s) on %s"), *GetNameSafe(GetOwner()), *Placed->GetName(),
					*Template->GetClass()->GetName(), *GetNameSafe(Parent));
			}
		}
		for (const USCS_Node* Child : Node->GetChildNodes())
		{
			Wear(Child, Placed);
		}
	};
	const USCS_Node* LODSyncNode = nullptr;
	for (const USCS_Node* Root : Class->SimpleConstructionScript->GetRootNodes())
	{
		if (Cast<ULODSyncComponent>(Root->GetActualComponentTemplate(MutableClass)))
		{
			LODSyncNode = Root;
		}
		for (const USCS_Node* Child : Root->GetChildNodes()) // the actor's root: this stands in
		{
			Wear(Child, this);
		}
	}
	// LODSync last, once the parts it names exist: the grooms' cards LODs follow the face's (without it a groom can sit
	// on a LOD it has no geometry for). Its "Body" is this.
	if (LODSyncNode)
	{
		ULODSyncComponent* Sync = CastChecked<ULODSyncComponent>(Copy(LODSyncNode->GetActualComponentTemplate(MutableClass), LODSyncNode->GetVariableName()));
		for (FComponentSync& Entry : Sync->ComponentsToSync)
		{
			Entry.Name = Entry.Name == Body ? GetFName() : Entry.Name;
			// A Fab strands hair is bound to the face's LOD0 only: its binding matches the head skin by material, and the
			// face's lower LODs skin the head with other materials, so the hair vanished whenever the face dropped a LOD.
			if (StrandsFace && Entry.Name == FName(*StrandsFace->GetFName().GetPlainNameString()))
			{
				Entry.SyncOption = ESyncOption::Disabled;
			}
		}
		if (StrandsFace)
		{
			StrandsFace->SetForcedLOD(1); // (1 = LOD0)
		}
		if (FLODMappingData* Mapping = Sync->CustomLODMapping.Find(Body))
		{
			const FLODMappingData Moved = *Mapping;
			Sync->CustomLODMapping.Remove(Body);
			Sync->CustomLODMapping.Add(GetFName(), Moved);
		}
		Sync->RegisterComponent();
	}
}

bool UShinobiBodyComponent::WearCharacter(const FString& Name)
{
	const FString Dir = FString::Printf(TEXT("/Game/Shinobi/Characters/%s/"), *Name);
	return Wear(*FString::Printf(TEXT("%sSK_%s.SK_%s"), *Dir, *Name, *Name),
		*FString::Printf(TEXT("%sRTG_MannyTo%s.RTG_MannyTo%s"), *Dir, *Name, *Name));
}

void UShinobiBodyComponent::HideDriver()
{
	if (USkeletalMeshComponent* Driver = Cast<USkeletalMeshComponent>(GetAttachParent()))
	{
		Driver->SetVisibility(false);
		Driver->SetCastShadow(false);
		Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}
