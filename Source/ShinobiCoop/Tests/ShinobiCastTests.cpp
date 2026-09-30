#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/World/ShinobiVillager.h"

// The generated NPC cast (Scripts/setup_hero.py -> /Game/Shinobi/Characters/<Name>): enemies wear a character from
// their type's pool, villagers one per Variant. These run against whatever has been imported: a character that isn't
// imported yet must leave its NPC as the tinted mannequin.

namespace
{
	/** Worn body is the named character, the driver is hidden, and the retargeted pose is full size. */
	void CheckWorn(FAutomationTestBase& Test, const FString& What, const FString& Name, const USkeletalMeshComponent* Body, const USkeletalMeshComponent* Driver)
	{
		const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
		Test.TestTrue(What + TEXT(" wears SK_") + Name, Mesh && Mesh->GetName() == TEXT("SK_") + Name);
		Test.TestFalse(What + TEXT(" hides its mannequin"), Driver->IsVisible());
		const float HeadHeight = Body->GetBoneLocation(TEXT("Head")).Z - Body->GetComponentLocation().Z;
		Test.TestTrue(FString::Printf(TEXT("%s head %.0f cm above the feet"), *What, HeadHeight), HeadHeight > 120.f && HeadHeight < 260.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiEnemiesWearPoolTest, "ShinobiCoop.Cast.EnemiesWearTheirPool", ShinobiTestFlags)
bool FShinobiEnemiesWearPoolTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	int32 Worn = 0;
	const TSubclassOf<AShinobiEnemyCharacter> Types[] = {
		AShinobiRogueNinja::StaticClass(), AShinobiKunaiThrower::StaticClass(), AShinobiRogueJonin::StaticClass() };
	for (int32 Round = 0; Round < 4; ++Round) // a few spawns each: the pick is random
	{
		for (const TSubclassOf<AShinobiEnemyCharacter>& Type : Types)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const AShinobiEnemyCharacter* Enemy = W.World->SpawnActor<AShinobiEnemyCharacter>(Type, FVector(Round * 300.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
			const FString What = Type->GetName();
			if (Enemy->GetBodyName().IsEmpty())
			{
				TestTrue(What + TEXT(" without an imported character is its visible mannequin"), Enemy->GetBodyMesh() == Enemy->GetMesh() && Enemy->GetMesh()->IsVisible());
				continue;
			}
			TestTrue(What + TEXT(" wears one of its pool"), Enemy->BodyPool.Contains(Enemy->GetBodyName()));
			CheckWorn(*this, What, Enemy->GetBodyName(), Enemy->GetBodyMesh(), Enemy->GetMesh());
			++Worn;
		}
	}
	AddInfo(FString::Printf(TEXT("%d of 12 enemy spawns wore an imported character"), Worn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiVillagersWearTheirCharacterTest, "ShinobiCoop.Cast.VillagersWearTheirCharacter", ShinobiTestFlags)
bool FShinobiVillagersWearTheirCharacterTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	int32 Worn = 0;
	for (int32 Variant = 0; Variant < UE_ARRAY_COUNT(AShinobiVillager::Bodies); ++Variant)
	{
		const FTransform Where(FVector(Variant * 300.f, 600.f, 0.f));
		AShinobiVillager* Villager = W.World->SpawnActorDeferred<AShinobiVillager>(AShinobiVillager::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Villager->Variant = Variant;
		Villager->FinishSpawning(Where);
		const FString Name = AShinobiVillager::Bodies[Variant];
		if (Villager->GetBodyMesh() == Villager->GetMesh())
		{
			TestTrue(Name + TEXT(" not imported: the tinted Quinn shows"), Villager->GetMesh()->IsVisible());
			continue;
		}
		CheckWorn(*this, TEXT("Villager ") + Name, Name, Villager->GetBodyMesh(), Villager->GetMesh());
		++Worn;
	}
	AddInfo(FString::Printf(TEXT("%d villagers wore an imported character"), Worn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiCastSidesTest, "ShinobiCoop.Cast.NoFaceOnTwoSides", ShinobiTestFlags)
bool FShinobiCastSidesTest::RunTest(const FString& Parameters)
{
	// A villager never looks like an enemy, enemy types don't share faces, and the NPC cast isn't pickable by players.
	TMap<FString, FString> Owner;
	auto Claim = [&](const FString& Name, const FString& Who)
	{
		if (const FString* Previous = Owner.Find(Name))
		{
			AddError(FString::Printf(TEXT("%s is both %s and %s"), *Name, **Previous, *Who));
		}
		Owner.Add(Name, Who);
	};
	for (const TCHAR* Name : AShinobiVillager::Bodies)
	{
		Claim(Name, TEXT("a villager"));
	}
	for (const FString& Name : GetDefault<AShinobiRogueNinja>()->BodyPool) { Claim(Name, TEXT("a rogue ninja")); }
	for (const FString& Name : GetDefault<AShinobiKunaiThrower>()->BodyPool) { Claim(Name, TEXT("a kunai thrower")); }
	for (const FString& Name : GetDefault<AShinobiRogueJonin>()->BodyPool) { Claim(Name, TEXT("the rogue jonin")); }
	for (const FShinobiRosterEntry& Entry : AShinobiPlayerCharacter::Roster)
	{
		Claim(Entry.Name, TEXT("a player character"));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
