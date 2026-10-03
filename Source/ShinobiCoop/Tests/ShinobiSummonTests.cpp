#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Abilities/GA_Summons.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/Texture2D.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/** A summoned pack, hovering (the test world has no floor). */
	TArray<AShinobiSummon*> Summon(AShinobiCharacterBase* Caster, EShinobiSummonKind Kind, const FVector& Ground)
	{
		TArray<AShinobiSummon*> Pack = AShinobiSummon::Summon(Caster, Kind, Ground);
		for (AShinobiSummon* Each : Pack)
		{
			ShinobiTest::Hover(Each);
		}
		return Pack;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonTeamTest, "ShinobiCoop.Summons.FightForTheCasterNotAgainstThem", ShinobiTestFlags)
bool FShinobiSummonTeamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);

	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("One toad"), Pack.Num(), 1);
	if (Pack.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("On the players' side"), Pack[0]->GetTeam(), EShinobiTeam::Players);
	TestTrue(TEXT("Hostile to enemies"), Pack[0]->IsHostileTo(Ninja) && Ninja->IsHostileTo(Pack[0]));
	TestFalse(TEXT("Not hostile to the caster"), Pack[0]->IsHostileTo(Caster) || Caster->IsHostileTo(Pack[0]));
	TestTrue(TEXT("Remembers its summoner"), Pack[0]->GetSummoner() == Caster);
	TestEqual(TEXT("Toad health"), ShinobiTest::Get(Pack[0], UShinobiAttributeSet::GetMaxHealthAttribute()), 600.f);

	// A jutsu that hits everything hostile around the caster leaves the summon alone.
	const TArray<AShinobiCharacterBase*> Hit = ShinobiCombat::FindHostilesInRadius(Caster, FVector::ZeroVector, 1000.f);
	TestFalse(TEXT("The caster's area jutsu skips the summon"), Hit.Contains(Pack[0]));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonFightTest, "ShinobiCoop.Summons.AttackEnemiesAndEnemiesAttackThem", ShinobiTestFlags)
bool FShinobiSummonFightTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-5000.f, 0.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>();
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != 1)
	{
		return false;
	}

	W.Tick(0.6f);
	TestTrue(TEXT("The ninja heads for the toad, not the far-off caster"), Ninja->GetActorLocation().X > 100.f);
	W.Tick(3.f);
	TestTrue(TEXT("The toad hurt the ninja"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()) < 200.f);
	TestEqual(TEXT("The caster was never touched"), ShinobiTest::Get(Caster, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLifetimeTest, "ShinobiCoop.Summons.LeaveWhenTheirTimeIsUp", ShinobiTestFlags)
bool FShinobiSummonLifetimeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Snake, FVector(300.f, 0.f, 0.f));
	const float Lifetime = AShinobiSummon::GetLifetime(EShinobiSummonKind::Snake);

	W.Tick(Lifetime - 1.f);
	TestEqual(TEXT("Still here before its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);
	W.Tick(1.2f);
	TestEqual(TEXT("Gone after its time"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonLimitTest, "ShinobiCoop.Summons.OneOfEachKindPerCaster", ShinobiTestFlags)
bool FShinobiSummonLimitTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* Other = W.Spawn<AShinobiPlayerCharacter>(FVector(0.f, 3000.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Other);

	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 0.f, 0.f));
	W.Tick(0.1f);
	Summon(Caster, EShinobiSummonKind::Toad, FVector(300.f, 300.f, 0.f));
	TestEqual(TEXT("The second toad replaced the first"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	Summon(Caster, EShinobiSummonKind::Snake, FVector(-300.f, 0.f, 0.f));
	TestEqual(TEXT("A snake joins the toad"), ShinobiTest::CountActors<AShinobiSummon>(W), 2);
	Summon(Other, EShinobiSummonKind::Toad, FVector(0.f, 3300.f, 0.f));
	TestEqual(TEXT("Another caster's toad is not replaced"), ShinobiTest::CountActors<AShinobiSummon>(W), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonDogsTest, "ShinobiCoop.Summons.DogsComeAsAPackOfFour", ShinobiTestFlags)
bool FShinobiSummonDogsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("Four ninken"), Pack.Num(), AShinobiSummon::DogPackSize);
	TestEqual(TEXT("Four dogs stand"), ShinobiTest::CountActors<AShinobiSummon>(W), AShinobiSummon::DogPackSize);

	FVector Min(FLT_MAX), Max(-FLT_MAX);
	for (const AShinobiSummon* Dog : Pack)
	{
		Min = Min.ComponentMin(Dog->GetActorLocation());
		Max = Max.ComponentMax(Dog->GetActorLocation());
		TestEqual(TEXT("Dog health"), ShinobiTest::Get(const_cast<AShinobiSummon*>(Dog), UShinobiAttributeSet::GetMaxHealthAttribute()), 120.f);
	}
	TestTrue(TEXT("Spread out, not stacked"), (Max - Min).Size2D() > 100.f);

	Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	TestEqual(TEXT("Calling the pack again replaces it"), ShinobiTest::CountActors<AShinobiSummon>(W), AShinobiSummon::DogPackSize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonDeathTest, "ShinobiCoop.Summons.AFallenSummonIsNotCounted", ShinobiTestFlags)
bool FShinobiSummonDeathTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	ShinobiTest::Damage(Pack[0], 500.f);
	TestFalse(TEXT("Dog fell"), Pack[0]->IsAlive());

	// Calling the pack again does not wait on the body: three of the old four were alive and go; the new four arrive.
	Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	int32 Alive = 0;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Alive += It->IsAlive() ? 1 : 0;
	}
	TestEqual(TEXT("Four living dogs"), Alive, AShinobiSummon::DogPackSize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCastToadTest, "ShinobiCoop.Summons.ToadJutsuCostsChakraThenCallsTheToad", ShinobiTestFlags)
bool FShinobiSummonCastToadTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	TestEqual(TEXT("Cost 60 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 40.f);
	TestEqual(TEXT("No toad during the seals"), ShinobiTest::CountActors<AShinobiSummon>(W), 0);
	W.Tick(1.2f);
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonToad));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_SummonToad::StaticClass()));

	AShinobiSummon* Toad = nullptr;
	for (TActorIterator<AShinobiSummon> It(W.World); It; ++It)
	{
		Toad = *It;
	}
	if (!TestNotNull(TEXT("The toad is here"), Toad))
	{
		return false;
	}
	TestEqual(TEXT("It is a toad"), Toad->GetKind(), EShinobiSummonKind::Toad);
	TestTrue(TEXT("It is the caster's"), Toad->GetSummoner() == P);
	TestTrue(FString::Printf(TEXT("and stands in front of the caster (X %.0f)"), Toad->GetActorLocation().X), Toad->GetActorLocation().X > 200.f && Toad->GetActorLocation().X < 600.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonCastSnakeAndDogsTest, "ShinobiCoop.Summons.SnakeAndDogJutsuCallTheirKind", ShinobiTestFlags)
bool FShinobiSummonCastSnakeAndDogsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Snake cost 50"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 50.f);
	W.Tick(1.2f);
	TestTrue(TEXT("Snake cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonSnake));
	TestEqual(TEXT("One snake"), ShinobiTest::CountActors<AShinobiSummon>(W), 1);

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	const float Left = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("Dogs cost 45 out of the ~52 regenerated so far (%.1f left)"), Left), Left > 3.f && Left < 12.f);
	W.Tick(1.2f);
	TestTrue(TEXT("Dog cooldown"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_SummonDogs));
	TestEqual(TEXT("The snake and four dogs"), ShinobiTest::CountActors<AShinobiSummon>(W), 1 + AShinobiSummon::DogPackSize);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonPageTest, "ShinobiCoop.Summons.OnlyCallableFromPageFour", ShinobiTestFlags)
bool FShinobiSummonPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu4);
	TestFalse(TEXT("Toad not on page 2"), ShinobiTest::IsActive(P, UGA_SummonToad::StaticClass()));
	TestFalse(TEXT("Snake not on page 2"), ShinobiTest::IsActive(P, UGA_SummonSnake::StaticClass()));
	TestFalse(TEXT("Dogs not on page 2"), ShinobiTest::IsActive(P, UGA_SummonDogs::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiToadSlamTest, "ShinobiCoop.Summons.ToadSlamHitsEveryoneAroundIt", ShinobiTestFlags)
bool FShinobiToadSlamTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(5000.f, 0.f, 0.f));
	AShinobiRogueNinja* InFront = W.Spawn<AShinobiRogueNinja>(FVector(350.f, 0.f, 0.f));
	AShinobiRogueNinja* Behind = W.Spawn<AShinobiRogueNinja>(FVector(-300.f, 0.f, 0.f));
	AShinobiRogueNinja* Far = W.Spawn<AShinobiRogueNinja>(FVector(0.f, 1500.f, 0.f));
	for (AShinobiCharacterBase* Each : { (AShinobiCharacterBase*)Caster, (AShinobiCharacterBase*)InFront, (AShinobiCharacterBase*)Behind, (AShinobiCharacterBase*)Far })
	{
		ShinobiTest::Hover(Each);
	}
	// Held still, so only the slam can have hurt them.
	for (AShinobiRogueNinja* Each : { InFront, Behind, Far })
	{
		ShinobiTest::ApplyEffect(Each, UGE_Shock::StaticClass());
	}
	Summon(Caster, EShinobiSummonKind::Toad, FVector::ZeroVector);

	W.Tick(1.2f);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	TestEqual(TEXT("In front: one slam of 60"), ShinobiTest::Get(InFront, Health), 140.f);
	TestEqual(TEXT("Behind: the slam is all around"), ShinobiTest::Get(Behind, Health), 140.f);
	TestEqual(TEXT("Out of reach: untouched"), ShinobiTest::Get(Far, Health), 200.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSnakeStrikeTest, "ShinobiCoop.Summons.SnakeStrikesFromFarOff", ShinobiTestFlags)
bool FShinobiSnakeStrikeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(5000.f, 0.f, 0.f));
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(550.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass()); // rooted for 1.5 s
	Summon(Caster, EShinobiSummonKind::Snake, FVector::ZeroVector);

	W.Tick(1.2f);
	TestEqual(TEXT("Bitten once for 70 from 550 away"), ShinobiTest::Get(Ninja, UShinobiAttributeSet::GetHealthAttribute()), 130.f);
	TestTrue(TEXT("The ninja never got close"), FVector::Dist2D(Ninja->GetActorLocation(), FVector::ZeroVector) > 450.f);
	return true;
}

namespace
{
	FString PlayingClip(const AShinobiSummon* Dog)
	{
		UAnimSingleNodeInstance* Instance = Dog->GetBodyMesh()->GetSingleNodeInstance();
		return Instance && Instance->GetCurrentAsset() ? Instance->GetCurrentAsset()->GetName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDogLookTest, "ShinobiCoop.Summons.DogsWearTheDarkWolfFacingForward", ShinobiTestFlags)
bool FShinobiDogLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector(400.f, 0.f, 0.f));
	if (Pack.Num() != AShinobiSummon::DogPackSize)
	{
		return false;
	}
	const AShinobiSummon* Dog = Pack[0];
	const USkeletalMeshComponent* Wolf = Dog->GetBodyMesh();
	TestTrue(TEXT("The wolf is the visible body"), Wolf != Dog->GetMesh());
	if (!TestNotNull(TEXT("with a mesh (the Animal Variety Pack is imported)"), Wolf->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("the wolf"), Wolf->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Wolf")));
	TestFalse(TEXT("The mannequin under it is hidden"), Dog->GetMesh()->IsVisible());
	TestTrue(TEXT("It faces the way the dog does (the wolf model faces +Y)"), FVector::DotProduct(Wolf->GetComponentTransform().TransformVectorNoScale(FVector(0.f, 1.f, 0.f)), Dog->GetActorForwardVector()) > 0.99f);
	TestTrue(TEXT("in the dark coat"), Wolf->GetMaterial(0) && Wolf->GetMaterial(0)->GetName().Contains(TEXT("Dark")));
	TestTrue(TEXT("The capsule is dog-sized, not a person's"), Dog->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < 70.f);
	const float FeetGap = (Wolf->Bounds.Origin.Z - Wolf->Bounds.BoxExtent.Z) - (Dog->GetActorLocation().Z - Dog->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("Paws on the capsule's floor (off by %.0f)"), FeetGap), FMath::Abs(FeetGap) < 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDogAnimTest, "ShinobiCoop.Summons.DogsIdleRunAndBite", ShinobiTestFlags)
bool FShinobiDogAnimTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Dog, FVector::ZeroVector);
	const AShinobiSummon* Dog = Pack[0];
	W.Tick(0.2f);
	TestTrue(FString::Printf(TEXT("Idle with nothing to fight (%s)"), *PlayingClip(Dog)), PlayingClip(Dog).Contains(TEXT("Idle")));

	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(900.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	W.Tick(0.3f);
	TestEqual(TEXT("Runs at the ninja"), PlayingClip(Dog), FString(TEXT("ANIM_Wolf_Run")));

	bool bBit = false;
	for (int32 Step = 0; Step < 40 && !bBit; ++Step)
	{
		W.Tick(0.05f);
		bBit = ShinobiTest::HasTag(Dog, ShinobiTags::State_SummonAttacking);
	}
	TestTrue(TEXT("Bites once it arrives"), bBit);
	TestEqual(TEXT("with the bite clip"), PlayingClip(Dog), FString(TEXT("ANIM_Wolf_Bite")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSnakeLookTest, "ShinobiCoop.Summons.SnakeIsTheLongSnakeFacingForwardAndDiesAnimated", ShinobiTestFlags)
bool FShinobiSnakeLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-5000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Snake, FVector::ZeroVector);
	if (Pack.Num() != 1)
	{
		return false;
	}
	const AShinobiSummon* Snake = Pack[0];
	const USkeletalMeshComponent* Body = Snake->GetBodyMesh();
	if (!TestTrue(TEXT("The snake model is the visible body (run Scripts/setup_creatures.py if not)"), Body != Snake->GetMesh() && Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("the snake"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Snake")));
	const FBoxSphereBounds Bounds = Body->Bounds;
	TestTrue(FString::Printf(TEXT("long along the way it faces (%.0f x %.0f)"), Bounds.BoxExtent.X * 2.f, Bounds.BoxExtent.Y * 2.f), Bounds.BoxExtent.X > 4.f * Bounds.BoxExtent.Y && Bounds.BoxExtent.X * 2.f > 800.f);
	TestTrue(TEXT("and centred on the capsule"), FMath::Abs(Bounds.Origin.X - Snake->GetActorLocation().X) < 80.f);
	W.Tick(0.1f);
	TestEqual(TEXT("It idles"), PlayingClip(Snake), FString(TEXT("ANIM_Snake_Idle")));

	ShinobiTest::Damage(const_cast<AShinobiSummon*>(Snake), 999.f);
	W.Tick(0.2f);
	TestEqual(TEXT("and plays its death clip when it falls"), PlayingClip(Snake), FString(TEXT("ANIM_Snake_Death")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiToadLookTest, "ShinobiCoop.Summons.ToadIsTheRiggedToadThatHopsAndSlams", ShinobiTestFlags)
bool FShinobiToadLookTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Caster = W.Spawn<AShinobiPlayerCharacter>(FVector(-9000.f, 0.f, 0.f));
	ShinobiTest::Hover(Caster);
	const TArray<AShinobiSummon*> Pack = Summon(Caster, EShinobiSummonKind::Toad, FVector::ZeroVector);
	if (Pack.Num() != 1)
	{
		return false;
	}
	AShinobiSummon* Toad = Pack[0];
	const USkeletalMeshComponent* Body = Toad->GetBodyMesh();
	if (!TestTrue(TEXT("The toad model is the visible body (run Scripts/setup_creatures.py if not)"), Body != Toad->GetMesh() && Body->GetSkeletalMeshAsset()))
	{
		return false;
	}
	TestEqual(TEXT("the toad"), Body->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Toad")));
	const FBoxSphereBounds Bounds = Body->Bounds;
	TestTrue(FString::Printf(TEXT("a giant: %.0f long, %.0f tall"), Bounds.BoxExtent.X * 2.f, Bounds.BoxExtent.Z * 2.f), Bounds.BoxExtent.X * 2.f > 400.f && Bounds.BoxExtent.Z * 2.f > 300.f);
	const float FeetGap = (Bounds.Origin.Z - Bounds.BoxExtent.Z) - (Toad->GetActorLocation().Z - Toad->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	TestTrue(FString::Printf(TEXT("standing on the capsule's floor (off by %.0f)"), FeetGap), FMath::Abs(FeetGap) < 20.f);
	W.Tick(0.1f);
	TestEqual(TEXT("It idles"), PlayingClip(Toad), FString(TEXT("ANIM_Toad_Idle")));

	// Something to slam: held still next to it.
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::Hover(Ninja);
	ShinobiTest::ApplyEffect(Ninja, UGE_Shock::StaticClass());
	bool bSlamming = false;
	for (int32 Step = 0; Step < 30 && !bSlamming; ++Step)
	{
		W.Tick(0.05f);
		bSlamming = ShinobiTest::HasTag(Toad, ShinobiTags::State_SummonAttacking);
	}
	TestTrue(TEXT("It rears up to slam"), bSlamming);
	TestEqual(TEXT("with the slam clip"), PlayingClip(Toad), FString(TEXT("ANIM_Toad_Slam")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSummonAssetsTest, "ShinobiCoop.Summons.JutsuHaveSealAnimationsAndIcons", ShinobiTestFlags)
bool FShinobiSummonAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	for (const UGA_SummonBase* Jutsu : { (const UGA_SummonBase*)GetDefault<UGA_SummonToad>(), (const UGA_SummonBase*)GetDefault<UGA_SummonSnake>(), (const UGA_SummonBase*)GetDefault<UGA_SummonDogs>() })
	{
		const FString Name = Jutsu->DisplayName.ToString();
		TestNotNull(Name + TEXT(" seal montage"), Jutsu->SealMontage.LoadSynchronous());
		TestNotNull(Name + TEXT(" release montage"), Jutsu->ReleaseMontage.LoadSynchronous());
		TestNotNull(Name + TEXT(" icon"), Jutsu->Icon.LoadSynchronous());
	}
	return true;
}

#endif
