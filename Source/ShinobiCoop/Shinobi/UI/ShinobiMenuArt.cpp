#include "Shinobi/UI/ShinobiMenuArt.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/PackageName.h"
#include "Shinobi/Hero/ShinobiCharacterSave.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiMenuStyle.h"
#include "TextureResource.h"

namespace
{
	FString MenuArtPath(const FString& Name)
	{
		return FString::Printf(TEXT("/Game/Shinobi/UI/Menu/%s.%s"), *Name, *Name);
	}

	/** Signed distance from P to a box of half extents Half with corners rounded by Radius (negative inside). */
	float MouseIconRoundBox(const FVector2f& P, const FVector2f& Half, float Radius)
	{
		const FVector2f Q(FMath::Abs(P.X) - Half.X + Radius, FMath::Abs(P.Y) - Half.Y + Radius);
		return FVector2f(FMath::Max(Q.X, 0.f), FMath::Max(Q.Y, 0.f)).Length() + FMath::Min(FMath::Max(Q.X, Q.Y), 0.f) - Radius;
	}

	/** How much of a pixel a shape covers, from its signed distance at the pixel's centre (a one-pixel antialiased edge). */
	float MouseIconCover(float Distance)
	{
		return FMath::Clamp(0.5f - Distance, 0.f, 1.f);
	}
}

FString ShinobiMenuArt::MapId(const FShinobiMap& Map)
{
	FString Id = FPackageName::GetShortName(Map.Path);
	Id.RemoveFromStart(TEXT("Lvl_"));
	return Id;
}

FString ShinobiMenuArt::MapImagePath(const FShinobiMap& Map)
{
	return MenuArtPath(TEXT("T_Map_") + MapId(Map));
}

FString ShinobiMenuArt::PortraitPath(int32 RosterIndex)
{
	return MenuArtPath(FString(TEXT("T_Portrait_")) + AShinobiPlayerCharacter::Roster[RosterIndex].Name);
}

UTexture2D* ShinobiMenuArt::LoadArt(const FString& Path)
{
	// Check first: a missing package logs even with LOAD_Quiet on some paths.
	if (!FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
	{
		return nullptr;
	}
	return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

UTexture2D* ShinobiMenuArt::MadePortrait(const FShinobiCharacterSlot& Slot)
{
	if (Slot.Portrait.IsEmpty())
	{
		return nullptr;
	}
	return FImageUtils::ImportBufferAsTexture2D(TArrayView64<const uint8>(Slot.Portrait.GetData(), Slot.Portrait.Num()));
}

UTexture2D* ShinobiMenuArt::MouseIcon(EShinobiMouseIcon Lit)
{
	// In pixels: a 30 x 52 body, round at both ends, the buttons above a line 5 px over its middle, split down the centre.
	constexpr int32 Size = 64;
	constexpr float Stroke = 3.f;
	constexpr float Gap = 1.5f; // between a lit part and the white lines around it
	constexpr float Middle = Size * 0.5f;
	const FVector2f Body(15.f, 26.f);
	constexpr float BodyRadius = 14.f;
	constexpr float ButtonLine = Middle - 5.f;
	const FVector2f Wheel(Middle, Middle - 15.f);
	const FVector2f WheelHalf(3.5f, 7.f);
	const FLinearColor Accent = ShinobiMenuStyle::Accent;

	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Size * Size);
	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const FVector2f P(X + 0.5f, Y + 0.5f);
			const float Inside = MouseIconRoundBox(P - FVector2f(Middle, Middle), Body, BodyRadius);
			const float ToWheel = MouseIconRoundBox(P - Wheel, WheelHalf, WheelHalf.X);
			const float Outline = FMath::Abs(Inside) - Stroke * 0.5f;
			const float Line = FMath::Max(FMath::Abs(P.Y - ButtonLine) - Stroke * 0.5f, Inside);
			float Divider = FMath::Max3(FMath::Abs(P.X - Middle) - Stroke * 0.5f, P.Y - ButtonLine, Inside);
			float LitPart;
			if (Lit == EShinobiMouseIcon::Wheel)
			{
				Divider = FMath::Max(Divider, Gap - ToWheel); // the divider stops short of the wheel
				LitPart = ToWheel;
			}
			else
			{
				LitPart = FMath::Max3(Inside + Stroke * 0.5f + Gap, P.Y - (ButtonLine - Stroke * 0.5f - Gap), Middle + Stroke * 0.5f + Gap - P.X);
			}
			const float White = MouseIconCover(FMath::Min3(Outline, Line, Divider));
			const float Shadow = 0.45f * MouseIconCover(FMath::Min3(Outline, Line, Divider) - 1.5f); // a dark rim, for bright backdrops
			const float Fill = MouseIconCover(LitPart);

			// Premultiplied: shadow, then the lit part, then the white lines, each over the last.
			FLinearColor Colour = FLinearColor(0.f, 0.f, 0.f, Shadow);
			Colour = FLinearColor(Accent.R * Fill, Accent.G * Fill, Accent.B * Fill, Fill) + Colour * (1.f - Fill);
			Colour = FLinearColor(White, White, White, White) + Colour * (1.f - White);
			if (Colour.A > 0.f)
			{
				Colour = FLinearColor(Colour.R / Colour.A, Colour.G / Colour.A, Colour.B / Colour.A, Colour.A);
			}
			Pixels[Y * Size + X] = Colour.ToFColor(true);
		}
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}
	Texture->SRGB = true;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->Filter = TF_Bilinear;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	FMemory::Memcpy(Mip.BulkData.Lock(LOCK_READ_WRITE), Pixels.GetData(), Pixels.Num() * sizeof(FColor)); // (FColor is BGRA)
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}
