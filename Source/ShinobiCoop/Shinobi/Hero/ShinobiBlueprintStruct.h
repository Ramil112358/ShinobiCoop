#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

/**
 * Reading and writing Blueprint structs from C++ (the Game Animation Sample's S_* structs, which our code fills and reads
 * instead of reimplementing the sample's assets). Fields are found by their authored name ("Gait", not the stored
 * "Gait_12_F5E6..."), Blueprint enum values by their display name ("Run", not "NewEnumerator1").
 */
namespace ShinobiBlueprintStruct
{
	inline const FProperty* Find(const UStruct* Struct, FName Name)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (FName(*It->GetAuthoredName()) == Name)
			{
				return *It;
			}
		}
		return nullptr;
	}

	inline const UEnum* GetEnum(const FProperty* Property)
	{
		if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			return Byte->Enum;
		}
		const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
		return EnumProperty ? EnumProperty->GetEnum() : nullptr;
	}

	/** The enum value with this display name, or INDEX_NONE. */
	inline int64 EnumValue(const FProperty* Property, const TCHAR* DisplayName)
	{
		const UEnum* Enum = GetEnum(Property);
		for (int32 Index = 0; Enum && Index < Enum->NumEnums() - 1; ++Index) // the last entry is the generated _MAX
		{
			if (Enum->GetDisplayNameTextByIndex(Index).ToString() == DisplayName)
			{
				return Enum->GetValueByIndex(Index);
			}
		}
		return INDEX_NONE;
	}

	inline void SetEnum(const FProperty* Property, void* Container, int64 Value)
	{
		if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			Byte->SetPropertyValue_InContainer(Container, static_cast<uint8>(Value));
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Container), Value);
		}
	}

	inline FString GetEnumDisplayName(const FProperty* Property, const void* Container)
	{
		const UEnum* Enum = GetEnum(Property);
		if (!Enum)
		{
			return FString();
		}
		int64 Value = 0;
		if (const FByteProperty* Byte = CastField<FByteProperty>(Property))
		{
			Value = Byte->GetPropertyValue_InContainer(Container);
		}
		else
		{
			const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property);
			Value = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Container));
		}
		return Enum->GetDisplayNameTextByValue(Value).ToString();
	}

	template <typename T>
	void SetStruct(const FProperty* Property, void* Container, const T& Value)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty && StructProperty->Struct == TBaseStructure<T>::Get())
		{
			*StructProperty->ContainerPtrToValuePtr<T>(Container) = Value;
		}
	}

	inline void SetReal(const FProperty* Property, void* Container, double Value)
	{
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
		if (Numeric && Numeric->IsFloatingPoint())
		{
			Numeric->SetFloatingPointPropertyValue(Numeric->ContainerPtrToValuePtr<void>(Container), Value);
		}
	}

	inline double GetReal(const FProperty* Property, const void* Container)
	{
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Property);
		return Numeric && Numeric->IsFloatingPoint() ? Numeric->GetFloatingPointPropertyValue(Numeric->ContainerPtrToValuePtr<void>(Container)) : 0.0;
	}

	inline void SetBool(const FProperty* Property, void* Container, bool bValue)
	{
		if (const FBoolProperty* Bool = CastField<FBoolProperty>(Property))
		{
			Bool->SetPropertyValue_InContainer(Container, bValue);
		}
	}
}
