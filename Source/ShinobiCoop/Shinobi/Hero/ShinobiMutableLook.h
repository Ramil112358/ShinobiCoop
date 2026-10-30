#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "ShinobiMutableLook.generated.h"

class UCustomizableObjectInstance;
class USkeletalMesh;
struct FUpdateContext;

DECLARE_DELEGATE_OneParam(FOnShinobiMutableBuilt, USkeletalMesh*);

/**
 * Builds a made character's body with Mutable (CO_Shinobi, Scripts/setup_mutable.py): an appearance becomes the object's
 * parameters (the sex's base as Body, the build and its blend, each wardrobe slot's piece and clip mesh from its
 * DT_Wardrobe_<Slot> row, passed at runtime), then one async build. Owned by UShinobiBodyComponent, which wears OnBuilt's mesh.
 */
UCLASS()
class SHINOBICOOP_API UShinobiMutableLook : public UObject
{
	GENERATED_BODY()

public:
	/** Starts building Look. The newest request wins: a build finishing after a newer request is dropped. False (nothing
	 * pending, logged once) when CO_Shinobi is missing. */
	bool Request(const FShinobiAppearance& Look);

	/** Drops the pending build, if any (a roster body is worn instead). */
	void Cancel();

	bool IsPending() const { return bPending; }

	/** The Mutable instance builds go through (null before the first request). */
	UCustomizableObjectInstance* GetInstance() const { return Instance; }

	/** The built mesh of the newest request (null if Mutable failed: the caller keeps what it wears). */
	FOnShinobiMutableBuilt OnBuilt;

	/** Row Index of the slot's wardrobe table (DT_Wardrobe_<Slot>), or null (None, or past the rows). */
	static const FShinobiWardrobeRow* FindRow(EShinobiWardrobeSlot Slot, uint8 Index);

	/** Tests only: the object to build instead of ShinobiMutable::ObjectPath (empty = that). */
	static FString ObjectPathOverride;

private:
	void SetParameters(const FShinobiAppearance& Look);
	void StartUpdate(int32 ForSequence);
	void Built(const FUpdateContext& Result, int32 ForSequence);

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstance> Instance;

	/** The newest request's look (its parameters are set when its build starts). */
	FShinobiAppearance Wanted;

	int32 Sequence = 0;
	bool bPending = false;
	bool bLoggedMissing = false;
};
