#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Block.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiPerfectSusanooComponent.h"
#include "Shinobi/VFX/ShinobiSusanooComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDamageTest, "ShinobiCoop.Block.Damage.AFifthLandsForStamina", ShinobiTestFlags)
bool FShinobiBlockDamageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Enemy->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Stamina = UShinobiAttributeSet::GetStaminaAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	TestTrue(TEXT("Guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("A blocked hit still lands"), ShinobiCombat::ApplyDamage(Source, P, 100.f));
	TestEqual(TEXT("but only a fifth of it"), ShinobiTest::Get(P, Health), 480.f);
	TestEqual(TEXT("for half its damage in stamina"), ShinobiTest::Get(P, Stamina), 50.f);
	TestEqual(TEXT("and it does not stagger"), Staggers.Count, 0);

	ShinobiCombat::ApplyDamage(Source, P, 10.f);
	TestEqual(TEXT("A small hit: a fifth"), ShinobiTest::Get(P, Health), 478.f);
	TestEqual(TEXT("for the minimum 5 stamina"), ShinobiTest::Get(P, Stamina), 45.f);
	TestTrue(TEXT("Still guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBreakTest, "ShinobiCoop.Block.Damage.AHitTheStaminaCannotPayBreaksTheGuard", ShinobiTestFlags)
bool FShinobiBlockBreakTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);
	UAbilitySystemComponent* Source = Enemy->GetAbilitySystemComponent();
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();
	const FGameplayAttribute Stamina = UShinobiAttributeSet::GetStaminaAttribute();

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::Set(P, Stamina, 30.f);
	ShinobiCombat::ApplyDamage(Source, P, 100.f); // costs 50
	TestEqual(TEXT("The breaking hit lands in full"), ShinobiTest::Get(P, Health), 400.f);
	TestEqual(TEXT("and empties the stamina"), ShinobiTest::Get(P, Stamina), 0.f);
	TestFalse(TEXT("The guard is broken"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("and the player staggers"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	TestEqual(TEXT("One stagger event, not two"), Staggers.Count, 1);

	ShinobiCombat::ApplyDamage(Source, P, 10.f);
	TestEqual(TEXT("The next hit is a normal one"), ShinobiTest::Get(P, Health), 390.f);
	TestEqual(TEXT("and below 20 it does not stagger"), Staggers.Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockSmallBreakTest, "ShinobiCoop.Block.Damage.ASmallBreakingHitStillStaggers", ShinobiTestFlags)
bool FShinobiBlockSmallBreakTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::FEventCounter Staggers(P, ShinobiTags::Event_Hit_Stagger);

	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 3.f);
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 10.f); // costs 5
	TestEqual(TEXT("Lands in full"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 490.f);
	TestFalse(TEXT("The guard is broken"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestEqual(TEXT("A guard break staggers whatever the damage"), Staggers.Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBurningTest, "ShinobiCoop.Block.Damage.BurningIsCutButCostsNothing", ShinobiTestFlags)
bool FShinobiBlockBurningTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplySetByCaller(P, UGE_BlackFlame::StaticClass(), ShinobiTags::Data_Damage, 20.f); // 20 a second
	W.Tick(2.5f); // two ticks
	TestEqual(TEXT("Two ticks of 20, a fifth each"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 492.f);
	TestEqual(TEXT("No stamina spent on burning"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	TestTrue(TEXT("Still guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockSusanooTest, "ShinobiCoop.Block.Damage.StacksWithSusanoo", ShinobiTestFlags)
bool FShinobiBlockSusanooTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestEqual(TEXT("100 x 0.5 x 0.2"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 490.f);
	TestEqual(TEXT("The cost is on what Susanoo let through"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDodgeTest, "ShinobiCoop.Block.Damage.ADodgedHitCostsNoStamina", ShinobiTestFlags)
bool FShinobiBlockDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_DodgeIFrames::StaticClass());
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 50.f);
	TestEqual(TEXT("Nothing lands"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);
	TestEqual(TEXT("and nothing is spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDownedTest, "ShinobiCoop.Block.Damage.ADownedPlayerIsNotGuarding", ShinobiTestFlags)
bool FShinobiBlockDownedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("No guard on a downed player"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockHoldTest, "ShinobiCoop.Block.Ability.HoldToGuardReleaseToLower", ShinobiTestFlags)
bool FShinobiBlockHoldTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("Guard up while held"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestTrue(TEXT("Guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestEqual(TEXT("Rooted while guarding"), Move->GetMaxSpeed(), 0.f);
	W.Tick(1.f);
	TestEqual(TEXT("Holding the guard costs nothing by itself"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 100.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Guard down on release"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestFalse(TEXT("Not guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	TestTrue(TEXT("Free to move again"), Move->GetMaxSpeed() > 0.f);
	TestTrue(TEXT("Stamina regen waits a moment, as after a sprint"), ShinobiTest::HasTag(P, ShinobiTags::State_StaminaRegenBlocked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockExhaustedTest, "ShinobiCoop.Block.Ability.NoGuardWithoutStamina", ShinobiTestFlags)
bool FShinobiBlockExhaustedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 0.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Can't raise a guard at 0 stamina"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockBreakEndsTest, "ShinobiCoop.Block.Ability.AGuardBreakEndsTheAbility", ShinobiTestFlags)
bool FShinobiBlockBreakEndsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(300.f, 0.f, 0.f));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 10.f);
	ShinobiCombat::ApplyDamage(Enemy->GetAbilitySystemComponent(), P, 100.f);
	TestFalse(TEXT("The broken guard ends the ability"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestTrue(TEXT("Staggered"), ShinobiTest::HasTag(P, ShinobiTags::State_Staggered));
	TestEqual(TEXT("The hit landed in full"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 400.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	W.Tick(0.6f); // the stagger is over
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 100.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("The guard can go up again"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockNoAttackTest, "ShinobiCoop.Block.Ability.NoAttackingOrSprintingBehindTheGuard", ShinobiTestFlags)
bool FShinobiBlockNoAttackTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	TestFalse(TEXT("No melee"), ShinobiTest::IsActive(P, UGA_Melee::StaticClass()));
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("No sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestFalse(TEXT("No jutsu"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	TestTrue(TEXT("Still guarding"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDodgeOutTest, "ShinobiCoop.Block.Ability.ADodgeDropsTheGuard", ShinobiTestFlags)
bool FShinobiBlockDodgeOutTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("The dodge goes"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("and the guard drops"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	TestFalse(TEXT("Not guarding"), ShinobiTest::HasTag(P, ShinobiTags::State_Blocking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockCastingTest, "ShinobiCoop.Block.Ability.NoGuardMidCast", ShinobiTestFlags)
bool FShinobiBlockCastingTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	ShinobiTest::EquipPage(P, 5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3); // Susanoo's seals take 0.9 s
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("A jutsu being cast keeps the guard down"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockDownedEndsTest, "ShinobiCoop.Block.Ability.GoingDownEndsTheGuard", ShinobiTestFlags)
bool FShinobiBlockDownedEndsTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("Downed: the guard ends"), ShinobiTest::IsActive(P, UGA_Block::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockFigureTest, "ShinobiCoop.Block.Figure.StandsWhileGuarding", ShinobiTestFlags)
bool FShinobiBlockFigureTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiSusanooComponent* Guard = P->GetGuard();
	if (!TestNotNull(TEXT("Players have a guard figure"), Guard))
	{
		return false;
	}
	TestTrue(TEXT("It is a second figure, not the Susanoo jutsu's"), static_cast<const UActorComponent*>(Guard) != P->GetSusanoo());
	TestFalse(TEXT("Down before guarding"), Guard->IsShown());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("Up while guarding"), Guard->IsShown());
	TestFalse(TEXT("and the 6 m Susanoo stays down"), P->GetSusanoo()->IsShown());

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("Down on release"), Guard->IsShown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockModelTest, "ShinobiCoop.Block.Figure.IsTheBodyHuggingRibcage", ShinobiTestFlags)
bool FShinobiBlockModelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass()); // the figure is built when it is first wanted
	const UStaticMeshComponent* Figure = P->GetGuard() ? P->GetGuard()->GetFigure() : nullptr;
	const UStaticMesh* Mesh = Figure ? Figure->GetStaticMesh() : nullptr;
	if (TestNotNull(TEXT("The guard figure has a mesh (run Scripts/setup_susanoo_ribs.py if not)"), Mesh))
	{
		TestEqual(TEXT("the guard ribcage"), Mesh->GetName(), FString(TEXT("SM_SusanooRibs")));
		const FBox Box = Mesh->GetBoundingBox();
		// The capsule is 192 cm tall and the head's top is at about 180: the ribs stop below it.
		TestTrue(FString::Printf(TEXT("standing on the ground, no taller than the head (Z %.0f to %.0f)"), Box.Min.Z, Box.Max.Z), Box.Min.Z > -20.0 && Box.Min.Z < 20.0 && Box.Max.Z > 160.0 && Box.Max.Z < 180.0);
		// Hugging the body: the 42 cm capsule fits inside, with little room to spare.
		TestTrue(FString::Printf(TEXT("close round the body (Y %.0f to %.0f)"), Box.Min.Y, Box.Max.Y), Box.Max.Y - Box.Min.Y > 90.0 && Box.Max.Y - Box.Min.Y < 115.0);
		TestTrue(TEXT("centred on the player"), FMath::Abs(Box.GetCenter().X) < 25.0 && FMath::Abs(Box.GetCenter().Y) < 25.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockPoseTest, "ShinobiCoop.Block.Pose.GuardsWhileHeld", ShinobiTestFlags)
bool FShinobiBlockPoseTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py with JUTSU_ONLY=Block if the montage is missing)
	const UAnimMontage* Pose = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Block_Guard.AM_Block_Guard"));
	if (!TestNotNull(TEXT("guard montage"), Pose))
	{
		return false;
	}
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UAnimInstance* Anim = P->GetMesh()->GetAnimInstance();
	if (!TestNotNull(TEXT("the character's animation instance"), Anim))
	{
		return false;
	}
	W.Tick(0.3f);
	TestFalse(TEXT("No guard pose before guarding"), Anim->Montage_IsPlaying(Pose));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	W.Tick(0.3f);
	TestTrue(TEXT("Guard pose while guarding"), Anim->Montage_IsPlaying(Pose));
	W.Tick(Pose->GetPlayLength() * 2.5f);
	TestTrue(TEXT("and still after the clip has looped"), Anim->Montage_IsPlaying(Pose));

	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	W.Tick(1.f);
	TestFalse(TEXT("Gone on release"), Anim->Montage_IsPlaying(Pose));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockNoJumpTest, "ShinobiCoop.Block.Ability.NoJumpingOrVaultingBehindTheGuard", ShinobiTestFlags)
bool FShinobiBlockNoJumpTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestTrue(TEXT("Free to jump"), P->CanJumpOrTraverse());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("A guard stays on the ground: no jump, vault or mantle"), P->CanJumpOrTraverse());
	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("Free to jump again"), P->CanJumpOrTraverse());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockPredictedRootTest, "ShinobiCoop.Block.Ability.TheRootIsAPredictedMoveFlag", ShinobiTestFlags)
bool FShinobiBlockPredictedRootTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	TestTrue(TEXT("The ability sets the move flag (on the owning client too)"), Move->IsBlocking());
	ShinobiTest::Release(P, EShinobiAbilityInputID::Block);
	TestFalse(TEXT("and clears it on release"), Move->IsBlocking());

	// The root follows the flag, not the server's tag, so a client roots (and frees) itself without waiting a round trip.
	Move->SetBlocking(true);
	TestEqual(TEXT("Rooted by the flag alone"), Move->GetMaxSpeed(), 0.f);
	Move->SetBlocking(false);
	ShinobiTest::ApplyEffect(P, UGE_Blocking::StaticClass());
	TestTrue(TEXT("The tag alone does not root"), Move->GetMaxSpeed() > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlockQuietRiseTest, "ShinobiCoop.Block.Figure.RisesWithoutAFlash", ShinobiTestFlags)
bool FShinobiBlockQuietRiseTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Press(P, EShinobiAbilityInputID::Block);
	// The Susanoo rise burst's star card floated over the player's head: the ribcage fading in is the whole effect.
	TestEqual(TEXT("No burst when the guard goes up"), ShinobiTest::CountActors<AShinobiBurst>(W), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
