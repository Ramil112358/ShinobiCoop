#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Misc/PackageName.h"
#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Shinobi/UI/ShinobiLoadingScreen.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadingStepsTest, "ShinobiCoop.UI.LoadingScreen.Steps", ShinobiTestFlags)
bool FShinobiLoadingStepsTest::RunTest(const FString& Parameters)
{
	using namespace ShinobiLoadingScreen;

	// Everything still to do, then each wait cleared in the order the screen waits for them.
	FChecks Checks;
	Checks.bConnecting = true;
	Checks.bTravelling = true;
	Checks.bWaitingForPawn = true;
	Checks.LevelsStreaming = 3;
	Checks.ShadersCompiling = 40;
	Checks.ShadersPeak = 40;
	Checks.CharactersBuilding = 1;
	Checks.TexturesStreaming = 12;

	TArray<FStatus> Seen;
	Seen.Add(Evaluate(Checks));
	Checks.bConnecting = false;
	Seen.Add(Evaluate(Checks));
	Checks.bTravelling = false;
	Seen.Add(Evaluate(Checks)); // world not started
	Checks.bWorldStarted = true;
	Seen.Add(Evaluate(Checks)); // no pawn yet
	Checks.bWaitingForPawn = false;
	Seen.Add(Evaluate(Checks));
	Checks.LevelsStreaming = 0;
	Seen.Add(Evaluate(Checks));
	Checks.ShadersCompiling = 10;
	Seen.Add(Evaluate(Checks)); // fewer shaders: further along
	Checks.ShadersCompiling = 0;
	Seen.Add(Evaluate(Checks));
	Checks.CharactersBuilding = 0;
	Seen.Add(Evaluate(Checks));
	Checks.bTexturesTimedOut = true;
	Seen.Add(Evaluate(Checks));

	const EStep Expected[] = { EStep::Connecting, EStep::LoadingMap, EStep::StartingGame, EStep::StartingGame, EStep::StreamingWorld,
		EStep::PreparingShaders, EStep::PreparingShaders, EStep::PreparingCharacters, EStep::StreamingTextures, EStep::Ready };
	TestEqual(TEXT("one status per stage"), Seen.Num(), int32(UE_ARRAY_COUNT(Expected)));
	for (int32 Index = 0; Index < Seen.Num() && Index < int32(UE_ARRAY_COUNT(Expected)); ++Index)
	{
		TestTrue(FString::Printf(TEXT("stage %d is step %d"), Index, int32(Expected[Index])), Seen[Index].Step == Expected[Index]);
		TestFalse(FString::Printf(TEXT("stage %d has a label"), Index), Seen[Index].Label.IsEmpty());
		if (Index > 0)
		{
			TestTrue(FString::Printf(TEXT("progress never goes back (stage %d)"), Index), Seen[Index].Progress > Seen[Index - 1].Progress);
		}
	}
	TestEqual(TEXT("ready is full"), Seen.Last().Progress, 1.f);
	TestTrue(TEXT("shaders say how many are left"), Seen[5].Detail.ToString().Contains(TEXT("40")));

	// Textures only hold the screen until they've had their time, and nothing else is pending.
	FChecks Textures;
	Textures.bWorldStarted = true;
	Textures.TexturesStreaming = 5;
	TestTrue(TEXT("textures hold the screen"), Evaluate(Textures).Step == EStep::StreamingTextures);
	Textures.bTexturesTimedOut = true;
	TestTrue(TEXT("...until their time is up"), Evaluate(Textures).Step == EStep::Ready);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadingMapPathTest, "ShinobiCoop.UI.LoadingScreen.MapPath", ShinobiTestFlags)
bool FShinobiLoadingMapPathTest::RunTest(const FString& Parameters)
{
	using namespace ShinobiLoadingScreen;
	const TArray<FShinobiMap>& Maps = UShinobiSessionSubsystem::GetMaps();
	for (int32 Index = 0; Index < Maps.Num(); ++Index)
	{
		const FString Path = Maps[Index].Path;
		const FString Short = FPackageName::GetShortName(Path);
		TestEqual(FString::Printf(TEXT("%s by package path"), *Short), FindMapByPath(Path), Index);
		TestEqual(FString::Printf(TEXT("%s by object path"), *Short), FindMapByPath(Path + TEXT(".") + Short), Index);
		TestEqual(FString::Printf(TEXT("%s in PIE"), *Short), FindMapByPath(FPackageName::GetLongPackagePath(Path) + TEXT("/UEDPIE_0_") + Short), Index);
	}
	TestEqual(TEXT("the menu is no map"), FindMapByPath(UShinobiSessionSubsystem::MenuMap), int32(INDEX_NONE));
	TestEqual(TEXT("nothing is no map"), FindMapByPath(FString()), int32(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadingTipsTest, "ShinobiCoop.UI.LoadingScreen.Tips", ShinobiTestFlags)
bool FShinobiLoadingTipsTest::RunTest(const FString& Parameters)
{
	using namespace ShinobiLoadingScreen;
	const TArray<FText> Coop = Tips(false);
	const TArray<FText> Arena = Tips(true);
	TestTrue(TEXT("co-op has tips"), Coop.Num() >= 5);
	TestTrue(TEXT("arena has tips"), Arena.Num() >= 5);
	for (const TArray<FText>* List : { &Coop, &Arena })
	{
		for (const FText& Tip : *List)
		{
			const FString Line = Tip.ToString();
			TestFalse(FString::Printf(TEXT("every key token is filled in: %s"), *Line), Line.Contains(TEXT("{")) || Line.Contains(TEXT("}")));
			TestTrue(FString::Printf(TEXT("tip fits the panel: %s"), *Line), Line.Len() > 20 && Line.Len() <= 200);
		}
	}
	auto Mentions = [](const TArray<FText>& List, const TCHAR* Word)
	{
		return List.ContainsByPredicate([Word](const FText& Tip) { return Tip.ToString().Contains(Word); });
	};
	TestTrue(TEXT("co-op tips cover reviving"), Mentions(Coop, TEXT("revive")) || Mentions(Coop, TEXT("downed")));
	TestFalse(TEXT("arena tips don't promise a revive"), Mentions(Arena, TEXT("30%")));
	TestTrue(TEXT("arena tips cover rounds"), Mentions(Arena, TEXT("round")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLoadingOffInTestsTest, "ShinobiCoop.UI.LoadingScreen.OffInTests", ShinobiTestFlags)
bool FShinobiLoadingOffInTestsTest::RunTest(const FString& Parameters)
{
	// Automation (like the -unattended shot and smoke scripts) must never get a screen over what it looks at.
	TestFalse(TEXT("off while automation runs"), UShinobiLoadingScreenSubsystem::IsEnabled());
	return true;
}

#endif
