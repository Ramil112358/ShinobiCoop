#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiFaceShapeAnimInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiAppearanceCodeTest, "ShinobiCoop.Creator.AppearanceCode", ShinobiTestFlags)
bool FShinobiAppearanceCodeTest::RunTest(const FString& Parameters)
{
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex::Female;
	Look.Build = EShinobiBuild::Over;
	Look.Face = 3;
	Look.Hair = 7;
	Look.Mustache = 2;
	Look.Shoes = 1;
	Look.FaceShape = {-127, 0, 64, 127};
	Look.BodyShape = {10, -10};
	Look.BlendFace[0] = 5;
	Look.BlendWeight[0] = 128;
	const FString Code = Look.ToCode();
	FShinobiAppearance Back;
	TestTrue(TEXT("a code reads back"), FShinobiAppearance::FromCode(Code, Back));
	TestTrue(TEXT("to the same look"), Back == Look);
	TestTrue(TEXT("and survives spaces round it"), FShinobiAppearance::FromCode(TEXT("  ") + Code + TEXT("\n"), Back) && Back == Look);
	TestFalse(TEXT("an URL-safe code with no padding"), Code.Contains(TEXT("=")) || Code.Contains(TEXT("+")) || Code.Contains(TEXT("/")));

	FShinobiAppearance Untouched;
	const FString Half = Code.Left(Code.Len() / 2);
	for (const TCHAR* Bad : {TEXT(""), TEXT("hello"), TEXT("!!!!"), *Half})
	{
		TestFalse(FString::Printf(TEXT("'%s' is not a code"), Bad), FShinobiAppearance::FromCode(Bad, Untouched));
	}
	TestTrue(TEXT("a failed read leaves the look alone"), Untouched == FShinobiAppearance());

	// A client's look is clamped: unknown sex and build, slider lists longer than the sliders.
	FShinobiAppearance Wild;
	Wild.Sex = EShinobiSex(9);
	Wild.Build = EShinobiBuild(200);
	Wild.FaceShape.Init(1, 500);
	Wild.BodyShape.Init(1, 500);
	Wild.Sanitize();
	TestEqual(TEXT("sex"), Wild.Sex, EShinobiSex::Male);
	TestEqual(TEXT("an unknown build is the normal one"), Wild.Build, EShinobiBuild::Normal);
	TestEqual(TEXT("face sliders"), Wild.FaceShape.Num(), UShinobiFaceShapeAnimInstance::GetSliderNames().Num());
	TestEqual(TEXT("body sliders"), Wild.BodyShape.Num(), UShinobiBodyComponent::GetBodySliderNames().Num());

	// A version-1 code (before the Mutable fields) still reads, and the new fields keep their defaults.
	FShinobiAppearance Old;
	if (TestTrue(TEXT("a version-1 code reads"), FShinobiAppearance::FromCode(TEXT("AQECAwf_AgAAAQX_gAAEgQBAfwIK9g"), Old)))
	{
		TestEqual(TEXT("v1 sex"), Old.Sex, EShinobiSex::Female);
		TestEqual(TEXT("v1 build"), Old.Build, EShinobiBuild::Over);
		TestEqual(TEXT("v1 face"), Old.Face, uint8(3));
		TestEqual(TEXT("v1 hair"), Old.Hair, uint8(7));
		TestEqual(TEXT("v1 beard"), Old.Beard, FShinobiAppearance::None);
		TestEqual(TEXT("v1 mustache"), Old.Mustache, uint8(2));
		TestEqual(TEXT("v1 shoes"), Old.Shoes, uint8(1));
		TestEqual(TEXT("v1 blend face"), Old.BlendFace[0], uint8(5));
		TestEqual(TEXT("v1 blend weight"), Old.BlendWeight[0], uint8(128));
		TestTrue(TEXT("v1 face sliders"), Old.FaceShape == TArray<int8>({-127, 0, 64, 127}));
		TestTrue(TEXT("v1 body sliders"), Old.BodyShape == TArray<int8>({10, -10}));
		TestEqual(TEXT("v1 head defaults to none"), Old.Head, FShinobiAppearance::None);
		TestEqual(TEXT("v1 build blend defaults to full"), Old.BuildBlend, uint8(255));
		TestEqual(TEXT("v1 skin tone defaults to the middle"), Old.SkinTone, uint8(128));
		TestEqual(TEXT("v1 outer defaults to none"), Old.Outer, FShinobiAppearance::None);
	}
	return true;
}

#endif
