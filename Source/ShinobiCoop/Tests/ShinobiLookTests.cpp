#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Shinobi/ShinobiLook.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/World/ShinobiVillager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiOutlineStencilTest, "ShinobiCoop.Look.CharactersWriteOutlineStencil", ShinobiTestFlags)
bool FShinobiOutlineStencilTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	AShinobiPlayerCharacter* Player = W.Spawn<AShinobiPlayerCharacter>(FVector::ZeroVector);
	AShinobiRogueNinja* Ninja = W.Spawn<AShinobiRogueNinja>(FVector(400.f, 0.f, 0.f));
	AShinobiVillager* Villager = W.Spawn<AShinobiVillager>(FVector(0.f, 400.f, 0.f));

	auto Outlined = [this](const UPrimitiveComponent* Mesh, uint8 Stencil, const TCHAR* Who)
	{
		TestTrue(FString::Printf(TEXT("%s writes custom depth"), Who), Mesh && Mesh->bRenderCustomDepth);
		TestEqual(FString::Printf(TEXT("%s stencil"), Who), Mesh ? Mesh->CustomDepthStencilValue : -1, int32(Stencil));
	};
	Outlined(Player->GetBodyMesh(), ShinobiLook::StencilFriendly, TEXT("Player's visible body"));
	Outlined(Ninja->GetBodyMesh(), ShinobiLook::StencilEnemy, TEXT("Enemy"));
	Outlined(Villager->GetBodyMesh(), ShinobiLook::StencilFriendly, TEXT("Villager's visible body"));
	if (Player->GetHeroMesh())
	{
		// The hidden Manny that drives the hero's animation must not draw an outline of its own.
		TestFalse(TEXT("Hidden animation driver writes no custom depth"), Player->GetMesh()->bRenderCustomDepth);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
