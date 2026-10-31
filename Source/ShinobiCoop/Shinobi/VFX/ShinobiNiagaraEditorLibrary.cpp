#include "Shinobi/VFX/ShinobiNiagaraEditorLibrary.h"
#include "NiagaraSystem.h"

bool UShinobiNiagaraEditorLibrary::RecompileAndWait(UNiagaraSystem* System)
{
#if WITH_EDITOR
	if (!System)
	{
		return false;
	}
	System->RequestCompile(true);
	System->WaitForCompilationComplete(false, false);
	return !System->HasOutstandingCompilationRequests();
#else
	return false;
#endif
}
