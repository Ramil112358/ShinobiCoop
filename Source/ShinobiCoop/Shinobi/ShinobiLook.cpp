#include "Shinobi/ShinobiLook.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	const TCHAR* Key = TEXT("bOutline");
}

void ShinobiLook::EnableOutline(UPrimitiveComponent* Mesh, uint8 Stencil)
{
	if (Mesh)
	{
		Mesh->SetCustomDepthStencilValue(Stencil);
		Mesh->SetRenderCustomDepth(IsOutlineEnabled());
	}
}

bool ShinobiLook::IsOutlineEnabled()
{
	bool bEnabled = true;
	GConfig->GetBool(Section, Key, bEnabled, GGameUserSettingsIni);
	return bEnabled;
}

void ShinobiLook::SetOutlineEnabled(UWorld* World, bool bEnabled)
{
	GConfig->SetBool(Section, Key, bEnabled, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		const int32 Stencil = It->CustomDepthStencilValue;
		if (It->GetWorld() == World && (Stencil == StencilFriendly || Stencil == StencilEnemy))
		{
			It->SetRenderCustomDepth(bEnabled);
		}
	}
}
