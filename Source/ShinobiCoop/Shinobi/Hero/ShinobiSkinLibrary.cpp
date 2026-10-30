#include "Shinobi/Hero/ShinobiSkinLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#endif

bool UShinobiSkinLibrary::AddTattooLayer(UMaterial* Material)
{
#if WITH_EDITOR
	UMaterialExpression* Source = Material ? UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_MaterialAttributes) : nullptr;
	if (!Source)
	{
		return false;
	}
	int32 X = -1400, Y = -600;
	auto Make = [&]<typename T>(T*) { T* Node = Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y)); X += 160; return Node; };
	auto Scalar = [&](const TCHAR* Name, float Default)
	{
		UMaterialExpressionScalarParameter* Node = Make((UMaterialExpressionScalarParameter*)nullptr);
		Node->ParameterName = Name;
		Node->DefaultValue = Default;
		return Node;
	};
	// The base colour from the attributes.
	UMaterialExpressionGetMaterialAttributes* Get = Make((UMaterialExpressionGetMaterialAttributes*)nullptr);
	// (the pins by hand, as the node's own edit does: that needs an editor graph node, which a script-built one lacks)
	const FGuid BaseColor = FMaterialAttributeDefinitionMap::GetID(MP_BaseColor);
	const FString BaseColorName = FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(BaseColor, Material).ToString();
	Get->AttributeGetTypes.Add(BaseColor);
	Get->Outputs.Add(FExpressionOutput(*BaseColorName, 0, 0, 0, 0, 0));
	Get->MaterialAttributes.Connect(0, Source);
	// UV: (UV - offset) / scale + 0.5, turned about the middle.
	UMaterialExpressionTextureCoordinate* UV = Make((UMaterialExpressionTextureCoordinate*)nullptr);
	UMaterialExpressionVectorParameter* Offset = Make((UMaterialExpressionVectorParameter*)nullptr);
	Offset->ParameterName = TEXT("Tattoo Offset");
	Offset->DefaultValue = FLinearColor(0.5f, 0.5f, 0.f, 0.f);
	UMaterialExpressionComponentMask* OffsetUV = Make((UMaterialExpressionComponentMask*)nullptr);
	OffsetUV->R = OffsetUV->G = 1;
	OffsetUV->Input.Connect(0, Offset);
	UMaterialExpressionSubtract* Centred = Make((UMaterialExpressionSubtract*)nullptr);
	Centred->A.Connect(0, UV);
	Centred->B.Connect(0, OffsetUV);
	UMaterialExpressionDivide* Scaled = Make((UMaterialExpressionDivide*)nullptr);
	Scaled->A.Connect(0, Centred);
	Scaled->B.Connect(0, Scalar(TEXT("Tattoo Scale"), 0.1f));
	UMaterialExpressionConstant2Vector* Half = Make((UMaterialExpressionConstant2Vector*)nullptr);
	Half->R = Half->G = 0.5f;
	UMaterialExpressionAdd* Placed = Make((UMaterialExpressionAdd*)nullptr);
	Placed->A.Connect(0, Scaled);
	Placed->B.Connect(0, Half);
	UMaterialExpressionMultiply* Radians = Make((UMaterialExpressionMultiply*)nullptr);
	Radians->A.Connect(0, Scalar(TEXT("Tattoo Rotation"), 0.f));
	Radians->ConstB = 2.f * UE_PI;
	UMaterialExpressionRotator* Turned = Make((UMaterialExpressionRotator*)nullptr); // (angle = Time x Speed, about 0.5, 0.5)
	Turned->Speed = 1.f;
	Turned->Coordinate.Connect(0, Placed);
	Turned->Time.Connect(0, Radians);
	UMaterialExpressionTextureSampleParameter2D* Tattoo = Make((UMaterialExpressionTextureSampleParameter2D*)nullptr);
	Tattoo->ParameterName = TEXT("Tattoo");
	Tattoo->Coordinates.Connect(0, Turned);
	// Over the base colour by its alpha times the opacity.
	UMaterialExpressionMultiply* Alpha = Make((UMaterialExpressionMultiply*)nullptr);
	Alpha->A.Connect(4, Tattoo); // A
	Alpha->B.Connect(0, Scalar(TEXT("Tattoo Opacity"), 0.f));
	UMaterialExpressionLinearInterpolate* Inked = Make((UMaterialExpressionLinearInterpolate*)nullptr);
	Inked->A.Connect(1, Get); // (output 0: the attributes; 1: the base colour)
	Inked->B.Connect(0, Tattoo); // RGB
	Inked->Alpha.Connect(0, Alpha);
	UMaterialExpressionSetMaterialAttributes* Set = Make((UMaterialExpressionSetMaterialAttributes*)nullptr);
	Set->AttributeSetTypes.Add(BaseColor);
	Set->Inputs.SetNum(2); // the attributes in, then the base colour
	Set->Inputs[1].InputName = *BaseColorName;
	Set->Inputs[0].Connect(0, Source);
	Set->Inputs[1].Connect(0, Inked);
	if (!UMaterialEditingLibrary::ConnectMaterialProperty(Set, TEXT(""), MP_MaterialAttributes))
	{
		return false;
	}
	UMaterialEditingLibrary::RecompileMaterial(Material);
	return true;
#else
	return false;
#endif
}
