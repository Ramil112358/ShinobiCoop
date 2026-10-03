#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiSusanooComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooDamageTest, "ShinobiCoop.Susanoo.HalvesDamageTakenAndBoostsDamageDealt", ShinobiTestFlags)
bool FShinobiSusanooDamageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	AShinobiCharacterBase* Enemy2 = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	TestTrue(TEXT("Susanoo is on"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestTrue(TEXT("An enemy's 100 lands"), ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f));
	TestEqual(TEXT("but only half of it does"), ShinobiTest::Get(P, Health), 450.f);

	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Enemy, 100.f);
	TestEqual(TEXT("The caster's 100 hits for 130"), ShinobiTest::Get(Enemy, Health), 370.f);

	// Susanoo on both sides: 100 x 1.3 x 0.5.
	ShinobiTest::ApplyEffect(Enemy2, UGE_Susanoo::StaticClass());
	ShinobiCombat::ApplyDamage(P->GetAbilitySystemComponent(), Enemy2, 100.f);
	TestEqual(TEXT("Armour against armour: x1.3 x0.5"), ShinobiTest::Get(Enemy2, Health), 435.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooPeriodicTest, "ShinobiCoop.Susanoo.HalvesBurningAndEndsAfterTwentySeconds", ShinobiTestFlags)
bool FShinobiSusanooPeriodicTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::ApplySetByCaller(P, UGE_BlackFlame::StaticClass(), ShinobiTags::Data_Damage, 20.f); // 20 a second, self-inflicted
	W.Tick(2.5f);
	// Two ticks of 20: the caster is both source and target here, so x1.3 x0.5 = 13 each.
	TestEqual(TEXT("Burning damage is scaled too"), ShinobiTest::Get(P, Health), 474.f);

	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Burning_Black));
	W.Tick(17.0f); // 19.5 s since the cast
	TestTrue(TEXT("Still on just before 20 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	W.Tick(1.0f);
	TestFalse(TEXT("Over after 20 s"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestEqual(TEXT("and damage is full again"), ShinobiTest::Get(P, Health), 374.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooChakraTest, "ShinobiCoop.Susanoo.RegeneratesChakraFaster", ShinobiTestFlags)
bool FShinobiSusanooChakraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FGameplayAttribute Chakra = UShinobiAttributeSet::GetChakraAttribute();

	ShinobiTest::Set(P, Chakra, 0.f);
	W.Tick(2.0f);
	const float Normal = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("Normally about 4 in two seconds (%.1f)"), Normal), Normal > 2.f && Normal < 6.f);

	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::Set(P, Chakra, 0.f);
	W.Tick(2.0f);
	const float Boosted = ShinobiTest::Get(P, Chakra);
	TestTrue(FString::Printf(TEXT("With Susanoo about 12 (%.1f)"), Boosted), Boosted > 9.f && Boosted < 15.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooCastTest, "ShinobiCoop.Susanoo.CastCostsCooldownAndRaisesTheArmour", ShinobiTestFlags)
bool FShinobiSusanooCastTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestEqual(TEXT("Cost 40 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 60.f);
	TestFalse(TEXT("Not armoured during the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	W.Tick(0.9f);
	TestTrue(TEXT("Armoured after the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Susanoo));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("It cannot be recast while it is up"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooPageTest, "ShinobiCoop.Susanoo.OnlyFiresFromPageFour", ShinobiTestFlags)
bool FShinobiSusanooPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	P->ApplyJutsuPage(2); // key 1 is Teleport there
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	TestFalse(TEXT("Not on page 2"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooFigureTest, "ShinobiCoop.Susanoo.FigureStandsWhileTheArmourIsUp", ShinobiTestFlags)
bool FShinobiSusanooFigureTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiSusanooComponent* Armour = P->GetSusanoo();
	if (!TestNotNull(TEXT("Every character has the figure component"), Armour))
	{
		return false;
	}
	TestFalse(TEXT("No figure before the cast"), Armour->IsShown());
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	TestTrue(TEXT("The figure stands while the armour is up"), Armour->IsShown());
	W.Tick(21.f);
	TestFalse(TEXT("and goes when it ends"), Armour->IsShown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooModelTest, "ShinobiCoop.Susanoo.FigureIsTheRibcageModelStandingOnTheGround", ShinobiTestFlags)
bool FShinobiSusanooModelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UStaticMeshComponent* Figure = P->GetSusanoo() ? P->GetSusanoo()->GetFigure() : nullptr;
	const UStaticMesh* Mesh = Figure ? Figure->GetStaticMesh() : nullptr;
	if (TestNotNull(TEXT("The figure has a mesh (run Scripts/setup_susanoo.py if not)"), Mesh))
	{
		TestEqual(TEXT("the Susanoo ribcage"), Mesh->GetName(), FString(TEXT("SM_Susanoo")));
		const FBox Box = Mesh->GetBoundingBox();
		TestTrue(FString::Printf(TEXT("standing on the ground, about 6 m tall (Z %.0f to %.0f)"), Box.Min.Z, Box.Max.Z), Box.Min.Z > -20.0 && Box.Min.Z < 20.0 && Box.Max.Z > 500.0 && Box.Max.Z < 700.0);
		TestTrue(TEXT("facing +X: the arms reach forward"), Box.Max.X > 120.0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
