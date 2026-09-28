#include "Shinobi/ShinobiLook.h"
#include "Components/PrimitiveComponent.h"

void ShinobiLook::EnableOutline(UPrimitiveComponent* Mesh, uint8 Stencil)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(true);
		Mesh->SetCustomDepthStencilValue(Stencil);
	}
}
