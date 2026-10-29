#include "Shinobi/Hero/ShinobiCreatorCatalog.h"

#include "GroomBindingAsset.h"
#include "Misc/Base64.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"

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

const FShinobiCreatorHead* UShinobiCreatorCatalog::FindHead(uint8 Index) const
{
	return Heads.IsValidIndex(Index) ? &Heads[Index] : nullptr; // (None = 255 is past the end)
}

const FShinobiAppearance* UShinobiCreatorCatalog::FindPreset(FName Roster) const
{
	const FShinobiRosterPreset* Found = Presets.FindByPredicate([&](const FShinobiRosterPreset& P) { return P.Roster == Roster; });
	return Found ? &Found->Look : nullptr;
}

void UShinobiCreatorCatalog::SetPreset(FName Roster, EShinobiSex Sex, uint8 Head, uint8 Face, EShinobiBuild Build, uint8 Pieces)
{
	FShinobiAppearance Look;
	Look.Sex = Sex;
	Look.Head = Head;
	Look.Face = Face;
	Look.Build = Build;
	Look.Top = Look.Bottom = Look.Shoes = Pieces;
	Look.Hair = Look.Beard = Look.Mustache = FShinobiAppearance::None;
	Presets.RemoveAll([&](const FShinobiRosterPreset& P) { return P.Roster == Roster; });
	Presets.Add({Roster, Look});
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

uint8& FShinobiAppearance::Slot(EShinobiWardrobeSlot S)
{
	uint8* Fields[] = {&Top, &Bottom, &Shoes, &Outer, &Accessory, &Cloak};
	static_assert(UE_ARRAY_COUNT(Fields) == ShinobiWardrobeSlotCount);
	return *Fields[FMath::Min(int32(S), ShinobiWardrobeSlotCount - 1)];
}

void FShinobiAppearance::Sanitize()
{
	Sex = Sex == EShinobiSex::Female ? EShinobiSex::Female : EShinobiSex::Male;
	Build = uint8(Build) > uint8(EShinobiBuild::Own) ? EShinobiBuild::Normal : Build;
	FaceShape.SetNum(FMath::Min(FaceShape.Num(), UShinobiFaceShapeAnimInstance::GetSliderNames().Num()));
	BodyShape.SetNum(FMath::Min(BodyShape.Num(), UShinobiBodyComponent::GetBodySliderNames().Num()));
	Tattoos.SetNum(FMath::Min(Tattoos.Num(), ShinobiMutable::MaxTattoos));
}

namespace
{
	/** 1: before Mutable looks. 2: adds the Mutable fields after the version-1 ones (FromCode reads both). */
	constexpr uint8 CodeVersion = 2;
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
	Bytes.Append({Head, BuildBlend, Outer, Accessory, Cloak});
	Bytes.Append(SlotColour, ShinobiWardrobeSlotCount);
	Bytes.Append(SlotWear, ShinobiWardrobeSlotCount);
	Bytes.Append({SkinTone, Aged, MakeUp, Freckles, uint8(Tattoos.Num())});
	for (const FShinobiTattoo& Tattoo : Tattoos)
	{
		Bytes.Append({Tattoo.Design, Tattoo.Region, uint8(Tattoo.OffsetU), uint8(Tattoo.OffsetV), Tattoo.Rotation, Tattoo.Scale});
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
	if (!FBase64::Decode(Padded, Bytes, EBase64Mode::UrlSafe) || Bytes.Num() < Fixed + 2 || Bytes[0] < 1 || Bytes[0] > CodeVersion)
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
	if (Bytes[0] >= 2)
	{
		// Version 2: the Mutable fields, in ToCode's order.
		uint8* Fields[] = {&Look.Head, &Look.BuildBlend, &Look.Outer, &Look.Accessory, &Look.Cloak};
		constexpr int32 Fixed2 = UE_ARRAY_COUNT(Fields) + 2 * ShinobiWardrobeSlotCount + 5;
		if (At + Fixed2 > Bytes.Num())
		{
			return false;
		}
		for (uint8* Field : Fields)
		{
			*Field = Bytes[At++];
		}
		for (uint8* Array : {Look.SlotColour, Look.SlotWear})
		{
			for (int32 Index = 0; Index < ShinobiWardrobeSlotCount; ++Index)
			{
				Array[Index] = Bytes[At++];
			}
		}
		for (uint8* Field : {&Look.SkinTone, &Look.Aged, &Look.MakeUp, &Look.Freckles})
		{
			*Field = Bytes[At++];
		}
		const int32 Count = Bytes[At++];
		if (At + Count * 6 > Bytes.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Count; ++Index, At += 6)
		{
			Look.Tattoos.Add({Bytes[At], Bytes[At + 1], int8(Bytes[At + 2]), int8(Bytes[At + 3]), Bytes[At + 4], Bytes[At + 5]});
		}
	}
	Look.Sanitize();
	Out = MoveTemp(Look);
	return true;
}
