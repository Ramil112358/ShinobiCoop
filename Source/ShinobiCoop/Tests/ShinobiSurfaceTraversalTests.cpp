#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AIController.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementReplication.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"

namespace
{
	/** The wall's near face is the plane X = WallX; the ground's top is Z = 0. */
	constexpr float WallX = 500.f;

	AStaticMeshActor* Block(FShinobiTestWorld& W, const FVector& Center, const FVector& Size)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Actor = W.World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->SetActorScale3D(Size / 100.f); // the engine cube is 100 uu
		Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		return Actor;
	}

	/** Ground plus a 2 m thick wall, Height tall, spanning Y from -Width / 2 to Width / 2. */
	void Arena(FShinobiTestWorld& W, float Height = 2000.f, float Width = 2000.f)
	{
		Block(W, FVector(0.f, 0.f, -50.f), FVector(40000.f, 40000.f, 100.f));
		Block(W, FVector(WallX + 100.f, 0.f, Height / 2.f), FVector(200.f, Width, Height));
	}

	AShinobiPlayerCharacter* PlayerAt(FShinobiTestWorld& W, const FVector& Location)
	{
		AShinobiPlayerCharacter* P = W.Spawn<AShinobiPlayerCharacter>(Location);
		W.Tick(0.2f); // settle onto the ground
		return P;
	}

	UShinobiMovementComponent* Moves(const AShinobiPlayerCharacter* P)
	{
		return Cast<UShinobiMovementComponent>(P->GetCharacterMovement());
	}

	/** Holds a world-space movement intent for Seconds. Input is consumed every frame, so it is re-added each step; on a
	 * wall it goes through WallInputDirection, as AShinobiPlayerCharacter::Move does. */
	void Drive(FShinobiTestWorld& W, AShinobiPlayerCharacter* P, const FVector& Intent, float Seconds)
	{
		for (; Seconds > KINDA_SMALL_NUMBER; Seconds -= 0.05f)
		{
			const UShinobiMovementComponent* M = Moves(P);
			P->AddMovementInput(M->IsWallTraversing() ? M->WallInputDirection(Intent) : Intent);
			W.Tick(0.05f);
		}
	}

	/** Runs at the wall (+X) and reports whether the player ended up on it. */
	bool RunOntoWall(FAutomationTestBase& Test, FShinobiTestWorld& W, AShinobiPlayerCharacter* P)
	{
		Drive(W, P, FVector::ForwardVector, 1.5f);
		return Test.TestTrue(TEXT("on the wall"), Moves(P)->IsWallTraversing());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallEntryTest, "ShinobiCoop.Movement.SurfaceTraversal.RunningIntoAWallClimbsIt", ShinobiTestFlags)
bool FShinobiWallEntryTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	const UShinobiMovementComponent* M = Moves(P);
	TestTrue(TEXT("gravity points into the wall"), M->GetGravityDirection().Equals(FVector(1.f, 0.f, 0.f), 0.01f));
	TestTrue(TEXT("feet against the wall"), P->GetActorUpVector().Equals(FVector(-1.f, 0.f, 0.f), 0.01f));
	TestEqual(TEXT("walking on it"), M->MovementMode.GetValue(), MOVE_Walking);

	const float StartZ = P->GetActorLocation().Z;
	Drive(W, P, FVector::ForwardVector, 1.f); // keep pushing into the wall = climb
	TestTrue(FString::Printf(TEXT("climbs (%.0f)"), P->GetActorLocation().Z - StartZ), P->GetActorLocation().Z - StartZ > 300.f);
	TestTrue(TEXT("never inside the wall"), P->GetActorLocation().X < WallX);

	const float StartY = P->GetActorLocation().Y;
	Drive(W, P, FVector::RightVector, 0.5f);
	TestTrue(FString::Printf(TEXT("runs sideways (%.0f)"), P->GetActorLocation().Y - StartY), P->GetActorLocation().Y - StartY > 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallNoAccidentTest, "ShinobiCoop.Movement.SurfaceTraversal.OnlyRunningIntoATallFaceAttaches", ShinobiTestFlags)
bool FShinobiWallNoAccidentTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 45.f, 0.f, 100.f));
	W.Tick(1.f);
	TestFalse(TEXT("standing beside a wall does not attach"), Moves(P)->IsWallTraversing());
	Drive(W, P, FVector::RightVector, 1.f);
	TestFalse(TEXT("running along a wall does not attach"), Moves(P)->IsWallTraversing());

	// A 60 cm block (a bench, a car side): too low to step onto, too low to climb.
	Block(W, FVector(-1000.f, 0.f, 30.f), FVector(200.f, 400.f, 60.f));
	AShinobiPlayerCharacter* Q = PlayerAt(W, FVector(-1300.f, 0.f, 100.f));
	Drive(W, Q, FVector::ForwardVector, 1.5f);
	TestFalse(TEXT("a low face does not attach"), Moves(Q)->IsWallTraversing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallNoRoomTest, "ShinobiCoop.Movement.SurfaceTraversal.NoRoomToTurnKeepsTheCharacterUpright", ShinobiTestFlags)
bool FShinobiWallNoRoomTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	// A post the upright capsule clears but the wall-turned one (its long axis out from the wall) would cut through.
	Block(W, FVector(WallX - 145.f, 0.f, 150.f), FVector(50.f, 60.f, 300.f));
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 60.f, 0.f, 100.f));
	Drive(W, P, FVector::ForwardVector, 1.f);
	TestFalse(TEXT("does not attach"), Moves(P)->IsWallTraversing());
	TestTrue(TEXT("stays upright"), P->GetActorUpVector().Z > 0.99f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallKickOffTest, "ShinobiCoop.Movement.SurfaceTraversal.JumpKicksOffTheWall", ShinobiTestFlags)
bool FShinobiWallKickOffTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	P->Jump();
	W.Tick(0.1f);
	P->StopJumping();
	const UShinobiMovementComponent* M = Moves(P);
	TestFalse(TEXT("off the wall"), M->IsWallTraversing());
	TestTrue(TEXT("world gravity again"), M->GetGravityDirection().Equals(FVector(0.f, 0.f, -1.f), 0.01f));
	TestTrue(TEXT("upright"), P->GetActorUpVector().Z > 0.99f);
	TestTrue(FString::Printf(TEXT("launched away (%.0f)"), M->Velocity.X), M->Velocity.X < -300.f);
	TestTrue(FString::Printf(TEXT("and up (%.0f)"), M->Velocity.Z), M->Velocity.Z > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallForcedDetachTest, "ShinobiCoop.Movement.SurfaceTraversal.KnockbackAndMountingDetach", ShinobiTestFlags)
bool FShinobiWallForcedDetachTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	P->LaunchCharacter(FVector(-500.f, 0.f, 300.f), true, true);
	TestFalse(TEXT("knockback detaches at once"), Moves(P)->IsWallTraversing());
	TestTrue(TEXT("with world gravity"), Moves(P)->GetGravityDirection().Equals(FVector(0.f, 0.f, -1.f), 0.01f));

	W.Tick(1.5f); // land
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	ShinobiTest::ApplyEffect(P, UGE_Mounted::StaticClass());
	W.Tick(0.2f);
	TestFalse(TEXT("mounting detaches"), Moves(P)->IsWallTraversing());
	TestEqual(TEXT("and flies"), Moves(P)->MovementMode.GetValue(), MOVE_Flying);
	return true;
}

namespace
{
	/** Drives Intent until the player leaves the wall (or MaxSeconds pass). */
	void DriveUntilOff(FShinobiTestWorld& W, AShinobiPlayerCharacter* P, const FVector& Intent, float MaxSeconds)
	{
		for (float T = 0.f; T < MaxSeconds && Moves(P)->IsWallTraversing(); T += 0.05f)
		{
			Drive(W, P, Intent, 0.05f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallVaultTest, "ShinobiCoop.Movement.SurfaceTraversal.ClimbingPastTheTopVaultsOntoTheRoof", ShinobiTestFlags)
bool FShinobiWallVaultTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W, 600.f); // a 6 m building with a flat 2 m deep roof
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	DriveUntilOff(W, P, FVector::ForwardVector, 3.f);
	W.Tick(0.3f);
	const FVector Location = P->GetActorLocation();
	TestFalse(TEXT("off the wall"), Moves(P)->IsWallTraversing());
	TestEqual(TEXT("walking"), Moves(P)->MovementMode.GetValue(), MOVE_Walking);
	TestTrue(TEXT("upright"), P->GetActorUpVector().Z > 0.99f);
	TestTrue(FString::Printf(TEXT("on the roof (%.0f, %.0f)"), Location.X, Location.Z), Location.X > WallX && Location.Z > 600.f + 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallWalkDownTest, "ShinobiCoop.Movement.SurfaceTraversal.WalkingDownStandsUpOnTheGround", ShinobiTestFlags)
bool FShinobiWallWalkDownTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	Drive(W, P, FVector::ForwardVector, 0.6f); // up a little
	DriveUntilOff(W, P, FVector::BackwardVector, 3.f); // then back down
	W.Tick(0.5f);
	const FVector Location = P->GetActorLocation();
	TestFalse(TEXT("off the wall"), Moves(P)->IsWallTraversing());
	TestEqual(TEXT("walking"), Moves(P)->MovementMode.GetValue(), MOVE_Walking);
	TestTrue(TEXT("upright"), P->GetActorUpVector().Z > 0.99f);
	TestTrue(FString::Printf(TEXT("standing on the ground (%.0f)"), Location.Z), Location.Z > 80.f && Location.Z < 120.f);
	TestTrue(TEXT("outside the wall"), Location.X < WallX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallCornerTest, "ShinobiCoop.Movement.SurfaceTraversal.RunningRoundAnOuterCornerWrapsOntoTheSide", ShinobiTestFlags)
bool FShinobiWallCornerTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W, 2000.f, 1000.f); // the wall ends at Y = 500; its side face looks along +Y
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 300.f, 100.f));
	AAIController* C = W.World->SpawnActor<AAIController>(); // stands in for the player's controller: its control rotation is the camera
	C->bSetControlRotationFromPawnOrientation = false;
	C->Possess(P);
	C->SetControlRotation(FRotator::ZeroRotator);
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	Drive(W, P, FVector::ForwardVector, 0.5f); // off the ground
	for (float T = 0.f; T < 3.f && !Moves(P)->GetGravityDirection().Equals(FVector(0.f, -1.f, 0.f), 0.05f); T += 0.05f)
	{
		Drive(W, P, FVector::RightVector, 0.05f);
	}
	TestTrue(TEXT("still on a wall"), Moves(P)->IsWallTraversing());
	TestTrue(TEXT("now on the side face"), Moves(P)->GetGravityDirection().Equals(FVector(0.f, -1.f, 0.f), 0.05f));
	TestTrue(TEXT("beside it, not inside"), P->GetActorLocation().Y > 500.f);
	// Round the corner the run carries on along the new face, and the camera turns with the wall, so the held key keeps
	// running sideways instead of turning into "down".
	TestTrue(FString::Printf(TEXT("keeps its speed round the corner (%.0f)"), Moves(P)->Velocity.X), Moves(P)->Velocity.X > 200.f);
	TestTrue(FString::Printf(TEXT("the camera turned with the wall (%.0f)"), C->GetControlRotation().Yaw), FMath::IsNearlyEqual(FRotator::NormalizeAxis(C->GetControlRotation().Yaw), -90.f, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiFaceDirectionTest, "ShinobiCoop.Movement.SurfaceTraversal.FacingKeepsTheFeetOnTheWall", ShinobiTestFlags)
bool FShinobiFaceDirectionTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));

	UShinobiMovementComponent::FaceDirection(P, FVector(1.f, 1.f, -5.f));
	TestTrue(TEXT("upright: yaw only"), P->GetActorRotation().Equals(FRotator(0.f, 45.f, 0.f), 0.5f));

	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	UShinobiMovementComponent::FaceDirection(P, FVector(0.f, 1.f, 0.f));
	TestTrue(TEXT("on the wall the feet stay on it"), P->GetActorUpVector().Equals(FVector(-1.f, 0.f, 0.f), 0.01f));
	TestTrue(TEXT("and it faces the asked way along it"), P->GetActorForwardVector().Equals(FVector(0.f, 1.f, 0.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWaterTest, "ShinobiCoop.Movement.SurfaceTraversal.WaterTaggedSurfacesAreWalkedOn", ShinobiTestFlags)
bool FShinobiWaterTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	// Water as the map builders make it: tagged, query-only, blocking Pawn and nothing else.
	AStaticMeshActor* Water = Block(W, FVector(0.f, 0.f, -50.f), FVector(4000.f, 4000.f, 100.f));
	Water->Tags.Add(UShinobiMovementComponent::WaterSurfaceTag);
	UStaticMeshComponent* Mesh = Water->GetStaticMeshComponent();
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Block(W, FVector(5000.f, 0.f, -50.f), FVector(4000.f, 4000.f, 100.f)); // ordinary ground beside it

	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(0.f, 0.f, 100.f));
	W.Tick(0.5f);
	TestEqual(TEXT("walking"), Moves(P)->MovementMode.GetValue(), MOVE_Walking);
	TestTrue(FString::Printf(TEXT("on the surface (%.0f)"), P->GetActorLocation().Z), P->GetActorLocation().Z > 80.f);
	TestTrue(TEXT("on water"), Moves(P)->IsWaterTraversing());
	Drive(W, P, FVector::ForwardVector, 0.5f);
	TestTrue(TEXT("still on water while moving"), Moves(P)->IsWaterTraversing());

	AShinobiPlayerCharacter* Q = PlayerAt(W, FVector(5000.f, 0.f, 100.f));
	TestFalse(TEXT("ordinary ground is not water"), Moves(Q)->IsWaterTraversing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiSurfaceFxTest, "ShinobiCoop.Movement.SurfaceTraversal.FeetGlowOnlyWhileTraversing", ShinobiTestFlags)
bool FShinobiSurfaceFxTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	const UShinobiSurfaceFxComponent* Fx = P->FindComponentByClass<UShinobiSurfaceFxComponent>();
	if (!TestNotNull(TEXT("players have the foot glow"), Fx))
	{
		return false;
	}
	W.Tick(0.3f);
	TestEqual(TEXT("hidden on ordinary ground"), Fx->GetFade(), 0.f);
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	W.Tick(0.3f);
	TestTrue(FString::Printf(TEXT("shown on the wall (%.2f)"), Fx->GetFade()), Fx->GetFade() > 0.99f);
	P->Jump();
	W.Tick(0.05f);
	P->StopJumping();
	W.Tick(0.3f);
	TestEqual(TEXT("fades out after kicking off"), Fx->GetFade(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallCorrectionTest, "ShinobiCoop.Movement.SurfaceTraversal.ServerCorrectionsCarryGravityAndRotation", ShinobiTestFlags)
bool FShinobiWallCorrectionTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	UShinobiMovementComponent* M = Moves(P);
	TestTrue(TEXT("the server sends rotation in corrections while on a wall"), M->ShouldCorrectRotation());

	// The server knocked this client off the wall (a hit, a teleport): its correction says "world gravity".
	FCharacterMoveResponseDataContainer Response;
	Response.ClientAdjustment.bAckGoodMove = false;
	Response.ClientAdjustment.TimeStamp = 1.f;
	Response.ClientAdjustment.NewLoc = P->GetActorLocation();
	Response.ClientAdjustment.GravityDirection = FVector(0.f, 0.f, -1.f);
	M->ClientHandleMoveResponse(Response);
	TestFalse(TEXT("the client takes the server's gravity"), M->IsWallTraversing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallProxyTest, "ShinobiCoop.Movement.SurfaceTraversal.OtherPlayersCopiesFollowOnlyReplicatedGravity", ShinobiTestFlags)
bool FShinobiWallProxyTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	// Another machine's copy: its extrapolation can briefly lose the floor. It must not decide wall state itself.
	P->SetRole(ROLE_SimulatedProxy);
	UShinobiMovementComponent* M = Moves(P);
	M->SetMovementMode(MOVE_Falling);
	M->UpdateCharacterStateBeforeMovement(0.05f);
	TestTrue(TEXT("still on the wall until the server says otherwise"), M->IsWallTraversing());
	P->SetRole(ROLE_Authority);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallPawnTest, "ShinobiCoop.Movement.SurfaceTraversal.ACharacterBesideTheWallDoesNotBlockAttaching", ShinobiTestFlags)
bool FShinobiWallPawnTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	// Where the wall-turned capsule will lie. Other characters move out of the way, and their positions differ between
	// machines, so they must not decide whether a player attaches.
	W.Spawn<AShinobiCharacterBase>(FVector(WallX - 150.f, 0.f, 100.f));
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 60.f, 0.f, 100.f));
	Drive(W, P, FVector::ForwardVector, 1.f);
	TestTrue(TEXT("attaches"), Moves(P)->IsWallTraversing());
	return true;
}

namespace
{
	/** On the wall a little above the ground, standing still and facing down it. */
	bool HangFacingDown(FAutomationTestBase& Test, FShinobiTestWorld& W, AShinobiPlayerCharacter* P)
	{
		if (!RunOntoWall(Test, W, P))
		{
			return false;
		}
		Drive(W, P, FVector::ForwardVector, 0.25f);
		W.Tick(0.3f);
		UShinobiMovementComponent::FaceDirection(P, FVector::DownVector);
		return Test.TestTrue(TEXT("still on the wall"), Moves(P)->IsWallTraversing());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallMeleeTest, "ShinobiCoop.Movement.SurfaceTraversal.MeleeOnAWallHitsAnEnemyBelow", ShinobiTestFlags)
bool FShinobiWallMeleeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!HangFacingDown(*this, W, P))
	{
		return false;
	}
	AShinobiCharacterBase* E = W.Spawn<AShinobiCharacterBase>(FVector(WallX - 60.f, 0.f, P->GetActorLocation().Z - 150.f));
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Melee);
	W.Tick(0.6f);
	const float Health = ShinobiTest::Get(E, UShinobiAttributeSet::GetHealthAttribute());
	TestTrue(FString::Printf(TEXT("the enemy below is hit (%.0f)"), Health), Health < 500.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallDodgeTest, "ShinobiCoop.Movement.SurfaceTraversal.DodgeOnAWallMovesAlongIt", ShinobiTestFlags)
bool FShinobiWallDodgeTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	Drive(W, P, FVector::ForwardVector, 0.3f); // climbing: the dodge goes the way the input points, up the wall
	const float StartZ = P->GetActorLocation().Z;
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Dodge);
	W.Tick(0.3f);
	TestTrue(FString::Printf(TEXT("dodged up the wall (%.0f)"), P->GetActorLocation().Z - StartZ), P->GetActorLocation().Z - StartZ > 250.f);
	TestTrue(TEXT("and stayed on it"), Moves(P)->IsWallTraversing());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiWallDashVaultTest, "ShinobiCoop.Movement.SurfaceTraversal.ADashThatVaultsOntoTheRoofEndsUpright", ShinobiTestFlags)
bool FShinobiWallDashVaultTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld W;
	Arena(W, 600.f);
	AShinobiPlayerCharacter* P = PlayerAt(W, FVector(WallX - 300.f, 0.f, 100.f));
	if (!RunOntoWall(*this, W, P))
	{
		return false;
	}
	ShinobiTest::Tap(P, EShinobiAbilityInputID::Jutsu2); // Chidori: seals on the wall, then a dash up it and over the top
	W.Tick(1.5f);
	TestFalse(TEXT("on the roof, off the wall"), Moves(P)->IsWallTraversing());
	TestTrue(FString::Printf(TEXT("upright after the dash (%.2f)"), P->GetActorUpVector().Z), P->GetActorUpVector().Z > 0.99f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
