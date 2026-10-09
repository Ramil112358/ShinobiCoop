#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/UI/ShinobiMiniMapData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMiniMapWorldToUVTest, "ShinobiCoop.UI.MiniMap.WorldToUV", ShinobiTestFlags)
bool FShinobiMiniMapWorldToUVTest::RunTest(const FString& Parameters)
{
	UShinobiMiniMapData* Map = NewObject<UShinobiMiniMapData>();
	Map->WorldMin = FVector2D(-100.f, -50.f);
	Map->WorldMax = FVector2D(300.f, 150.f);
	TestTrue(TEXT("bounds are valid"), Map->HasValidBounds());
	// Unmirrored top-down view of UE's left-handed axes: +X right (east), +Y down (south).
	TestTrue(TEXT("min X, min Y is the top-left (north-west)"), Map->WorldToUV(FVector(-100.f, -50.f, 0.f)).Equals(FVector2D(0.f, 0.f)));
	TestTrue(TEXT("max X, min Y is the top-right (north-east)"), Map->WorldToUV(FVector(300.f, -50.f, 0.f)).Equals(FVector2D(1.f, 0.f)));
	TestTrue(TEXT("min X, max Y is the bottom-left (south-west)"), Map->WorldToUV(FVector(-100.f, 150.f, 0.f)).Equals(FVector2D(0.f, 1.f)));
	TestTrue(TEXT("midpoint maps to the centre"), Map->WorldToUV(FVector(100.f, 50.f, 0.f)).Equals(FVector2D(0.5f, 0.5f)));
	TestTrue(TEXT("positions outside the bounds are clamped"), Map->WorldToUV(FVector(-200.f, 250.f, 0.f)).Equals(FVector2D(0.f, 1.f)));

	Map->WorldMax.X = Map->WorldMin.X;
	TestFalse(TEXT("zero-width bounds are invalid"), Map->HasValidBounds());

	TestNull(TEXT("a level without the Lvl_ prefix has no map"), UShinobiMiniMapData::ForLevel(TEXT("MainMenu")));
	return true;
}

#endif
