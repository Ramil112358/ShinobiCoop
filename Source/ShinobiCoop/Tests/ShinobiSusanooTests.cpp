#include "Tests/ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Shinobi/ShinobiCombat.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Abilities/GA_Susanoo.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Shinobi/VFX/ShinobiBurst.h"
#include "Shinobi/VFX/ShinobiPerfectSusanooComponent.h"

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
	P->ApplyJutsuPage(5);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestEqual(TEXT("Cost 40 paid up front"), ShinobiTest::Get(P, UShinobiAttributeSet::GetChakraAttribute()), 60.f);
	TestFalse(TEXT("Not armoured during the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	W.Tick(0.9f);
	TestTrue(TEXT("Armoured after the seals"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestTrue(TEXT("Cooldown running"), ShinobiTest::HasTag(P, ShinobiTags::Cooldown_Jutsu_Susanoo));
	TestFalse(TEXT("The jutsu itself is over"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestFalse(TEXT("It cannot be recast while it is up"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooPageTest, "ShinobiCoop.Susanoo.OnlyFiresFromPageFive", ShinobiTestFlags)
bool FShinobiSusanooPageTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::Hover(P);
	P->ApplyJutsuPage(4); // its old page: key 1 is Summon Pusho now, key 3 Misha
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu1);
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu3);
	TestFalse(TEXT("Not on page 4"), ShinobiTest::IsActive(P, UGA_Susanoo::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooFigureTest, "ShinobiCoop.Susanoo.FigureStandsWhileTheArmourIsUp", ShinobiTestFlags)
bool FShinobiSusanooFigureTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	UShinobiPerfectSusanooComponent* Armour = P->GetSusanoo();
	if (!TestNotNull(TEXT("Every character has the figure component"), Armour))
	{
		return false;
	}
	// Is any of the character's own body drawn (not the figure, its swords, its hilt flares or its sparks)?
	auto BodyShown = [P, Armour]()
	{
		TInlineComponentArray<UMeshComponent*> Meshes(P);
		for (const UMeshComponent* Mesh : Meshes)
		{
			const bool bFigure = Mesh == Armour->GetFigure() || (Armour->GetFigure() && Mesh->IsAttachedTo(Armour->GetFigure())) || Mesh->GetName().StartsWith(TEXT("SusanooSpark")) || Mesh->GetName().StartsWith(TEXT("SusanooFlare"));
			if (!bFigure && Mesh->IsVisible() && !Mesh->bHiddenInGame)
			{
				return true;
			}
		}
		return false;
	};
	TestFalse(TEXT("No figure before the cast"), Armour->IsShown());
	TestTrue(TEXT("The character is seen before the cast"), BodyShown());
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	TestTrue(TEXT("The figure stands while the armour is up"), Armour->IsShown());
	W.Tick(1.f);
	TestFalse(TEXT("and only the figure is seen: the character is hidden inside it"), BodyShown());
	W.Tick(20.f);
	TestFalse(TEXT("and goes when it ends"), Armour->IsShown());
	TestTrue(TEXT("the character is seen again"), BodyShown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooModelTest, "ShinobiCoop.Susanoo.FigureIsThePerfectSusanooStandingTwelveMetresTall", ShinobiTestFlags)
bool FShinobiSusanooModelTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass()); // the figure is built when the armour first goes up
	UShinobiPerfectSusanooComponent* Armour = P->GetSusanoo();
	USkeletalMeshComponent* Figure = Armour ? Armour->GetFigure() : nullptr;
	if (!TestNotNull(TEXT("The figure is built (run Scripts/setup_susanoo.py if not)"), Figure))
	{
		return false;
	}
	TestEqual(TEXT("the Perfect Susanoo"), Figure->GetSkeletalMeshAsset()->GetName(), FString(TEXT("SK_Susanoo")));
	W.Tick(1.f);
	const FBoxSphereBounds Bounds = Figure->CalcBounds(Figure->GetComponentTransform());
	const float Feet = P->GetActorLocation().Z - P->GetSimpleCollisionHalfHeight();
	TestTrue(FString::Printf(TEXT("about 12 m tall (%.0f)"), 2.f * Bounds.BoxExtent.Z), Bounds.BoxExtent.Z > 450.0 && Bounds.BoxExtent.Z < 700.0);
	TestTrue(FString::Printf(TEXT("standing on the ground (lowest point %.0f above the feet)"), Bounds.Origin.Z - Bounds.BoxExtent.Z - Feet), FMath::Abs(Bounds.Origin.Z - Bounds.BoxExtent.Z - Feet) < 150.0);

	// It faces where its owner faces: its toes are ahead of its heels.
	auto Bone = [Figure](const TCHAR* Suffix)
	{
		for (int32 Index = 0; Index < Figure->GetNumBones(); ++Index)
		{
			if (Figure->GetBoneName(Index).ToString().EndsWith(Suffix))
			{
				return Figure->GetBoneName(Index);
			}
		}
		return FName();
	};
	TestTrue(TEXT("its feet have the Mixamo bones"), !Bone(TEXT("LeftToeBase")).IsNone() && !Bone(TEXT("LeftFoot")).IsNone());
	const FVector Toes = Figure->GetSocketLocation(Bone(TEXT("LeftToeBase")));
	const FVector Heel = Figure->GetSocketLocation(Bone(TEXT("LeftFoot")));
	TestTrue(TEXT("facing the owner's forward"), FVector::DotProduct((Toes - Heel).GetSafeNormal2D(), P->GetActorForwardVector()) > 0.5);

	TestEqual(TEXT("a sword in each hand"), Armour->GetSwords().Num(), 2);
	for (const UStaticMeshComponent* Sword : Armour->GetSwords())
	{
		TestTrue(TEXT("held by a hand bone"), Sword->GetAttachSocketName().ToString().EndsWith(TEXT("Hand")));
		const FBoxSphereBounds Blade = Sword->CalcBounds(Sword->GetComponentTransform());
		TestTrue(FString::Printf(TEXT("a blade about as long as the figure is tall (%.0f)"), Blade.SphereRadius * 2.f), Blade.SphereRadius > 400.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooSlashTest, "ShinobiCoop.Susanoo.SwingsItsSwordsWithTheOwnersCombo", ShinobiTestFlags)
bool FShinobiSusanooSlashTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	UShinobiPerfectSusanooComponent* Armour = P->GetSusanoo();
	W.Tick(0.5f);
	UShinobiSusanooAnimInstance* Anim = Armour->GetFigureAnim();
	if (!TestNotNull(TEXT("The figure animates"), Anim) || !TestTrue(TEXT("with slashes to swing"), Armour->GetSlashes().Num() >= 3))
	{
		return false;
	}
	TestEqual(TEXT("It holds its stance"), Anim->GetCurrentClip()->GetName(), FString(TEXT("A_Susanoo_Idle")));
	for (const UAnimSequence* Clip : Armour->GetSlashes())
	{
		// Slash1's right hand strikes about 0.8 s into its 1.9 s (Blender): the lead-in and the recovery are cut.
		const FVector2f Window = Anim->GetSlashWindow(Clip);
		AddInfo(FString::Printf(TEXT("%s window %.2f-%.2f of %.2f s"), *Clip->GetName(), Window.X, Window.Y, Clip->GetPlayLength()));
		TestTrue(FString::Printf(TEXT("%s swings around its strike (%.2f-%.2f of %.2f s)"), *Clip->GetName(), Window.X, Window.Y, Clip->GetPlayLength()),
			Window.X > 0.1f && Window.Y < Clip->GetPlayLength() - 0.1f && Window.Y - Window.X > 0.5f);
	}

	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee); // the owner's first swing
	W.Tick(0.1f);
	TestEqual(TEXT("and swings with the right blade when its owner swings"), Anim->GetSlashClip(), Armour->GetSlashes()[0].Get());
	TestEqual(TEXT("with its arms only: the legs keep the stance"), Anim->GetCurrentClip()->GetName(), FString(TEXT("A_Susanoo_Idle")));
	W.Tick(4.f);
	TestNull(TEXT("then lets go of the swing"), Anim->GetSlashClip());

	// A fresh combo (the window long closed) still swings the next slash, not the first one again.
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	W.Tick(0.1f);
	TestEqual(TEXT("A new combo swings the next slash"), Anim->GetSlashClip(), Armour->GetSlashes()[1].Get());
	// Mashed: swings that come while one slash swings wait for it instead of cutting it off.
	Armour->Slash();
	Armour->Slash(); // no room for this one
	W.Tick(0.05f);
	TestEqual(TEXT("A swing mid-slash lets the current one play on"), Anim->GetSlashClip(), Armour->GetSlashes()[1].Get());
	W.Tick(0.9f);
	TestEqual(TEXT("then the next slash follows it"), Anim->GetSlashClip(), Armour->GetSlashes()[2].Get());
	W.Tick(4.f);
	TestNull(TEXT("one swing waits at most"), Anim->GetSlashClip());
	Armour->Slash();
	W.Tick(0.05f);
	TestEqual(TEXT("and a dropped swing does not skip a slash"), Anim->GetSlashClip(), Armour->GetSlashes()[3 % Armour->GetSlashes().Num()].Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooWalkTest, "ShinobiCoop.Susanoo.WalksAndRunsWithItsWearer", ShinobiTestFlags)
bool FShinobiSusanooWalkTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	W.Tick(0.5f);
	UShinobiSusanooAnimInstance* Anim = P->GetSusanoo()->GetFigureAnim();
	if (!TestNotNull(TEXT("The figure animates"), Anim))
	{
		return false;
	}
	TestEqual(TEXT("It stands while its wearer stands"), Anim->GetCurrentClip()->GetName(), FString(TEXT("A_Susanoo_Idle")));
	P->GetCharacterMovement()->Velocity = FVector(400.f, 0.f, 0.f);
	W.Tick(0.02f);
	TestEqual(TEXT("walks while they walk (run Scripts/setup_susanoo.py with susanoo_walk.fbx if not)"), Anim->GetCurrentClip()->GetName(), FString(TEXT("A_Susanoo_Walk")));
	P->GetCharacterMovement()->Velocity = FVector(1200.f, 0.f, 0.f);
	W.Tick(0.02f);
	TestEqual(TEXT("and runs when they go fast"), Anim->GetCurrentClip()->GetName(), FString(TEXT("A_Susanoo_Run")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooAssetsTest, "ShinobiCoop.Susanoo.HasAnimationsAndAnIcon", ShinobiTestFlags)
bool FShinobiSusanooAssetsTest::RunTest(const FString& Parameters)
{
	// (run Scripts/setup_jutsu_anims.py and Scripts/setup_hud.py if an asset is missing)
	const UGA_Susanoo* Susanoo = GetDefault<UGA_Susanoo>();
	TestNotNull(TEXT("seal montage"), Susanoo->SealMontage.LoadSynchronous());
	TestNotNull(TEXT("release montage"), Susanoo->ReleaseMontage.LoadSynchronous());
	TestNotNull(TEXT("icon"), Susanoo->Icon.LoadSynchronous());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooLazyTest, "ShinobiCoop.Susanoo.FigureIsOnlyBuiltWhenTheArmourFirstGoesUp", ShinobiTestFlags)
bool FShinobiSusanooLazyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiCharacterBase* Enemy = W.Spawn<AShinobiCharacterBase>(); // never armoured: it must not pay for a 12 m figure
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	TestNull(TEXT("No figure on an enemy"), Enemy->GetSusanoo()->GetFigure());
	TestNull(TEXT("and none on a player who has not cast it"), P->GetSusanoo()->GetFigure());
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	TestNotNull(TEXT("The figure exists once the armour is up"), P->GetSusanoo()->GetFigure());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSusanooDownedTest, "ShinobiCoop.Susanoo.IsSuspendedWhileDownedAndComesBackOnRevive", ShinobiTestFlags)
bool FShinobiSusanooDownedTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>();
	ShinobiTest::ApplyEffect(P, UGE_Susanoo::StaticClass());
	W.Tick(1.f);
	TestTrue(TEXT("Armoured"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));

	ShinobiTest::ApplyEffect(P, UGE_Downed::StaticClass());
	TestFalse(TEXT("A downed player is not armoured"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestFalse(TEXT("and no figure stands over the body"), P->GetSusanoo()->IsShown());

	P->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_Downed));
	TestTrue(TEXT("Revived: the rest of the 20 s is theirs again"), ShinobiTest::HasTag(P, ShinobiTags::State_Susanoo));
	TestTrue(TEXT("and the figure is back"), P->GetSusanoo()->IsShown());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
