#include "Shinobi/Hero/ShinobiMutableLook.h"

#include "ShinobiCoop.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"

FString UShinobiMutableLook::ObjectPathOverride;

namespace
{
	/** The Index-th row of a slot's wardrobe table (rows in table order), or null. */
	const FShinobiWardrobeRow* FindPiece(const TCHAR* Slot, uint8 Index)
	{
		const UDataTable* Table = Index != FShinobiAppearance::None
			? LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		const TArray<FName> Rows = Table ? Table->GetRowNames() : TArray<FName>();
		return Rows.IsValidIndex(Index) ? Table->FindRow<FShinobiWardrobeRow>(Rows[Index], TEXT("UShinobiMutableLook")) : nullptr;
	}
}

const FShinobiWardrobeRow* UShinobiMutableLook::FindRow(EShinobiWardrobeSlot Slot, uint8 Index)
{
	return int32(Slot) < ShinobiWardrobeSlotCount ? FindPiece(ShinobiMutable::SlotNames[int32(Slot)], Index) : nullptr;
}

const FShinobiWardrobeRow* UShinobiMutableLook::WornRow(const FShinobiAppearance& Look, EShinobiWardrobeSlot Slot)
{
	for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
	{
		const FShinobiWardrobeRow* Other = Index != int32(Slot) ? FindRow(EShinobiWardrobeSlot(Index), Look.Slot(EShinobiWardrobeSlot(Index))) : nullptr;
		if (Other && Other->Replaces.Contains(Slot))
		{
			return nullptr;
		}
	}
	return FindRow(Slot, Look.Slot(Slot));
}

bool UShinobiMutableLook::Request(const FShinobiAppearance& Look)
{
	const FString Path = ObjectPathOverride.IsEmpty() ? FString(ShinobiMutable::ObjectPath) : ObjectPathOverride;
	UCustomizableObject* Object = LoadObject<UCustomizableObject>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Object)
	{
		UE_CLOG(!bLoggedMissing, LogShinobiCoop, Error, TEXT("No Mutable object %s: run Scripts/setup_mutable.py"), *Path);
		bLoggedMissing = true;
		Cancel();
		return false;
	}
	if (!Instance || Instance->GetCustomizableObject() != Object)
	{
		Instance = Object->CreateInstance();
	}
	// The parameters are set when the build starts: an object still compiling (editor only) rejects them.
	Wanted = Look;
	bPending = true;
	const int32 ForSequence = ++Sequence;
	if (Object->IsCompiled())
	{
		StartUpdate(ForSequence);
		return true;
	}
	// Editor only (a packaged game always has it compiled): compile, then build.
	FCompileParams Params;
	Params.bSkipIfCompiled = true;
	Params.CallbackNative.BindWeakLambda(this, [this, ForSequence](const FCompileCallbackParams&) { StartUpdate(ForSequence); });
	Object->Compile(Params);
	return true;
}

void UShinobiMutableLook::SetParameters(const FShinobiAppearance& Look)
{
	const bool bFemale = Look.Sex == EShinobiSex::Female;
	const TCHAR* Sex = bFemale ? TEXT("Female") : TEXT("Male");
	USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(Sex));
	// The Own build wears the roster character's own body (its joints), the pieces reshaped onto it from the base.
	USkeletalMesh* Own = Look.Build == EShinobiBuild::Own ? LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::OwnPath(Sex), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
	Instance->SetSkeletalMeshParameterSelectedOption(ShinobiMutable::Body, Own ? Own : Base);
	Instance->SetSkeletalMeshParameterSelectedOption(ShinobiMutable::FitBase, Base);
	Instance->SetEnumParameterSelectedOption(ShinobiMutable::Build, StaticEnum<EShinobiBuild>()->GetNameStringByValue(int64(Look.Build)));
	Instance->SetFloatParameterSelectedOption(ShinobiMutable::BuildBlend, Look.BuildBlend / 255.f);
	// Slots a worn piece takes off (a whole outfit's Top, Bottom and Shoes).
	TSet<EShinobiWardrobeSlot> Replaced;
	for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
	{
		if (const FShinobiWardrobeRow* Piece = FindPiece(ShinobiMutable::SlotNames[Index], Look.Slot(EShinobiWardrobeSlot(Index))))
		{
			Replaced.Append(Piece->Replaces);
		}
	}
	for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
	{
		const TCHAR* Slot = ShinobiMutable::SlotNames[Index];
		const FShinobiWardrobeRow* Piece = Replaced.Contains(EShinobiWardrobeSlot(Index)) ? nullptr : FindPiece(Slot, Look.Slot(EShinobiWardrobeSlot(Index)));
		USkeletalMesh* Mesh = Piece ? (bFemale ? Piece->FemaleMesh : Piece->MaleMesh).LoadSynchronous() : nullptr;
		USkeletalMesh* Clip = Piece ? (bFemale ? Piece->FemaleClip : Piece->MaleClip).LoadSynchronous() : nullptr;
		const FName Fit = Mesh ? (bFemale ? Piece->FemaleFit : Piece->MaleFit) : NAME_None;
		Instance->SetEnumParameterSelectedOption(FString(Slot) + ShinobiMutable::FitSuffix, Fit.IsNone() ? FString(ShinobiMutable::NoFit) : Fit.ToString());
		Instance->SetEnumParameterSelectedOption(Slot, Mesh ? ShinobiMutable::On : ShinobiMutable::Off);
		Instance->SetEnumParameterSelectedOption(FString(Slot) + ShinobiMutable::HidesSuffix, Clip ? ShinobiMutable::On : ShinobiMutable::Off);
		if (Mesh)
		{
			Instance->SetSkeletalMeshParameterSelectedOption(FString(Slot) + ShinobiMutable::MeshSuffix, Mesh);
		}
		if (Clip)
		{
			Instance->SetSkeletalMeshParameterSelectedOption(FString(Slot) + ShinobiMutable::ClipSuffix, Clip);
		}
	}
}

void UShinobiMutableLook::Cancel()
{
	++Sequence; // any build in flight is now stale
	bPending = false;
}

void UShinobiMutableLook::StartUpdate(int32 ForSequence)
{
	if (ForSequence != Sequence || !Instance)
	{
		return;
	}
	SetParameters(Wanted);
	FInstanceUpdateNativeDelegate Done;
	Done.AddWeakLambda(this, [this, ForSequence](const FUpdateContext& Result) { Built(Result, ForSequence); });
	Instance->UpdateSkeletalMeshAsyncResult(Done, true, true);
}

void UShinobiMutableLook::Built(const FUpdateContext& Result, int32 ForSequence)
{
	if (ForSequence != Sequence)
	{
		return; // a newer request (or a roster body) replaced this one
	}
	bPending = false;
	USkeletalMesh* Mesh = nullptr;
	// ErrorOptimized: the parameters are the last build's (two roster heads on one body), so that mesh stands.
	if ((Result.UpdateResult == EUpdateResult::Success || Result.UpdateResult == EUpdateResult::ErrorOptimized) && Instance)
	{
		const TArray<FName> Components = Instance->GetComponentNames();
		Mesh = Components.IsEmpty() ? nullptr : Instance->GetComponentMeshSkeletalMesh(Components[0]);
	}
	UE_CLOG(!Mesh, LogShinobiCoop, Error, TEXT("Mutable build of %s failed (result %d)"), *GetNameSafe(Instance ? Instance->GetCustomizableObject() : nullptr),
		int32(Result.UpdateResult));
	OnBuilt.ExecuteIfBound(Mesh);
}
