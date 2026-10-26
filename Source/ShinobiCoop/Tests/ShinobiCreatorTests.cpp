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
	TestEqual(TEXT("build"), Wild.Build, EShinobiBuild::Over);
	TestEqual(TEXT("face sliders"), Wild.FaceShape.Num(), UShinobiFaceShapeAnimInstance::GetSliderNames().Num());
	TestEqual(TEXT("body sliders"), Wild.BodyShape.Num(), UShinobiBodyComponent::GetBodySliderNames().Num());
	return true;
}

#endif
