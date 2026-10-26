#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "GroomBindingAsset.h"
#include "Misc/Base64.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"

namespace
{
	/** The Index-th element of Items passing Keep, or null. */
	template <typename T, typename F>
	const T* NthWhere(const TArray<T>& Items, int32 Index, F Keep)
	{
		for (const T& Item : Items)
		{
			if (Keep(Item) && Index-- == 0)
			{
				return &Item;
			}
		}
		return nullptr;
	}

	template <typename T, typename F>
	int32 CountWhere(const TArray<T>& Items, F Keep)
	{
		int32 Count = 0;
		for (const T& Item : Items)
		{
			Count += Keep(Item) ? 1 : 0;
		}
		return Count;
	}
}

const FShinobiCreatorFrame* UShinobiCreatorCatalog::FindFrame(EShinobiSex Sex, EShinobiBuild Build) const
{
	return NthWhere(Frames, 0, [&](const FShinobiCreatorFrame& F) { return F.Sex == Sex && F.Build == Build; });
}

FString UShinobiCreatorCatalog::FittingName(EShinobiSex Sex, EShinobiBuild Build)
{
	static const TCHAR* Builds[] = {TEXT("Under"), TEXT("Normal"), TEXT("Over")};
	return FString::Printf(TEXT("Crowd%s%s"), Sex == EShinobiSex::Male ? TEXT("M") : TEXT("F"), Builds[FMath::Min(int32(Build), 2)]);
}

const FShinobiCreatorFace* UShinobiCreatorCatalog::FindFace(EShinobiSex Sex, int32 Index) const
{
	return NthWhere(Faces, Index, [&](const FShinobiCreatorFace& F) { return F.Sex == Sex; });
}

const FShinobiCreatorGroom* UShinobiCreatorCatalog::FindGroom(EShinobiSex Sex, EShinobiGroomSlot Slot, int32 Index) const
{
	return NthWhere(Grooms, Index, [&](const FShinobiCreatorGroom& G) { return G.Sex == Sex && G.Slot == Slot; });
}

const FShinobiCreatorStyle* UShinobiCreatorCatalog::FindStyle(EShinobiSex Sex, EShinobiWardrobeSlot Slot, int32 Index) const
{
	return NthWhere(Styles, Index, [&](const FShinobiCreatorStyle& S) { return S.Sex == Sex && S.Slot == Slot; });
}

UGroomBindingAsset* UShinobiCreatorCatalog::LoadBinding(FName Face, FName Groom) const
{
	const FShinobiCreatorBinding* Found = NthWhere(Bindings, 0, [&](const FShinobiCreatorBinding& B) { return B.Face == Face && B.Groom == Groom; });
	return Found ? Found->Binding.LoadSynchronous() : nullptr;
}

int32 UShinobiCreatorCatalog::CountFaces(EShinobiSex Sex) const
{
	return CountWhere(Faces, [&](const FShinobiCreatorFace& F) { return F.Sex == Sex; });
}

int32 UShinobiCreatorCatalog::CountGrooms(EShinobiSex Sex, EShinobiGroomSlot Slot) const
{
	return CountWhere(Grooms, [&](const FShinobiCreatorGroom& G) { return G.Sex == Sex && G.Slot == Slot; });
}

int32 UShinobiCreatorCatalog::CountStyles(EShinobiSex Sex, EShinobiWardrobeSlot Slot) const
{
	return CountWhere(Styles, [&](const FShinobiCreatorStyle& S) { return S.Sex == Sex && S.Slot == Slot; });
}

void FShinobiAppearance::Sanitize()
{
	Sex = Sex == EShinobiSex::Female ? EShinobiSex::Female : EShinobiSex::Male;
	Build = EShinobiBuild(FMath::Min(uint8(Build), uint8(EShinobiBuild::Over)));
	FaceShape.SetNum(FMath::Min(FaceShape.Num(), UShinobiFaceShapeAnimInstance::GetSliderNames().Num()));
	BodyShape.SetNum(FMath::Min(BodyShape.Num(), UShinobiBodyComponent::GetBodySliderNames().Num()));
}

namespace
{
	constexpr uint8 CodeVersion = 1;
}

FString FShinobiAppearance::ToCode() const
{
	TArray<uint8> Bytes = {CodeVersion, uint8(Sex), uint8(Build), Face, Hair, Beard, Mustache, Top, Bottom, Shoes,
		BlendFace[0], BlendFace[1], BlendWeight[0], BlendWeight[1]};
	for (const TArray<int8>* Shape : {&FaceShape, &BodyShape})
	{
		Bytes.Add(uint8(Shape->Num()));
		for (const int8 Value : *Shape)
		{
			Bytes.Add(uint8(Value));
		}
	}
	return FBase64::Encode(Bytes, EBase64Mode::UrlSafe).Replace(TEXT("="), TEXT(""));
}

bool FShinobiAppearance::FromCode(const FString& Code, FShinobiAppearance& Out)
{
	FString Padded = Code.TrimStartAndEnd();
	while (Padded.Len() % 4)
	{
		Padded += TEXT("=");
	}
	TArray<uint8> Bytes;
	constexpr int32 Fixed = 14;
	if (!FBase64::Decode(Padded, Bytes, EBase64Mode::UrlSafe) || Bytes.Num() < Fixed + 2 || Bytes[0] != CodeVersion)
	{
		return false;
	}
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex(Bytes[1]);
	Look.Build = EShinobiBuild(Bytes[2]);
	uint8* Slots[] = {&Look.Face, &Look.Hair, &Look.Beard, &Look.Mustache, &Look.Top, &Look.Bottom, &Look.Shoes,
		&Look.BlendFace[0], &Look.BlendFace[1], &Look.BlendWeight[0], &Look.BlendWeight[1]};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
	{
		*Slots[Index] = Bytes[3 + Index];
	}
	int32 At = Fixed;
	for (TArray<int8>* Shape : {&Look.FaceShape, &Look.BodyShape})
	{
		const int32 Count = At < Bytes.Num() ? Bytes[At++] : -1;
		if (Count < 0 || At + Count > Bytes.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Shape->Add(int8(Bytes[At++]));
		}
	}
	Look.Sanitize();
	Out = MoveTemp(Look);
	return true;
}
