#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiNiagaraEditorLibrary.generated.h"

class UNiagaraSystem;

/** Editor-time helper for Python (Scripts/recompile_fab_fire.py): Niagara systems edited through the toolsets are saved before their compile finishes. */
UCLASS()
class SHINOBICOOP_API UShinobiNiagaraEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Recompiles every script of the system and waits for it (editor only); false if it could not. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Editor")
	static bool RecompileAndWait(UNiagaraSystem* System);
};
