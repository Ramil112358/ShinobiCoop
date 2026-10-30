#include "Shinobi/Hero/ShinobiSkinLibrary.h"

#include "Materials/Material.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#endif

bool UShinobiSkinLibrary::AddSkinLayers(UMaterial* Material)
{
#if WITH_EDITOR
	UMaterialExpression* Source = Material ? UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_MaterialAttributes) : nullptr;
	if (!Source)
	{
		return false;
	}
	UTexture* Black = LoadObject<UTexture>(nullptr, TEXT("/Engine/EngineResources/Black.Black"));
	int32 X = -2400, Y = -1600;
	auto Make = [&]<typename T>(T*) { T* Node = Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y)); X += 160; return Node; };
	auto Scalar = [&](const FString& Name, float Default)
	{
		UMaterialExpressionScalarParameter* Node = Make((UMaterialExpressionScalarParameter*)nullptr);
		Node->ParameterName = *Name;
		Node->DefaultValue = Default;
		return Node;
	};
	auto Vector = [&](const FString& Name, const FLinearColor& Default)
	{
		UMaterialExpressionVectorParameter* Node = Make((UMaterialExpressionVectorParameter*)nullptr);
		Node->ParameterName = *Name;
		Node->DefaultValue = Default;
		return Node;
	};
	auto Texture = [&](const FString& Name, bool bClamp)
	{
		UMaterialExpressionTextureSampleParameter2D* Node = Make((UMaterialExpressionTextureSampleParameter2D*)nullptr);
		Node->ParameterName = *Name;
		Node->Texture = Black;
		Node->SamplerType = SAMPLERTYPE_Color;
		if (bClamp)
		{
			Node->SamplerSource = SSM_Clamp_WorldGroupSettings; // a design's transparent border, not its next copy, past its square
		}
		return Node;
	};
	// (the pins by hand, as the nodes' own edits do: those need an editor graph node, which a script-built one lacks)
	const FGuid BaseColorId = FMaterialAttributeDefinitionMap::GetID(MP_BaseColor);
	const FString BaseColorName = FMaterialAttributeDefinitionMap::GetDisplayNameForMaterial(BaseColorId, Material).ToString();
	UMaterialExpressionGetMaterialAttributes* Get = Make((UMaterialExpressionGetMaterialAttributes*)nullptr);
	Get->AttributeGetTypes.Add(BaseColorId);
	Get->Outputs.Add(FExpressionOutput(*BaseColorName, 0, 0, 0, 0, 0));
	Get->MaterialAttributes.Connect(0, Source);
	UMaterialExpression* Colour = Get;
	int32 ColourOutput = 1; // (output 0: the attributes; 1: the base colour)
	// Each layer: the colour so far, lerped toward Over by Alpha times its opacity.
	auto Layer = [&](UMaterialExpression* Over, int32 OverOutput, UMaterialExpression* AlphaFrom, int32 AlphaOutput, const FString& OpacityName)
	{
		UMaterialExpressionMultiply* Alpha = Make((UMaterialExpressionMultiply*)nullptr);
		Alpha->A.Connect(AlphaOutput, AlphaFrom);
		Alpha->B.Connect(0, Scalar(OpacityName, 0.f));
		UMaterialExpressionLinearInterpolate* Mixed = Make((UMaterialExpressionLinearInterpolate*)nullptr);
		Mixed->A.Connect(ColourOutput, Colour);
		Mixed->B.Connect(OverOutput, Over);
		Mixed->Alpha.Connect(0, Alpha);
		Colour = Mixed;
		ColourOutput = 0;
		X = -2400;
		Y += 260;
	};
	UMaterialExpressionTextureCoordinate* UV = Make((UMaterialExpressionTextureCoordinate*)nullptr);
	for (int32 Slot = 1; Slot <= 3; ++Slot)
	{
		// UV: (UV - offset) / scale + 0.5, turned about the middle.
		const FString Name = FString::Printf(TEXT("Tattoo %d"), Slot);
		UMaterialExpressionComponentMask* Offset = Make((UMaterialExpressionComponentMask*)nullptr);
		Offset->R = Offset->G = 1;
		Offset->Input.Connect(0, Vector(Name + TEXT(" Offset"), FLinearColor(0.5f, 0.5f, 0.f, 0.f)));
		UMaterialExpressionSubtract* Centred = Make((UMaterialExpressionSubtract*)nullptr);
		Centred->A.Connect(0, UV);
		Centred->B.Connect(0, Offset);
		UMaterialExpressionDivide* Scaled = Make((UMaterialExpressionDivide*)nullptr);
		Scaled->A.Connect(0, Centred);
		Scaled->B.Connect(0, Scalar(Name + TEXT(" Scale"), 0.1f));
		UMaterialExpressionConstant2Vector* Half = Make((UMaterialExpressionConstant2Vector*)nullptr);
		Half->R = Half->G = 0.5f;
		UMaterialExpressionAdd* Placed = Make((UMaterialExpressionAdd*)nullptr);
		Placed->A.Connect(0, Scaled);
		Placed->B.Connect(0, Half);
		UMaterialExpressionMultiply* Radians = Make((UMaterialExpressionMultiply*)nullptr);
		Radians->A.Connect(0, Scalar(Name + TEXT(" Rotation"), 0.f));
		Radians->ConstB = 2.f * UE_PI;
		UMaterialExpressionRotator* Turned = Make((UMaterialExpressionRotator*)nullptr); // (angle = Time x Speed, about 0.5, 0.5)
		Turned->Speed = 1.f;
		Turned->Coordinate.Connect(0, Placed);
		Turned->Time.Connect(0, Radians);
		UMaterialExpressionTextureSampleParameter2D* Design = Texture(Name, true);
		Design->Coordinates.Connect(0, Turned);
		Layer(Design, 0, Design, 4, Name + TEXT(" Opacity")); // (RGB over, by A)
	}
	// MetaHuman face masks (the plugin's ArtistDelights textures, face UVs): a colour where the mask is.
	for (const TCHAR* Mask : {TEXT("Eye Makeup"), TEXT("Lips Makeup"), TEXT("Blush Makeup"), TEXT("Freckles")})
	{
		UMaterialExpressionTextureSampleParameter2D* Where = Texture(Mask, false);
		Layer(Vector(FString(Mask) + TEXT(" Colour"), FLinearColor::Black), 0, Where, 1, FString(Mask) + TEXT(" Opacity")); // (by R)
	}
	UMaterialExpressionSetMaterialAttributes* Set = Make((UMaterialExpressionSetMaterialAttributes*)nullptr);
	Set->AttributeSetTypes.Add(BaseColorId);
	Set->Inputs.SetNum(2); // the attributes in, then the base colour
	Set->Inputs[1].InputName = *BaseColorName;
	Set->Inputs[0].Connect(0, Source);
	Set->Inputs[1].Connect(ColourOutput, Colour);
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
