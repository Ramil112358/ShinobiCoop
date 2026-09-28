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

	namespace
	{
		UMaterialInstanceDynamic* MakeInstance(UObject* Outer, const TCHAR* Path)
		{
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, Path);
			if (!ensureMsgf(Material, TEXT("ShinobiVfx: missing material %s"), Path))
			{
				return nullptr;
			}
			return UMaterialInstanceDynamic::Create(Material, Outer);
		}
	}

	UMaterialInstanceDynamic* MakeFlame(UObject* Outer, const FLinearColor& Color, float Intensity, bool bDark)
	{
		UMaterialInstanceDynamic* Flame = MakeInstance(Outer, bDark
			? TEXT("/Game/Shinobi/VFX/M_FlameDark.M_FlameDark")
			: TEXT("/Game/Shinobi/VFX/M_Flame.M_Flame"));
		if (Flame)
		{
			Flame->SetVectorParameterValue(TEXT("Color"), Color);
			Flame->SetScalarParameterValue(TEXT("Intensity"), Intensity);
			Flame->SetScalarParameterValue(TEXT("Fade"), 1.f);
		}
		return Flame;
	}

	UMaterialInstanceDynamic* MakeStreak(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInstanceDynamic* Streak = MakeInstance(Outer, TEXT("/Game/Shinobi/VFX/M_Streak.M_Streak"));
		if (Streak)
		{
			Streak->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return Streak;
	}
}
