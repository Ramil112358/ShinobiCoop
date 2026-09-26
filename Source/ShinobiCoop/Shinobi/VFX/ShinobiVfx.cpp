#include "Shinobi/VFX/ShinobiVfx.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace ShinobiVfx
{
	UMaterialInstanceDynamic* MakeGlow(UObject* Outer, const FLinearColor& Color, float Intensity, float Core, float Inflate)
	{
		static const TCHAR* Path = TEXT("/Game/Shinobi/VFX/M_ChakraBodyAura.M_ChakraBodyAura");
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
		if (!ensureMsgf(Material, TEXT("ShinobiVfx: missing material %s"), Path))
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(Material, Outer);
		Glow->SetVectorParameterValue(TEXT("Color"), Color);
		Glow->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		Glow->SetScalarParameterValue(TEXT("Core"), Core);
		Glow->SetScalarParameterValue(TEXT("Inflate"), Inflate);
		Glow->SetScalarParameterValue(TEXT("Fade"), 1.f);
		return Glow;
	}
}
