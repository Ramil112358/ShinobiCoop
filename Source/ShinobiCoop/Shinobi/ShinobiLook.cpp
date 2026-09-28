#include "Shinobi/ShinobiLook.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const TCHAR* OutlineConfigSection = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	const TCHAR* OutlineConfigKey = TEXT("bOutline");
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
	GConfig->GetBool(OutlineConfigSection, OutlineConfigKey, bEnabled, GGameUserSettingsIni);
	return bEnabled;
}

void ShinobiLook::SetOutlineEnabled(UWorld* World, bool bEnabled)
{
	GConfig->SetBool(OutlineConfigSection, OutlineConfigKey, bEnabled, GGameUserSettingsIni);
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
