#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: some use editor-only APIs)

#include "AIController.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/Abilities/GA_Sprint.h"
#include "Shinobi/Abilities/GA_Dodge.h"
#include "Shinobi/Abilities/GA_ChakraCharge.h"
#include "Shinobi/Abilities/GA_Melee.h"
#include "Shinobi/VFX/ShinobiChakraAuraComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintTest, "ShinobiCoop.Abilities.SprintDrainsStaminaThenRegenAfterDelay", ShinobiTestFlags)
bool FShinobiSprintTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	// Max speed is not asserted: test actors have no floor, so they are falling and sprint speed only applies when walking.
	TestTrue(TEXT("Movement sprinting"), Move->IsSprinting());

	W.Tick(1.f);
	const float AfterSprint = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	TestTrue(FString::Printf(TEXT("Stamina drained ~10 (got %.2f)"), AfterSprint), AfterSprint >= 86.f && AfterSprint <= 91.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint ended on release"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting"), Move->IsSprinting());

	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after sprint"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), AfterSprint);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > AfterSprint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintExhaustTest, "ShinobiCoop.Abilities.SprintEndsAtZeroStamina", ShinobiTestFlags)
bool FShinobiSprintExhaustTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 5.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(1.f);
	TestFalse(TEXT("Sprint ended when exhausted"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Stamina empty"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	TestFalse(TEXT("Movement not sprinting after exhaustion"), Move->IsSprinting());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Can't start sprint at 0 stamina"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSprintCancelTest, "ShinobiCoop.Abilities.SprintCancelClearsFlagAndDelaysRegen", ShinobiTestFlags)
bool FShinobiSprintCancelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const UShinobiMovementComponent* Move = CastChecked<UShinobiMovementComponent>(P->GetCharacterMovement());

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	W.Tick(0.3f);

	FGameplayTagContainer Tags(ShinobiTags::Ability_Sprint);
	P->GetAbilitySystemComponent()->CancelAbilities(&Tags);

	TestFalse(TEXT("Sprint cancelled"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestFalse(TEXT("Movement not sprinting after cancel"), Move->IsSprinting());

	const float S = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	W.Tick(0.8f);
	TestEqual(TEXT("Regen delayed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), S);
	W.Tick(0.5f);
	TestTrue(TEXT("Regen resumed after cancel"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()) > S);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiDodgeTest, "ShinobiCoop.Abilities.DodgeCostsStaminaAndGrantsIFrames", ShinobiTestFlags)
bool FShinobiDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const FVector Start = P->GetActorLocation();

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before dodge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	const float Before = ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute());
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("Dodge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestEqual(TEXT("Dodge cost 25"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), Before - 25.f);
	TestTrue(TEXT("Invulnerable"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("I-frames block damage"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	W.Tick(0.15f);
	TestTrue(TEXT("Still invulnerable mid-dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	W.Tick(0.25f);
	TestFalse(TEXT("Dodge finished"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("I-frames over"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));

	const FVector Displacement = (P->GetActorLocation() - Start) * FVector(1.f, 1.f, 0.f);
	TestTrue(FString::Printf(TEXT("Backstep ~600 on -X (got %s)"), *Displacement.ToString()), Displacement.X < -550.f && Displacement.X > -700.f);

	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Damage after dodge"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 450.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 20.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge blocked below 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("No stamina spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 20.f);

	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 25.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestTrue(TEXT("Dodge active at exactly 25 stamina"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("Stamina exactly spent"), ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiBlinkTest, "ShinobiCoop.Abilities.SecondDodgePressBlinks", ShinobiTestFlags)
bool FShinobiBlinkTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	const auto Stamina = [P] { return ShinobiTest::Get(P, UShinobiAttributeSet::GetStaminaAttribute()); };
	// What AShinobiPlayerCharacter::AbilityInputPressed does with the dodge key (it is private).
	const auto PressDodge = [P]
	{
		if (!P->GetAbilitySystemComponent()->TryActivateAbilityByClass(UGA_Blink::StaticClass()))
		{
			ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
		}
	};

	const float Before = Stamina();
	PressDodge();
	TestTrue(TEXT("First press dodges"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestFalse(TEXT("First press doesn't blink"), ShinobiTest::IsActive(P, UGA_Blink::StaticClass()));
	TestTrue(TEXT("Dodge opens the blink window"), ShinobiTest::HasTag(P, ShinobiTags::State_BlinkReady));
	TestEqual(TEXT("Dodge cost 25"), Stamina(), Before - 25.f);

	W.Tick(0.1f);
	const FVector Mid = P->GetActorLocation();
	PressDodge();
	TestTrue(TEXT("Second press blinks"), ShinobiTest::IsActive(P, UGA_Blink::StaticClass()));
	TestFalse(TEXT("Blink cancels the dodge"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));
	TestEqual(TEXT("Blink cost 35"), Stamina(), Before - 60.f);
	TestTrue(TEXT("Body hidden while blinking"), P->IsHidden());
	ShinobiTest::Damage(P, 50.f);
	TestEqual(TEXT("Blink untouchable"), ShinobiTest::Get(P, UShinobiAttributeSet::GetHealthAttribute()), 500.f);

	W.Tick(0.2f);
	TestFalse(TEXT("Blink over"), ShinobiTest::IsActive(P, UGA_Blink::StaticClass()));
	TestFalse(TEXT("Body back"), P->IsHidden());
	TestTrue(TEXT("Still untouchable on arrival"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	const FVector Blinked = (P->GetActorLocation() - Mid) * FVector(1.f, 1.f, 0.f);
	TestTrue(FString::Printf(TEXT("Blinked ~800 on -X (got %s)"), *Blinked.ToString()), Blinked.X < -650.f && Blinked.X > -900.f);

	W.Tick(0.3f);
	TestFalse(TEXT("Blink i-frames over"), ShinobiTest::HasTag(P, ShinobiTags::State_Invulnerable));
	PressDodge();
	TestFalse(TEXT("One blink per dodge"), ShinobiTest::IsActive(P, UGA_Blink::StaticClass()));
	TestTrue(TEXT("Third press dodges"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	W.Tick(1.8f);
	ShinobiTest::Set(P, UShinobiAttributeSet::GetStaminaAttribute(), 100.f);
	TestFalse(TEXT("Window closed"), ShinobiTest::HasTag(P, ShinobiTags::State_BlinkReady));
	PressDodge();
	TestFalse(TEXT("No blink after the window"), ShinobiTest::IsActive(P, UGA_Blink::StaticClass()));
	TestEqual(TEXT("A plain dodge after the window"), Stamina(), 75.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraChargeTest, "ShinobiCoop.Abilities.ChakraChargeRefillsWhileHeld", ShinobiTestFlags)
bool FShinobiChakraChargeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	// At full chakra (the default) charging still works: the pose and aura play, chakra just can't rise.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Can charge at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestTrue(TEXT("Charging tag at full chakra"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));

	// The pose: GA_ChakraCharge loads AM_ChakraCharge and loops its one ("Default") section while held.
	UAbilitySystemComponent* ASC = P->GetAbilitySystemComponent();
	UAnimMontage* ChargeMontage = ASC->GetCurrentMontage();
	if (TestNotNull(TEXT("Charging plays a montage"), ChargeMontage))
	{
		TestEqual(TEXT("Charging plays AM_ChakraCharge"), ChargeMontage->GetName(), FString(TEXT("AM_ChakraCharge")));
		TestEqual(TEXT("Montage set to loop its Default section"), ASC->CurrentMontageGetNextSectionName(), FName(TEXT("Default")));
	}

	W.Tick(0.3f);
	TestTrue(TEXT("Still charging at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestEqual(TEXT("Chakra stays at max"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);

	// AS_PowerUp is ~2.37 s; tick well past it (and its Default section's loop point) to prove the montage
	// keeps playing under CurrentMontageSetNextSectionName's loop instead of stopping on its own.
	UAnimInstance* AnimInstance = P->GetMesh()->GetAnimInstance();
	W.Tick(2.67f);
	if (ChargeMontage && TestNotNull(TEXT("Player has an anim instance"), AnimInstance))
	{
		TestTrue(TEXT("Montage still playing past clip length (looping)"), AnimInstance->Montage_IsPlaying(ChargeMontage));
	}
	TestTrue(TEXT("Ability still active after looping past clip length"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Release ends full-chakra charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	if (ChargeMontage && AnimInstance)
	{
		TestTrue(TEXT("Montage stopping or stopped after release"), AnimInstance->Montage_GetIsStopped(ChargeMontage));
	}

	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 0.f);

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging tag"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(1.1f);
	const float Charged = ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("~27.5 chakra after 1.1s charge (got %.2f)"), Charged), Charged >= 26.5f && Charged <= 28.f);

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge ended"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	W.Tick(0.5f);
	TestTrue(TEXT("Only passive regen after release"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()) <= Charged + 1.5f);

	// Chakra is well below max here, so charging interacts normally with sprint/dodge.
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprint active before charge"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge cancels sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	TestTrue(TEXT("Charge active"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestFalse(TEXT("Sprint blocked while charging"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));

	ShinobiTest::Press(P, EShinobiAbilityInputID::Dodge);
	TestFalse(TEXT("Dodge cancels charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag removed by dodge"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));

	// Let the dodge finish: GA_ChakraCharge blocks activation while State.Dodging is present, and
	// that tag stays granted for Dodge's full 0.25 s duration, not just until it cancels the charge.
	W.Tick(0.4f);
	TestFalse(TEXT("Dodge finished before next charge attempt"), ShinobiTest::IsActive(P, UGA_Dodge::StaticClass()));

	// Charging keeps going after chakra reaches the cap; only release (or dodge / a stagger hit) ends it.
	ShinobiTest::Set(P, UShinobiAttributeSet::GetChakraAttribute(), 95.f);
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charge started from 95"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	W.Tick(0.6f);
	TestTrue(TEXT("Charge continues at full chakra"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestEqual(TEXT("Chakra capped at max"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 100.f);
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Release ends charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeComboTest, "ShinobiCoop.Abilities.MeleeComboHitsHostilesOnly", ShinobiTestFlags)
bool FShinobiMeleeComboTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator); // faces +X
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	AShinobiPlayerCharacter* Ally = W.Spawn<AShinobiPlayerCharacter>(FVector(120.f, 40.f, 0.f));
	AShinobiCharacterBase* FarEnemy = W.Spawn<AShinobiCharacterBase>(FVector(600.f, 0.f, 0.f));
	ShinobiTest::Set(Attacker, UShinobiAttributeSet::GetChakraAttribute(), 50.f);
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 1: 20 damage"), ShinobiTest::Get(Enemy, Health), 480.f);
	TestEqual(TEXT("Ally untouched"), ShinobiTest::Get(Ally, Health), 500.f);
	TestEqual(TEXT("Far enemy untouched"), ShinobiTest::Get(FarEnemy, Health), 500.f);
	const float Chakra = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	TestTrue(FString::Printf(TEXT("+5 chakra on hit (got %.2f)"), Chakra), Chakra >= 55.f && Chakra <= 56.5f);

	// GA_ChakraCharge has no chakra gate, so this can only be blocked by GA_Melee's BlockAbilitiesWithTag.
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Charge blocked mid-combo"), ShinobiTest::IsActive(Attacker, UGA_ChakraCharge::StaticClass()));
	ShinobiTest::Release(Attacker, EShinobiAbilityInputID::ChakraCharge);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 2: 25 damage"), ShinobiTest::Get(Enemy, Health), 455.f);

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Hit 3: 40 damage"), ShinobiTest::Get(Enemy, Health), 415.f);
	// LaunchCharacter only sets PendingLaunchVelocity; CharacterMovementComponent applies it to Velocity
	// on its next tick, which lands after this one (the hit frame's timer fires from FTimerManager::Tick,
	// which runs after this substep's tick groups, i.e. after this substep's movement tick already ran).
	W.Tick(0.1f);
	TestTrue(FString::Printf(TEXT("Finisher knockback (got %.2f)"), Enemy->GetVelocity().X), Enemy->GetVelocity().X > 100.f);

	W.Tick(0.5f);
	TestFalse(TEXT("Combo ended"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMeleeWindowTest, "ShinobiCoop.Abilities.MeleeComboResetsAfterWindow", ShinobiTestFlags)
bool FShinobiMeleeWindowTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Attacker = W.Spawn<AShinobiPlayerCharacter>();
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(FVector(150.f, 0.f, 0.f));
	const FGameplayAttribute Health = UShinobiAttributeSet::GetHealthAttribute();

	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(1.f); // hit at 0.25, window closes at 0.75
	TestFalse(TEXT("Ended after window"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("New combo restarts at hit 1 (20 + 20)"), ShinobiTest::Get(Enemy, Health), 460.f);

	// Invulnerable target: the hit is blocked, so it doesn't count -- no chakra for the attacker.
	// Chakra must be well below max here, or a wrongly-counted +5 would just get clamped away and
	// this assertion would pass regardless of whether the landed-hit gating actually works.
	Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(ShinobiTags::State_Invulnerable);
	ShinobiTest::Set(Attacker, UShinobiAttributeSet::GetChakraAttribute(), 50.f);
	const float ChakraBeforeBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute());
	W.Tick(0.7f); // let the still-active combo's window close (it's at 0.3 of 0.75s; extra margin for timer jitter)
	TestFalse(TEXT("Second combo ended before third attempt"), ShinobiTest::IsActive(Attacker, UGA_Melee::StaticClass()));
	ShinobiTest::Press(Attacker, EShinobiAbilityInputID::Melee);
	W.Tick(0.3f);
	TestEqual(TEXT("Invulnerable target untouched"), ShinobiTest::Get(Enemy, Health), 460.f);
	const float ChakraGainedFromBlockedHit = ShinobiTest::Get(Attacker, UShinobiAttributeSet::GetChakraAttribute()) - ChakraBeforeBlockedHit;
	TestTrue(FString::Printf(TEXT("No chakra from a blocked hit, only passive regen (got +%.2f)"), ChakraGainedFromBlockedHit), ChakraGainedFromBlockedHit < 5.f);
	Enemy->GetAbilitySystemComponent()->RemoveLooseGameplayTag(ShinobiTags::State_Invulnerable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChakraAuraTest, "ShinobiCoop.Abilities.ChakraAuraFollowsCharging", ShinobiTestFlags)
bool FShinobiChakraAuraTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiChakraAuraComponent* Aura = P->GetChakraAura();
	if (!TestNotNull(TEXT("Player has a chakra aura"), Aura))
	{
		return false;
	}
	TestFalse(TEXT("Not charging at spawn"), Aura->IsCharging());
	TestEqual(TEXT("Hidden at spawn"), Aura->GetFade(), 0.f);

	UMaterialInstanceDynamic* AuraMaterial = Aura->GetAuraMaterial();
	if (!TestNotNull(TEXT("Aura material created"), AuraMaterial))
	{
		return false;
	}
	// Regression guard: an overlay material missing MATUSAGE_SkeletalMesh compiles and runs fine
	// under -nullrhi (this test's mode) but is silently nulled out by FSkeletalMeshSceneProxy in a
	// real PIE/-game/cooked render, so this can't be caught by rendering behaviour in this test --
	// only by asserting the flag is actually baked into the asset.
	TestTrue(TEXT("Aura material usable on skeletal meshes"), AuraMaterial->GetUsageByFlag(MATUSAGE_SkeletalMesh));
	TestEqual(TEXT("Aura material parent is the body aura material"), AuraMaterial->Parent ? AuraMaterial->Parent->GetName() : FString(), FString(TEXT("M_ChakraBodyAura")));

	// The overlay is applied once at BeginPlay and left in place (never toggled) to avoid re-triggering
	// PSO precache / proxy recreation on every charge start; visibility is driven purely by the Fade
	// material parameter (0 = the additive overlay contributes nothing).
	USkeletalMeshComponent* Body = P->GetBodyMesh(); // the visible mesh: the hero when in use
	TestTrue(TEXT("Overlay applied from spawn"), Body->GetOverlayMaterial() == AuraMaterial);
	TestEqual(TEXT("Fade param 0 at spawn"), AuraMaterial->K2_GetScalarParameterValue(TEXT("Fade")), 0.f);

	// Full chakra (the default): charging is visual-only but the aura still shows.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Aura follows the charging tag"), Aura->IsCharging());
	W.Tick(0.3f);
	TestEqual(TEXT("Aura faded in"), Aura->GetFade(), 1.f);
	TestTrue(TEXT("Overlay stays applied while charging"), Body->GetOverlayMaterial() == AuraMaterial);
	TestEqual(TEXT("Fade param 1 while charging"), AuraMaterial->K2_GetScalarParameterValue(TEXT("Fade")), 1.f);
	TestEqual(TEXT("Chakra flows around the body while charging (wisps, streams, ground swirl)"), Aura->NumVisibleFlowCards(), 19);

	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);
	TestFalse(TEXT("Aura stops with the charge"), Aura->IsCharging());
	W.Tick(0.3f);
	TestEqual(TEXT("Aura faded out"), Aura->GetFade(), 0.f);
	TestTrue(TEXT("Overlay stays applied after fade-out (Fade param hides it instead)"), Body->GetOverlayMaterial() == AuraMaterial);
	TestEqual(TEXT("Fade param 0 after fade-out"), AuraMaterial->K2_GetScalarParameterValue(TEXT("Fade")), 0.f);
	TestEqual(TEXT("Chakra flow hidden after fade-out"), Aura->NumVisibleFlowCards(), 0);
	TestFalse(TEXT("Tick disabled after fade-out"), Aura->IsComponentTickEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiChargeStaggerTest, "ShinobiCoop.Abilities.ChakraChargeEndsOnStaggerOrBrokenPose", ShinobiTestFlags)
bool FShinobiChargeStaggerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();

	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	ShinobiTest::Damage(P, 10.f);
	TestTrue(TEXT("A 10-damage hit doesn't end the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	ShinobiTest::Damage(P, 25.f);
	TestFalse(TEXT("A 25-damage hit ends the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	TestFalse(TEXT("Charging tag gone"), ShinobiTest::HasTag(P, ShinobiTags::State_ChakraCharging));
	ShinobiTest::Release(P, EShinobiAbilityInputID::ChakraCharge);

	// Another montage taking over the slot (a future hit reaction) must end the charge too.
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging again"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	UAnimMontage* Other = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Shinobi/Anims/AM_Dodge.AM_Dodge"));
	UAnimInstance* Anim = P->GetMesh()->GetAnimInstance();
	if (TestNotNull(TEXT("AM_Dodge loads"), Other) && TestNotNull(TEXT("Player has an anim instance"), Anim))
	{
		Anim->Montage_Play(Other);
		W.Tick(0.1f); // blending-out events are queued and dispatched during the anim tick
		TestFalse(TEXT("Charge ends when its pose is interrupted"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiUnpossessTest, "ShinobiCoop.Abilities.UnpossessEndsHeldAbilities", ShinobiTestFlags)
bool FShinobiUnpossessTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	AAIController* Controller = W.Spawn<AAIController>();

	Controller->Possess(P);
	ShinobiTest::Press(P, EShinobiAbilityInputID::ChakraCharge);
	TestTrue(TEXT("Charging while possessed"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess ends the charge"), ShinobiTest::IsActive(P, UGA_ChakraCharge::StaticClass()));

	Controller->Possess(P);
	ShinobiTest::Press(P, EShinobiAbilityInputID::Sprint);
	TestTrue(TEXT("Sprinting while possessed"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	Controller->UnPossess();
	TestFalse(TEXT("Unpossess ends the sprint"), ShinobiTest::IsActive(P, UGA_Sprint::StaticClass()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
