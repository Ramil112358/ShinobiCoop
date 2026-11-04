#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Components/PointLightComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Shinobi/ShinobiMeteor.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/UI/ShinobiGraphics.h"
#include "Shinobi/UI/ShinobiSettings.h"
#include "Shinobi/VFX/ShinobiInstancedVfx.h"
#include "Shinobi/VFX/ShinobiVfx.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCharactersOffNaniteTest, "ShinobiCoop.Render.CharactersStayOffNanite", ShinobiTestFlags)
bool FShinobiCharactersOffNaniteTest::RunTest(const FString& Parameters)
{
	// Nanite skinned meshes aren't production-ready in 5.8, and the characters are the worst case for them: MetaHuman faces and
	// groom hair, Mutable bodies, outfits with Chaos cloth. Every roster body, every outfit fitted to it, and whatever skinned
	// mesh a spawned player wears stay on the regular skinned path.
	int32 Checked = 0;
	const auto Expect = [this, &Checked](const USkinnedAsset* Asset, const FString& What)
	{
		if (const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset))
		{
			TestFalse(FString::Printf(TEXT("%s (%s) is not Nanite"), *What, *Mesh->GetPathName()), Mesh->GetNaniteSettings().bEnabled);
			++Checked;
		}
	};
	for (int32 Body = 0; Body < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Roster); ++Body)
	{
		const FShinobiRosterEntry& Entry = AShinobiPlayerCharacter::Roster[Body];
		if (Entry.Mesh)
		{
			Expect(LoadObject<USkeletalMesh>(nullptr, Entry.Mesh), FString::Printf(TEXT("roster body %s"), Entry.Name));
		}
		for (int32 Outfit = 1; Outfit < UE_ARRAY_COUNT(AShinobiPlayerCharacter::Outfits); ++Outfit)
		{
			if (USkeletalMesh* Worn = LoadObject<USkeletalMesh>(nullptr, *AShinobiPlayerCharacter::OutfitMeshPath(Outfit, Body), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Expect(Worn, FString::Printf(TEXT("outfit %s on %s"), AShinobiPlayerCharacter::Outfits[Outfit].Name, Entry.Name));
			}
		}
	}
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TArray<USkinnedMeshComponent*> Worn;
	P->GetComponents<USkinnedMeshComponent>(Worn);
	for (const USkinnedMeshComponent* Component : Worn)
	{
		Expect(Component->GetSkinnedAsset(), FString::Printf(TEXT("the player's %s"), *Component->GetName()));
	}
	TestTrue(FString::Printf(TEXT("character meshes were checked (%d)"), Checked), Checked > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiHailInstancedTest, "ShinobiCoop.Render.IceHailIsDrawnInstanced", ShinobiTestFlags)
bool FShinobiHailInstancedTest::RunTest(const FString& Parameters)
{
	// The Ice Meteor shower's shards, chips and frost trails are instances of a few instanced meshes, not mesh components:
	// a shower of 12 here draws 48 pieces from at most 4 components (3 shard variants, the trail), and gives them back.
	FShinobiTestWorld W;
	UShinobiInstancedVfxSubsystem* Instances = W.World->GetSubsystem<UShinobiInstancedVfxSubsystem>();
	if (!TestNotNull(TEXT("the instanced VFX subsystem runs in a game world"), Instances))
	{
		return false;
	}
	constexpr int32 Shards = 12;
	for (int32 Index = 0; Index < Shards; ++Index)
	{
		AShinobiMeteor::Spawn(W.World, nullptr, EShinobiMeteor::Ice, FVector(800.f + Index * 40.f, 0.f, 0.f), 0.5f, 1.f, FShinobiMeteorImpact(), Index * 0.05f);
	}
	W.Tick(0.1f);
	TestEqual(TEXT("every shard is still an actor (landing, damage, replication)"), ShinobiTest::CountActors<AShinobiMeteor>(W), Shards);
	TestEqual(TEXT("each draws its shard, trail and two chips as instances"), Instances->GetShownCount(), Shards * 4);
	TestTrue(FString::Printf(TEXT("from a few instanced meshes (%d)"), Instances->GetPoolCount()), Instances->GetPoolCount() <= 4);
	for (TActorIterator<AShinobiMeteor> It(W.World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Parts;
		It->GetComponents<UStaticMeshComponent>(Parts);
		int32 Drawn = 0;
		for (const UStaticMeshComponent* Part : Parts)
		{
			Drawn += Part->GetStaticMesh() != nullptr ? 1 : 0;
		}
		TestEqual(TEXT("a shard draws no mesh component of its own"), Drawn, 0);
		TestEqual(TEXT("and has its four pieces"), It->GetInstancedPieceCount(), 4);
	}
	W.Tick(2.f);
	TestEqual(TEXT("all landed"), ShinobiTest::CountActors<AShinobiMeteor>(W), 0);
	TestEqual(TEXT("their pieces were given back"), Instances->GetShownCount(), 0);

	// A second shower reuses the slots.
	AShinobiMeteor::Spawn(W.World, nullptr, EShinobiMeteor::Ice, FVector(800.f, 0.f, 0.f), 0.5f, 1.f, FShinobiMeteorImpact());
	W.Tick(0.1f);
	TestEqual(TEXT("a new shard shows again"), Instances->GetShownCount(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMegaLightsEffectLightsTest, "ShinobiCoop.Render.MegaLightsGivesEffectLightsShadows", ShinobiTestFlags)
bool FShinobiMegaLightsEffectLightsTest::RunTest(const FString& Parameters)
{
	// Jutsu and effect lights cast shadows only while MegaLights is on (cheap with it; six shadow views each without it), and
	// the setting reaches the ones already burning.
	const TCHAR* Section = TEXT("/Script/ShinobiCoop.ShinobiSettings");
	bool bOld = false;
	const bool bHadOld = GConfig->GetBool(Section, TEXT("bMegaLights"), bOld, GGameUserSettingsIni);
	FShinobiTestWorld W;
	AActor* Holder = W.World->SpawnActor<AActor>();
	UPointLightComponent* Burning = NewObject<UPointLightComponent>(Holder);
	Burning->RegisterComponent();
	using ShinobiSettings::ERow;

	ShinobiGraphics::FCaps None;
	None.MegaLightsLock = TEXT("Requires a graphics card with hardware ray tracing, running DirectX 12.");
	ShinobiGraphics::SetCapsOverride(&None);
	TestFalse(TEXT("without ray tracing: MegaLights is locked"), ShinobiSettings::RowLock(ERow::MegaLights).IsEmpty());
	ShinobiSettings::SetRowIndex(ERow::MegaLights, W.World, 1);
	TestFalse(TEXT("...and stays off"), ShinobiGraphics::IsMegaLights());
	ShinobiVfx::ApplyEffectLight(Burning);
	TestFalse(TEXT("an effect light casts no shadow"), bool(Burning->CastShadows));
	TestTrue(TEXT("...and is tagged"), Burning->ComponentTags.Contains(ShinobiVfx::EffectLightTag));

	ShinobiGraphics::FCaps Rtx;
	Rtx.bRayTracing = Rtx.bMegaLights = true;
	ShinobiGraphics::SetCapsOverride(&Rtx);
	ShinobiSettings::SetRowIndex(ERow::MegaLights, W.World, 1);
	TestTrue(TEXT("with ray tracing: MegaLights on"), ShinobiGraphics::IsMegaLights());
	TestTrue(TEXT("the burning effect light now casts shadows"), bool(Burning->CastShadows));
	UPointLightComponent* Lit = NewObject<UPointLightComponent>(Holder);
	ShinobiVfx::ApplyEffectLight(Lit);
	TestTrue(TEXT("a new one does too"), bool(Lit->CastShadows));
	UPointLightComponent* Plain = NewObject<UPointLightComponent>(Holder);
	Plain->SetCastShadows(false);
	Plain->RegisterComponent();
	ShinobiSettings::SetRowIndex(ERow::MegaLights, W.World, 0);
	TestFalse(TEXT("MegaLights off: shadows off again"), bool(Burning->CastShadows));
	ShinobiSettings::SetRowIndex(ERow::MegaLights, W.World, 1);
	TestFalse(TEXT("a light that isn't an effect light is left alone"), bool(Plain->CastShadows));

	ShinobiGraphics::SetCapsOverride(nullptr);
	if (bHadOld)
	{
		GConfig->SetBool(Section, TEXT("bMegaLights"), bOld, GGameUserSettingsIni);
	}
	else
	{
		GConfig->RemoveKey(Section, TEXT("bMegaLights"), GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	ShinobiGraphics::ApplySaved();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiLightingQualityTest, "ShinobiCoop.Render.LightingQualityPicksLumenLite", ShinobiTestFlags)
bool FShinobiLightingQualityTest::RunTest(const FString& Parameters)
{
	// Lighting quality sets global illumination and reflections together; Medium is 5.8's Lumen Lite.
	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	const int32 OldGI = Settings->GetGlobalIlluminationQuality();
	const int32 OldReflections = Settings->GetReflectionQuality();
	FShinobiTestWorld W;
	using ShinobiSettings::ERow;
	TestTrue(TEXT("listed under Graphics quality"), ShinobiSettings::RowsFor(true).IndexOfByKey(ERow::LightingQuality) == ShinobiSettings::RowsFor(true).IndexOfByKey(ERow::Quality) + 1);
	ShinobiSettings::SetRowIndex(ERow::LightingQuality, W.World, 1);
	TestEqual(TEXT("Medium: global illumination 1"), Settings->GetGlobalIlluminationQuality(), 1);
	TestEqual(TEXT("...and reflections 1"), Settings->GetReflectionQuality(), 1);
	TestEqual(TEXT("shown as Lumen Lite"), ShinobiSettings::RowValue(ERow::LightingQuality), FString(TEXT("Medium (Lumen Lite)")));
	ShinobiSettings::SetRowIndex(ERow::LightingQuality, W.World, 3);
	TestEqual(TEXT("Epic"), ShinobiSettings::RowValue(ERow::LightingQuality), FString(TEXT("Epic")));

	Settings->SetGlobalIlluminationQuality(OldGI);
	Settings->SetReflectionQuality(OldReflections);
	Settings->ApplyNonResolutionSettings();
	Settings->SaveSettings();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
