# Surface Traversal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Player shinobi run up and along building walls, fight on them, walk on water, and show a thin chakra-blue glow around each foot while doing so. This is the *Shinobi Striker* style of wall and water movement.

**Architecture:** Wall running uses the engine's custom gravity (`UCharacterMovementComponent::SetGravityDirection`). On a wall, gravity points into the wall and the character uses ordinary `MOVE_Walking` on it. The engine handles prediction and corrections, and replicates gravity to every machine. `UShinobiMovementComponent` owns entry, exits, the top-edge vault and corner wraps, all run from the movement update so client and server agree. Water is ordinary floor: the map builders give the water meshes Pawn-only collision and a tag. A player-only cosmetic component draws the foot glow from movement state.

**Tech Stack:** UE 5.8 C++ (CharacterMovement, GAS), automation tests (`FShinobiTestWorld`), Unreal Python map builders, the `ShinobiVfx` glow material.

**Spec:** `docs/superpowers/specs/2026-10-05-surface-traversal-design.md`

## Global Constraints

- Players only (`AShinobiPlayerCharacter`). Enemies, summons and other AI never attach to walls.
- Wall entry is automatic: movement input pushes into a blocking `WorldStatic` face within 25° of vertical (`|ImpactNormal.Z| <= 0.4226`). There is no new button.
- The face must also reach head height, about 1.8 m above the feet.
- Actors tagged `ShinobiNoSurfaceTraversal` are never climbed. Water actors carry `ShinobiWaterSurface`.
- Collision is never bypassed. Every capsule rotation or relocation is overlap-tested first, and the character does not attach when it would not fit.
- Exits:
  - Jump kicks off the wall (`KickOffSpeed = 700`, `KickOffUp = 450`).
  - Climbing past the top edge vaults onto the roof.
  - Walking down onto walkable ground stands the character up.
  - An outer corner wraps onto the next face, or the character falls if there is none.
  - Mount, downed, dead, phased and knockback launch all detach.
- The camera stays world-upright. On a wall, input pointing into the wall moves up, out of it moves down, and along it moves sideways.
- Mounted flight takes precedence over traversal. Ground movement, sprint, chill and speed boost are unchanged.
- The foot glow is cosmetic only. It uses `ShinobiVfx::MakeGlow` with `ShinobiVfx::Colors::Chakra`, creates nothing on a dedicated server and costs no chakra.
- Water: query-only collision that blocks `Pawn` and ignores every other channel. Materials, transforms and shadows stay unchanged. London `City:X` river-edge walls are removed, and outer `EdgeWall`s keep blocking.
- Testing (user rule): run only `ShinobiCoop.Movement.SurfaceTraversal` while working. Do not run the full suite.
- Work on branch `feature/surface-traversal`. `main` has unrelated uncommitted changes (Kamui, minimap, content packs): stage only the files each task names, never `git add -A`.
- Close the editor before `Scripts/build.ps1`, `Scripts/run_tests.ps1` or `Scripts/run_python.ps1`.

## Review Focus

1. **Low obstacles:** kerbs, benches and car sides have vertical faces, and walking into them must not attach. Task 1 tests a 60 cm block.
2. **No room to turn onto the wall:** when an overhang or post would intersect the wall-rotated capsule, the character stays upright and does not clip into geometry. Task 1 tests a post beside the wall.
3. **Running sideways past an outer building corner:** the character must wrap onto the side face, not float off into the air. Task 2 tests the corner wrap.
4. **Walking down to the street:** the character must stand up on the ground, not sink into it or stay sideways. Task 2 tests the descent.
5. **Knockback or mounting while on a wall:** the character must detach at once with world gravity. Task 1 tests launch and mount.

---

### Task 0: Branch

- [ ] **Step 1: Create the feature branch**

```bash
git checkout -b feature/surface-traversal
```

Uncommitted changes from `main` move with you. Leave them unstaged.

---

### Task 1: Wall entry, wall input, kick-off and forced detach

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp` (`Move`, new `LaunchCharacter` override)
- Create: `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`

**Interfaces:**
- Produces, on `UShinobiMovementComponent`:
  - `bool IsWallTraversing() const`
  - `bool CanSurfaceTraverse() const`
  - `void DetachFromWall(const FVector& Facing = FVector::ZeroVector)`
  - `FVector WallInputDirection(const FVector& Intent) const`
  - `static const FName WaterSurfaceTag`, `static const FName NoTraversalTag`
  - `static constexpr float WallMaxNormalZ, WallReach, KickOffSpeed, KickOffUp`
- Produces, for Task 2 (private): `bool SweepCapsule(...)`, `bool Fits(...)`, `bool IsClimbable(...)`, `bool AttachToWall(const FHitResult& Hit, const FVector& Anchor)`
- Consumes: `ShinobiTags::State_Mounted/Downed/Dead/Phased`, `UGE_Mounted`

- [ ] **Step 1: Write the failing tests**

Create `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`:

```cpp
#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/ShinobiPlayerCharacter.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
```

- [ ] **Step 2: Build and confirm the tests fail to compile**

Run: `.\Scripts\build.ps1`
Expected: FAIL, because `IsWallTraversing` and `WallInputDirection` are not members of `UShinobiMovementComponent`.

- [ ] **Step 3: Add the traversal API to the movement component header**

In `ShinobiMovementComponent.h`, update the class comment and add the members.

Add to the `public:` section, after `ShouldCorrectRotation`:

```cpp
	// ---- Wall and water traversal (players only; docs/superpowers/specs/2026-10-05-surface-traversal-design.md) ----

	/** On a wall: gravity points into it and the character walks on it. The engine replicates gravity to every machine. */
	bool IsWallTraversing() const { return HasCustomGravity(); }

	/** A player shinobi that is not mounted, downed, dead or phased. */
	bool CanSurfaceTraverse() const;

	/** Leaves the wall: world gravity, upright yaw-only rotation facing Facing (default: the wall) at the nearest spot the
	 * capsule fits, falling. No-op when not on a wall. */
	void DetachFromWall(const FVector& Facing = FVector::ZeroVector);

	/** Maps a horizontal camera-relative world intent onto the wall: into the wall = up, out of it = down, along it = sideways. */
	FVector WallInputDirection(const FVector& Intent) const;

	/** On a wall, Jump kicks off it instead of jumping along its normal. */
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	static const FName WaterSurfaceTag;  // walkable water actors (map builders: WATER_SURFACE_TAG)
	static const FName NoTraversalTag;   // never climbed, e.g. map-boundary walls (map builders: NO_TRAVERSAL_TAG)
	static constexpr float WallMaxNormalZ = 0.4226f; // sin 25 deg: steeper faces are walls
	static constexpr float WallReach = 30.f;         // how far ahead of the capsule a face is noticed
	static constexpr float KickOffSpeed = 700.f;
	static constexpr float KickOffUp = 450.f;
```

Add to the `private:` section:

```cpp
	void TryEnterWall();
	void UpdateWallTraversal();
	bool SweepCapsule(const FVector& From, const FVector& To, const FQuat& Rotation, FHitResult& OutHit) const;
	bool Fits(const FVector& Location, const FQuat& Rotation) const;
	bool IsClimbable(const FHitResult& Hit) const;
	/** Turns onto the wall Hit found, at Anchor projected onto it. False (and nothing changes) when the turned capsule would not fit. */
	bool AttachToWall(const FHitResult& Hit, const FVector& Anchor);
```

Change the class comment to:

```cpp
/** Adds a sprint flag that travels in saved moves, so the server agrees with the client (no rubber-banding), and wall/water
 * traversal for players: custom gravity into a wall while running on it, decided inside each move on both machines. */
```

- [ ] **Step 4: Implement entry, wall input, kick-off and forced detach**

In `ShinobiMovementComponent.cpp`, add `#include "Shinobi/ShinobiPlayerCharacter.h"`.

At the top of `UpdateCharacterStateBeforeMovement`, before `const AShinobiCharacterBase* Character = ...`, insert:

```cpp
	if (!CanSurfaceTraverse())
	{
		DetachFromWall(); // mounted, downed, dead, phased, or not a player: never on a wall
	}
	else if (IsWallTraversing())
	{
		UpdateWallTraversal();
	}
	else
	{
		TryEnterWall();
	}
```

Append to the end of the file:

```cpp
// ---- Wall and water traversal ----

const FName UShinobiMovementComponent::WaterSurfaceTag(TEXT("ShinobiWaterSurface"));
const FName UShinobiMovementComponent::NoTraversalTag(TEXT("ShinobiNoSurfaceTraversal"));

bool UShinobiMovementComponent::CanSurfaceTraverse() const
{
	const AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(GetOwner());
	if (!Player || !Player->GetAbilitySystemComponent())
	{
		return false;
	}
	static const FGameplayTagContainer Blockers = []
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(ShinobiTags::State_Mounted);
		Tags.AddTag(ShinobiTags::State_Downed);
		Tags.AddTag(ShinobiTags::State_Dead);
		Tags.AddTag(ShinobiTags::State_Phased);
		return Tags;
	}();
	return !Player->GetAbilitySystemComponent()->HasAnyMatchingGameplayTags(Blockers);
}

bool UShinobiMovementComponent::SweepCapsule(const FVector& From, const FVector& To, const FQuat& Rotation, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiTraversalSweep), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	return GetWorld()->SweepSingleByChannel(OutHit, From, To, Rotation, UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), Params, Response);
}

bool UShinobiMovementComponent::Fits(const FVector& Location, const FQuat& Rotation) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiTraversalFit), false, CharacterOwner);
	FCollisionResponseParams Response;
	InitCollisionParams(Params, Response);
	return !GetWorld()->OverlapBlockingTestByChannel(Location, Rotation, UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), Params, Response);
}

bool UShinobiMovementComponent::IsClimbable(const FHitResult& Hit) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	return Hit.IsValidBlockingHit() && Component && Component->GetCollisionObjectType() == ECC_WorldStatic
		&& FMath::Abs(Hit.ImpactNormal.Z) <= WallMaxNormalZ
		&& !(Hit.GetActor() && Hit.GetActor()->ActorHasTag(NoTraversalTag));
}

void UShinobiMovementComponent::TryEnterWall()
{
	const FVector Push = Acceleration.GetSafeNormal2D();
	if (Push.IsZero() || !(IsMovingOnGround() || IsFalling()))
	{
		return; // standing still beside a wall never attaches
	}
	const FVector Location = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	if (!SweepCapsule(Location, Location + Push * WallReach, UpdatedComponent->GetComponentQuat(), Hit) || !IsClimbable(Hit) || (Push | -Hit.ImpactNormal) < 0.5f)
	{
		return;
	}
	// Kerbs, benches and car sides have vertical faces too: the face must also reach head height.
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const FVector Head = Location + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() - 10.f);
	FHitResult HeadHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiWallHead), false, CharacterOwner);
	if (GetWorld()->LineTraceSingleByChannel(HeadHit, Head, Head - Hit.ImpactNormal * (Capsule->GetScaledCapsuleRadius() + WallReach + 10.f), UpdatedComponent->GetCollisionObjectType(), Params)
		&& IsClimbable(HeadHit))
	{
		AttachToWall(Hit, Location);
	}
}

bool UShinobiMovementComponent::AttachToWall(const FHitResult& Hit, const FVector& Anchor)
{
	const FVector Normal = Hit.ImpactNormal;
	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Center = FVector::PointPlaneProject(Anchor, Hit.ImpactPoint, Normal) + Normal * (HalfHeight + MIN_FLOOR_DIST);
	FVector Facing = FVector::VectorPlaneProject(Velocity, Normal);
	if (Facing.SizeSquared() < 1.f)
	{
		Facing = FVector::VectorPlaneProject(FVector::UpVector, Normal); // face up the wall
	}
	const FQuat Rotation = FRotationMatrix::MakeFromZX(Normal, Facing).ToQuat();
	if (!Fits(Center, Rotation))
	{
		return false; // no room to turn onto the wall: never rotate the capsule into geometry
	}
	SetGravityDirection(-Normal);
	UpdatedComponent->SetWorldLocationAndRotation(Center, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = FVector::VectorPlaneProject(Velocity, Normal);
	if (MovementMode == MOVE_Walking)
	{
		FindFloor(Center, CurrentFloor, false); // already walking (a corner): SetMovementMode would not look for the new floor
	}
	else
	{
		SetMovementMode(MOVE_Walking);
	}
	return true;
}

void UShinobiMovementComponent::UpdateWallTraversal()
{
	if (!IsMovingOnGround())
	{
		DetachFromWall(); // left the wall some other way: fall the normal way
	}
	// Task 2 adds the inner-corner and walk-down-to-the-ground checks here.
}

void UShinobiMovementComponent::DetachFromWall(const FVector& Facing)
{
	if (!IsWallTraversing())
	{
		return;
	}
	const FVector Normal = -GetGravityDirection();
	SetGravityDirection(DefaultGravityDirection);
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Lift = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
	const FVector Look = FVector::VectorPlaneProject(Facing.IsNearlyZero() ? -Normal : Facing, FVector::UpVector);
	const FQuat Upright = FRotationMatrix::MakeFromXZ(Look.IsNearlyZero() ? UpdatedComponent->GetForwardVector() : Look, FVector::UpVector).ToQuat();
	// Standing up swings the capsule's long axis from the wall's normal to vertical: take the first nearby spot it fits,
	// closest to the wall first. With none free, stay put and let the movement component push out of the overlap.
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Near = Location - Normal * (Lift - MIN_FLOOR_DIST);
	const FVector Candidates[] = { Near, Near + FVector::UpVector * Lift, Location, Location + FVector::UpVector * Lift };
	FVector Spot = Location;
	for (const FVector& Candidate : Candidates)
	{
		if (Fits(Candidate, Upright))
		{
			Spot = Candidate;
			break;
		}
	}
	UpdatedComponent->SetWorldLocationAndRotation(Spot, Upright, false, nullptr, ETeleportType::TeleportPhysics);
	SetMovementMode(MOVE_Falling);
}

FVector UShinobiMovementComponent::WallInputDirection(const FVector& Intent) const
{
	const FVector Normal = -GetGravityDirection();
	const FVector Into = -Normal.GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Into).GetSafeNormal();
	const FVector Up = FVector::VectorPlaneProject(FVector::UpVector, Normal).GetSafeNormal();
	return Up * (Intent | Into) + Side * (Intent | Side);
}

bool UShinobiMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (!IsWallTraversing())
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}
	const FVector Normal = -GetGravityDirection();
	DetachFromWall(Normal); // facing away from the wall
	Velocity = Normal * KickOffSpeed + FVector::UpVector * KickOffUp;
	return true; // counts as the first jump, so the double jump still works in the air
}
```

- [ ] **Step 5: Map player input onto the wall and detach on knockback**

In `ShinobiPlayerCharacter.h`, in the `public:` section next to the other overrides, add:

```cpp
	/** Knockback off a wall: detach first, so the launch happens with world gravity. */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;
```

In `ShinobiPlayerCharacter.cpp`, add `#include "Shinobi/ShinobiMovementComponent.h"` if it is missing. Replace the body of `Move` after `const FVector2D Axis = ...`:

```cpp
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix Yaw(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	FVector Intent = Yaw.GetUnitAxis(EAxis::X) * Axis.Y + Yaw.GetUnitAxis(EAxis::Y) * Axis.X;
	if (const UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(GetCharacterMovement()); Movement && Movement->IsWallTraversing())
	{
		Intent = Movement->WallInputDirection(Intent); // the camera stays upright; the wall turns it into up/down/sideways
	}
	AddMovementInput(Intent);
```

Add after `Move`:

```cpp
void AShinobiPlayerCharacter::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	if (UShinobiMovementComponent* Movement = Cast<UShinobiMovementComponent>(GetCharacterMovement()))
	{
		Movement->DetachFromWall();
	}
	Super::LaunchCharacter(LaunchVelocity, bXYOverride, bZOverride);
}
```

- [ ] **Step 6: Build and run the traversal tests**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: all 5 tests PASS:
- `RunningIntoAWallClimbsIt`
- `OnlyRunningIntoATallFaceAttaches`
- `NoRoomToTurnKeepsTheCharacterUpright`
- `JumpKicksOffTheWall`
- `KnockbackAndMountingDetach`

If the climb test stops partway up the 2000 uu wall, the run reached no edge in 1 s. That is fine. Task 2 owns edges.

- [ ] **Step 7: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp
git commit -m "feat: run onto walls with custom gravity, kick off with Jump"
```

---

### Task 2: Wall edges: roof vault, walking down, inner and outer corners

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`

**Interfaces:**
- Consumes (Task 1): `SweepCapsule`, `Fits`, `IsClimbable`, `AttachToWall(Hit, Anchor)`, `DetachFromWall`, `WallReach`
- Produces: `virtual bool CheckFall(...) override`, private `bool TryVault()`, private `bool TryWrapCorner(const FVector& Move)`

- [ ] **Step 1: Write the failing tests**

Append to `ShinobiSurfaceTraversalTests.cpp`, before `#endif`:

```cpp
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
	return true;
}
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: the 3 new tests FAIL (no vault, no ground detach, no wrap). The Task 1 tests still pass.

- [ ] **Step 3: Declare the edge handling**

In `ShinobiMovementComponent.h`, add to `public:` after `DoJump`:

```cpp
	/** Walking off a wall's edge: onto the roof at the top, round an outer corner at the sides, otherwise fall. */
	virtual bool CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump) override;
```

Add to `private:`:

```cpp
	bool TryVault();
	bool TryWrapCorner(const FVector& Move);
```

- [ ] **Step 4: Implement the edges**

In `ShinobiMovementComponent.cpp`, replace `UpdateWallTraversal` with:

```cpp
void UShinobiMovementComponent::UpdateWallTraversal()
{
	if (!IsMovingOnGround())
	{
		DetachFromWall(); // left the wall some other way: fall the normal way
		return;
	}
	const FVector Move = Acceleration.GetSafeNormal();
	if (Move.IsZero())
	{
		return;
	}
	const FVector Location = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	if (!SweepCapsule(Location, Location + Move * WallReach, UpdatedComponent->GetComponentQuat(), Hit) || !Hit.IsValidBlockingHit())
	{
		return;
	}
	if (Hit.ImpactNormal.Z >= GetWalkableFloorZ())
	{
		DetachFromWall(); // walked down onto the ground: stand up on it
	}
	else if (IsClimbable(Hit) && (Move | -Hit.ImpactNormal) >= 0.5f)
	{
		AttachToWall(Hit, Location); // inner corner: onto the next face
	}
}
```

Append:

```cpp
bool UShinobiMovementComponent::CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump)
{
	if (!IsWallTraversing())
	{
		return Super::CheckFall(OldFloor, Hit, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump);
	}
	const FVector Move = Delta.GetSafeNormal();
	if (!((Move | FVector::UpVector) > 0.5f && TryVault()) && !TryWrapCorner(Move))
	{
		DetachFromWall();
	}
	return true; // handled: PhysWalking must not start a fall with the wall's gravity
}

bool UShinobiMovementComponent::TryVault()
{
	const FVector Inward = GetGravityDirection().GetSafeNormal2D(); // gravity points into the wall
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FVector Over = Location + Inward * (HalfHeight + Capsule->GetScaledCapsuleRadius() + 20.f);
	FHitResult Roof;
	if (!SweepCapsule(Over + FVector::UpVector * HalfHeight * 2.f, Over - FVector::UpVector * HalfHeight * 2.f, FQuat::Identity, Roof)
		|| !Roof.IsValidBlockingHit() || Roof.ImpactNormal.Z < GetWalkableFloorZ() || Roof.ImpactPoint.Z < Location.Z - HalfHeight)
	{
		return false; // no roof within reach: a parapet in the way, or only the ground far below
	}
	SetGravityDirection(DefaultGravityDirection);
	UpdatedComponent->SetWorldLocationAndRotation(Roof.Location, FRotationMatrix::MakeFromX(Inward).ToQuat(), false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = Inward * Velocity.Size(); // keep running, now across the roof
	FindFloor(Roof.Location, CurrentFloor, false);
	return true;
}

bool UShinobiMovementComponent::TryWrapCorner(const FVector& Move)
{
	const FVector Normal = -GetGravityDirection();
	const FVector Along = FVector::VectorPlaneProject(Move, Normal).GetSafeNormal();
	if (Along.IsZero())
	{
		return false;
	}
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	// Past an outer corner the next face looks along the move: sweep back onto it from beyond the corner, then stand a
	// little way in from the corner so the whole capsule is over the new face.
	const FVector From = UpdatedComponent->GetComponentLocation() - Normal * (Capsule->GetScaledCapsuleHalfHeight() - Radius) + Along * (Radius + WallReach);
	FHitResult Hit;
	return SweepCapsule(From, From - Along * (Radius + 2.f * WallReach), UpdatedComponent->GetComponentQuat(), Hit)
		&& IsClimbable(Hit) && AttachToWall(Hit, Hit.ImpactPoint - Normal * (Radius + 5.f));
}
```

- [ ] **Step 5: Run the traversal tests**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: all 8 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp
git commit -m "feat: vault onto roofs, stand up at the ground and wrap wall corners"
```

---

### Task 3: Combat on walls: gravity-aware facing

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h` / `.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.cpp:42`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp:512-532` (`FaceAim`)
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/ShinobiDashJutsu.h:49`, `ShinobiDashJutsu.cpp:15-40`
- Modify: `Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.cpp:38`, `GA_Teleport.cpp:114`
- Modify: `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`

**Interfaces:**
- Produces: `static void UShinobiMovementComponent::FaceDirection(AActor* Actor, const FVector& WorldDir)`
- Consumes: `DetachFromWall()` (Task 1), `UShinobiGameplayAbility::GetShinobiMovement()`

- [ ] **Step 1: Write the failing test**

Append before `#endif`:

```cpp
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
```

- [ ] **Step 2: Build and confirm it fails to compile**

Run: `.\Scripts\build.ps1`
Expected: FAIL, because `FaceDirection` is not a member of `UShinobiMovementComponent`.

- [ ] **Step 3: Add `FaceDirection`**

Header, `public:` after `WallInputDirection`:

```cpp
	/** Turns Actor toward WorldDir about its gravity up axis: yaw only when upright, along the wall when on one. A WorldDir
	 * along the up axis leaves the facing as it is. */
	static void FaceDirection(AActor* Actor, const FVector& WorldDir);
```

Append to the `.cpp`:

```cpp
void UShinobiMovementComponent::FaceDirection(AActor* Actor, const FVector& WorldDir)
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const FVector Up = Character && Character->GetCharacterMovement() ? -Character->GetCharacterMovement()->GetGravityDirection() : FVector::UpVector;
	const FVector Forward = FVector::VectorPlaneProject(WorldDir, Up);
	if (Actor && !Forward.IsNearlyZero())
	{
		Actor->SetActorRotation(FRotationMatrix::MakeFromXZ(Forward, Up).Rotator());
	}
}
```

- [ ] **Step 4: Use it in the abilities**

`GA_Melee.cpp` line 42: replace `GetAvatarActorFromActorInfo()->SetActorRotation(FRotator(0.f, Pawn->GetControlRotation().Yaw, 0.f));` with the line below, and add `#include "Shinobi/ShinobiMovementComponent.h"`:

```cpp
		UShinobiMovementComponent::FaceDirection(GetAvatarActorFromActorInfo(), Pawn->GetControlRotation().Vector());
```

`ShinobiJutsuAbility.cpp` `FaceAim`: replace everything after the `if (!Self) { return; }` block with the code below, and add the include if it is missing:

```cpp
	if (Target)
	{
		UShinobiMovementComponent::FaceDirection(Self, Target->GetActorLocation() - Self->GetActorLocation());
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);
	UShinobiMovementComponent::FaceDirection(Self, ViewRotation.Vector());
```

`ShinobiDashJutsu.h` line 49: replace `float DashYaw = 0.f;` with:

```cpp
	FQuat DashRotation = FQuat::Identity; // facing when the dash started (on a wall too)
```

`ShinobiDashJutsu.cpp`:
- In the target branch, replace the `Distance = FMath::Max(0.f, FVector::Dist2D(...) - StopShort);` line with:

```cpp
		Distance = FMath::Max(0.f, FVector::VectorPlaneProject(Target->GetActorLocation() - Self->GetActorLocation(), Self->GetActorUpVector()).Size() - StopShort);
```

- Replace `DashYaw = Self->GetActorRotation().Yaw;` with `DashRotation = Self->GetActorQuat();`.
- In the `ApplyRootMotionConstantForce` call, replace `Self->GetActorForwardVector().GetSafeNormal2D()` with `Self->GetActorForwardVector()`. Forward is horizontal when upright and lies along the wall when on one.
- In `OnDashFinished`, replace `Self->SetActorRotation(FRotator(0.f, DashYaw, 0.f));` with `Self->SetActorRotation(DashRotation);`.

`GA_Substitution.cpp`: directly before `Self->TeleportTo(Behind, FaceAttacker);`, insert:

```cpp
	if (UShinobiMovementComponent* Movement = GetShinobiMovement())
	{
		Movement->DetachFromWall(); // the log is left on the wall; the shinobi lands behind the attacker upright
	}
```

`GA_Teleport.cpp`: directly before `if (Self->TeleportTo(Arrival, FRotator(0.f, ViewRotation.Yaw, 0.f)))`, insert:

```cpp
	if (UShinobiMovementComponent* Movement = GetShinobiMovement())
	{
		Movement->DetachFromWall(); // arrives upright at the kunai
	}
```

- [ ] **Step 5: Build and run the tests that cover the touched abilities**

Run: `.\Scripts\build.ps1`, then each of these filters. Melee is under `Abilities`, dash and `FaceAim` under `Jutsu.`, and Teleport under `Teleport`.
- `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
- `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Abilities`
- `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Jutsu.`
- `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Teleport`

Expected: all PASS. Upright facing is unchanged, so existing ability tests keep passing.

- [ ] **Step 6: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Melee.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiJutsuAbility.cpp Source/ShinobiCoop/Shinobi/Abilities/ShinobiDashJutsu.h Source/ShinobiCoop/Shinobi/Abilities/ShinobiDashJutsu.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Substitution.cpp Source/ShinobiCoop/Shinobi/Abilities/GA_Teleport.cpp Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp
git commit -m "feat: abilities face along walls instead of snapping upright"
```

---

### Task 4: Walkable water

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h` / `.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`
- Modify: `Scripts/world/build_world.py` (`stage_terrain` water and edge walls, `stage_water`, new `stage_traversal`, `STAGES`)
- Modify: `Scripts/london/build_london.py` (`place_city`, `stage_dress` edge walls, new `stage_traversal`, `STAGES`)
- Modify: `Scripts/london/import_london.py:50` (`CHUNK_KINDS` water collision)

**Interfaces:**
- Produces: `bool UShinobiMovementComponent::IsWaterTraversing() const`, `bool IsSurfaceTraversing() const`
- Produces: Python `bw.WATER_SURFACE_TAG`, `bw.NO_TRAVERSAL_TAG`, `bw.walkable_water(actor)`
- Consumes: `WaterSurfaceTag` (Task 1)

- [ ] **Step 1: Write the failing test**

Append before `#endif`:

```cpp
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
```

- [ ] **Step 2: Build and confirm it fails to compile**

Run: `.\Scripts\build.ps1`
Expected: FAIL, because `IsWaterTraversing` is not a member.

- [ ] **Step 3: Add the water query**

Header, `public:` after `IsWallTraversing`:

```cpp
	/** Walking on an actor tagged WaterSurfaceTag. Traces under the feet, so it also works for other players' characters. */
	bool IsWaterTraversing() const;
	bool IsSurfaceTraversing() const { return IsWallTraversing() || IsWaterTraversing(); }
```

`.cpp`:

```cpp
bool UShinobiMovementComponent::IsWaterTraversing() const
{
	if (!CharacterOwner || IsWallTraversing() || !IsMovingOnGround())
	{
		return false;
	}
	const FVector Feet = GetActorFeetLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShinobiWaterCheck), false, CharacterOwner);
	return GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, 5.f), Feet - FVector(0.f, 0.f, 20.f), ECC_Pawn, Params)
		&& Hit.GetActor() && Hit.GetActor()->ActorHasTag(WaterSurfaceTag);
}
```

- [ ] **Step 4: Run the traversal tests**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: all 10 tests PASS.

- [ ] **Step 5: Mark the valley water and edge walls (`Scripts/world/build_world.py`)**

After `WATER_TAG = "ShinobiWater"` (line 58), add:

```python
WATER_SURFACE_TAG = "ShinobiWaterSurface"  # walkable water (UShinobiMovementComponent::WaterSurfaceTag)
NO_TRAVERSAL_TAG = "ShinobiNoSurfaceTraversal"  # never wall-run, e.g. map boundaries (UShinobiMovementComponent::NoTraversalTag)


def walkable_water(actor):
    """Characters stand on the water; shots, the camera and every other trace pass through it."""
    smc = actor.static_mesh_component
    smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
    smc.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    smc.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponseType.ECR_BLOCK)
```

In `stage_terrain`, replace:

```python
    plane.static_mesh_component.set_collision_profile_name("NoCollision")  # wade through: the river bed is the floor
```

with:

```python
    walkable_water(plane)  # shinobi walk on the river
    plane.set_editor_property("tags", [unreal.Name(WATER_TAG), unreal.Name(WATER_SURFACE_TAG)])
```

In the same function, after `wall.static_mesh_component.set_collision_profile_name("InvisibleWall")`, add:

```python
        wall.set_editor_property("tags", [unreal.Name(NO_TRAVERSAL_TAG)])
```

In `stage_water`:
- Replace `plane.static_mesh_component.set_collision_profile_name("NoCollision")` with `walkable_water(plane)`.
- Replace the tags line with `plane.set_editor_property("tags", [unreal.Name(WATER_TAG), unreal.Name(WATER_SURFACE_TAG)])`.
- In the docstring, change `No collision: players wade on the bed.` to `Blocks only Pawn: shinobi walk on the surface.`

Add before `STAGES`:

```python
def stage_traversal(lay):
    """Walls and water (docs/superpowers/specs/2026-10-05-surface-traversal-design.md) on the existing map, without
    rebuilding any other stage: the river becomes walkable and the edge walls unclimbable. Re-running is harmless."""
    world = load_map_fully()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        label = actor.get_actor_label()
        if actor.actor_has_tag(WATER_TAG) or label == "RiverWater":
            walkable_water(actor)
            actor.set_editor_property("tags", [unreal.Name(WATER_TAG), unreal.Name(WATER_SURFACE_TAG)])
        elif label.startswith("EdgeWall"):
            actor.set_editor_property("tags", [unreal.Name(NO_TRAVERSAL_TAG)])
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError(f"failed to save {MAP}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("traversal: river walkable, edge walls unclimbable")
```

Change `STAGES` to:

```python
STAGES = {"terrain": stage_terrain, "dress": stage_dress, "mood": stage_mood, "water": stage_water, "traversal": stage_traversal}
```

- [ ] **Step 6: Mark the London water, remove the river-edge walls, tag the edge walls**

`Scripts/london/import_london.py` line 50: change `"W": "none"` to `"W": "complex"`. Also update the docstring at line 8 to: `collision complex-as-simple on buildings (B), ground (G) and water (W, walked on), none on the backdrop`.

`Scripts/london/build_london.py`:
- Under `import build_world as bw`, add `import import_library as lib  # noqa: E402`. `Scripts/world` is already on `sys.path`.
- In `place_city`, change the kind loop to `for kind in ("G", "B", "K", "W"):`. The hidden river-edge walls (`X`) are not placed: shinobi walk onto the water now.
- Replace the block from `if kind in ("K", "W"):` through `tagged(actor, tag, f"City:{kind}", always_loaded=True)` with:

```python
            if kind == "K":
                actor.static_mesh_component.set_collision_profile_name("NoCollision")
            if kind == "W":
                actor.static_mesh_component.set_cast_shadow(False)
                bw.walkable_water(actor)
            # Always loaded: London is seen across kilometres (skyline, river, bridges), and Nanite streams the
            # detail anyway; streamed cells left the distant city empty with landmarks floating over the void.
            extra = [bw.WATER_SURFACE_TAG] if kind == "W" else []
            tagged(actor, tag, f"City:{kind}", *extra, always_loaded=True)
```

- In `stage_dress`, change `tagged(wall, "LondonDress", always_loaded=True)` (the EdgeWall loop) to `tagged(wall, "LondonDress", bw.NO_TRAVERSAL_TAG, always_loaded=True)`.
- Add before `STAGES`:

```python
def stage_traversal():
    """Walls and water on the existing map: water chunks get exact collision that only Pawn hits, the hidden river-edge
    walls (City:X) go, and the playable-edge walls are marked unclimbable. Re-running is harmless."""
    for path in unreal.EditorAssetLibrary.list_assets(f"{ROOT}/City/W", recursive=True):
        mesh = unreal.EditorAssetLibrary.find_asset_data(path).get_asset()
        if isinstance(mesh, unreal.StaticMesh):
            lib.set_collision(mesh, "complex")
            unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    world = load_map()
    clear(world, "City:X")
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        add = None
        if actor.actor_has_tag("City:W"):
            bw.walkable_water(actor)
            add = bw.WATER_SURFACE_TAG
        elif actor.get_actor_label().startswith("EdgeWall_"):
            add = bw.NO_TRAVERSAL_TAG
        if add and not actor.actor_has_tag(add):
            actor.set_editor_property("tags", list(actor.tags) + [unreal.Name(add)])
    save(world)
    log("traversal: water walkable, river-edge walls removed, edge walls unclimbable")
```

- Change `STAGES` to: `STAGES = {"terrain": stage_terrain, "city": stage_city, "dress": stage_dress, "mood": stage_mood, "traversal": stage_traversal}`.

- [ ] **Step 7: Apply to both maps**

With the editor closed, run:

```powershell
$env:SHINOBI_WORLD_STAGE='traversal'; .\Scripts\run_python.ps1 Scripts\world\build_world.py
$env:SHINOBI_LONDON_STAGE='traversal'; .\Scripts\run_python.ps1 Scripts\london\build_london.py
```

Expected: logs end with `traversal: river walkable, edge walls unclimbable` and `traversal: water walkable, river-edge walls removed, edge walls unclimbable`, with no Python errors.

- [ ] **Step 8: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.h Source/ShinobiCoop/Shinobi/ShinobiMovementComponent.cpp Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp Scripts/world/build_world.py Scripts/london/build_london.py Scripts/london/import_london.py
git add Content/Shinobi/Maps/Lvl_HiddenLeafValley.umap Content/Shinobi/Maps/Lvl_London.umap Content/Shinobi/London/City/W
git status --short Content/__ExternalActors__/Shinobi/Maps
```

Stage only the external-actor packages that the traversal stages changed or deleted: RiverWater, EdgeWall, City:W and City:X. Pick them from that `git status` output; `git add -u` scoped to those paths records the deleted City:X files. Leave the pre-existing untracked actor folders listed in the git status at the start of this session unstaged.

```bash
git commit -m "feat: walkable water on both maps; London river-edge walls removed"
```

---

### Task 5: Foot chakra glow

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.h`
- Create: `Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.cpp`
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h` / `.cpp` (constructor)
- Modify: `Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp`

**Interfaces:**
- Produces: `UShinobiSurfaceFxComponent` with `float GetFade() const` (0 = hidden, 1 = fully shown)
- Consumes: `UShinobiMovementComponent::CanSurfaceTraverse()`, `IsSurfaceTraversing()`, `ShinobiVfx::MakeGlow`, `ShinobiVfx::Colors::Chakra`

- [ ] **Step 1: Write the failing test**

Add `#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"` to the test includes, and append before `#endif`:

```cpp
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
```

- [ ] **Step 2: Build and confirm it fails to compile**

Run: `.\Scripts\build.ps1`
Expected: FAIL, because `ShinobiSurfaceFxComponent.h` is not found.

- [ ] **Step 3: Create the component**

`Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ShinobiSurfaceFxComponent.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Chakra on the soles while a player runs on a wall or walks on water: a thin glow shell around each foot (the charge
 * aura's material, M_ChakraBodyAura, in chakra blue) and a faint light. Purely cosmetic and not replicated: every machine
 * reads the replicated movement state (gravity, movement mode), so the other players see it too. Death, downed and
 * mounting hide it at once; otherwise it fades.
 */
UCLASS(ClassGroup = (Shinobi), meta = (BlueprintSpawnableComponent))
class SHINOBICOOP_API UShinobiSurfaceFxComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UShinobiSurfaceFxComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0 = hidden, 1 = fully shown. */
	float GetFade() const { return Fade; }

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi", meta = (ClampMin = "0.01"))
	float FadeTime = 0.15f;

	/** Shell size around each foot in uu: length along the foot, width, height. */
	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	FVector ShellSize = FVector(30.f, 14.f, 14.f);

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightIntensity = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Shinobi")
	float LightRadius = 150.f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Shells;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Glow;

	float Fade = 0.f;
	float Age = 0.f;
};
```

`Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.cpp`:

```cpp
#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Shinobi/ShinobiMovementComponent.h"
#include "Shinobi/VFX/ShinobiVfx.h"

namespace
{
	// The Manny drives every pose (the hero mesh copies it) and keeps animating while hidden, so its bones are always current.
	const FName FootBones[] = { TEXT("foot_l"), TEXT("foot_r") };
	const FName BallBones[] = { TEXT("ball_l"), TEXT("ball_r") };
}

UShinobiSurfaceFxComponent::UShinobiSurfaceFxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UShinobiSurfaceFxComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Owner = Cast<ACharacter>(GetOwner());
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (IsNetMode(NM_DedicatedServer) || !Owner || !Owner->GetMesh() || !Sphere)
	{
		SetComponentTickEnabled(false); // purely cosmetic
		return;
	}
	Glow = ShinobiVfx::MakeGlow(this, ShinobiVfx::Colors::Chakra, 2.5f, 0.f); // Core 0: a rim, so it reads as a thin layer
	if (!Glow)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Glow->SetScalarParameterValue(TEXT("Fade"), 0.f);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FootBones); ++Index)
	{
		UStaticMeshComponent* Shell = NewObject<UStaticMeshComponent>(Owner);
		Shell->SetStaticMesh(Sphere);
		Shell->SetMaterial(0, Glow);
		Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shell->SetCastShadow(false);
		Shell->SetupAttachment(this);
		Shell->SetVisibility(false);
		Shell->RegisterComponent();
		Shells.Add(Shell);
	}
	Light = NewObject<UPointLightComponent>(Owner);
	Light->SetupAttachment(this);
	Light->SetLightColor(ShinobiVfx::Colors::Chakra);
	Light->SetAttenuationRadius(LightRadius);
	Light->SetCastShadows(false);
	Light->SetSpecularScale(0.f);
	Light->SetIntensity(0.f);
	Light->RegisterComponent();
}

void UShinobiSurfaceFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ACharacter* Owner = Cast<ACharacter>(GetOwner());
	const UShinobiMovementComponent* Movement = Owner ? Cast<UShinobiMovementComponent>(Owner->GetCharacterMovement()) : nullptr;
	if (!Movement || !Glow)
	{
		return;
	}
	Fade = Movement->CanSurfaceTraverse()
		? FMath::FInterpConstantTo(Fade, Movement->IsSurfaceTraversing() ? 1.f : 0.f, DeltaTime, 1.f / FadeTime)
		: 0.f; // death, downed and mounting hide it at once
	Age += DeltaTime;
	Glow->SetScalarParameterValue(TEXT("Fade"), Fade * (0.85f + 0.15f * FMath::Sin(Age * 6.f)));
	if (Light)
	{
		Light->SetIntensity(LightIntensity * Fade);
	}
	const USkeletalMeshComponent* Body = Owner->GetMesh();
	for (int32 Index = 0; Index < Shells.Num(); ++Index)
	{
		UStaticMeshComponent* Shell = Shells[Index];
		Shell->SetVisibility(Fade > 0.f);
		if (Fade <= 0.f)
		{
			continue;
		}
		// From the ankle to the ball of the foot, a little toward the sole: placed from bone positions, so it does not
		// depend on how each bone's axes are oriented.
		const FVector Ankle = Body->GetSocketLocation(FootBones[Index]);
		const FVector Ball = Body->GetSocketLocation(BallBones[Index]);
		const FVector Up = Owner->GetActorUpVector();
		Shell->SetWorldLocationAndRotation((Ankle + Ball) * 0.5f - Up * 3.f, FRotationMatrix::MakeFromXZ(Ball - Ankle, Up).Rotator());
		Shell->SetWorldScale3D(ShellSize / 100.f); // the engine sphere is 100 uu across
	}
}
```

- [ ] **Step 4: Give players the component**

`ShinobiPlayerCharacter.h`:
- Add `class UShinobiSurfaceFxComponent;` with the forward declarations.
- Next to `NameTag`, add:

```cpp
	UPROPERTY(VisibleAnywhere, Category = "Shinobi")
	TObjectPtr<UShinobiSurfaceFxComponent> SurfaceFx;
```

`ShinobiPlayerCharacter.cpp`:
- Add `#include "Shinobi/VFX/ShinobiSurfaceFxComponent.h"`.
- After the `NameTag` setup in the constructor, add:

```cpp
	SurfaceFx = CreateDefaultSubobject<UShinobiSurfaceFxComponent>(TEXT("SurfaceFx"));
	SurfaceFx->SetupAttachment(RootComponent); // turns with the capsule onto walls
```

- [ ] **Step 5: Run the traversal tests**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: all 11 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.h Source/ShinobiCoop/Shinobi/VFX/ShinobiSurfaceFxComponent.cpp Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.cpp Source/ShinobiCoop/Tests/ShinobiSurfaceTraversalTests.cpp
git commit -m "feat: chakra glow around the feet while on walls or water"
```

---

### Task 6: Run cycle on walls (animation blueprint)

**Files:**
- Modify (asset): `Content/Variant_Combat/Anims/ABP_Manny_Combat.uasset`

**Interfaces:**
- Consumes: wall movement from Tasks 1–2.

This task edits a Blueprint graph. Use the `unreal-mcp` skill with the editor open. The `unreal-mcp` server failed to connect in the planning session: ask the user to start the editor and its MCP server. If MCP is still unavailable, give the user the manual steps from Step 2 and wait for confirmation.

- [ ] **Step 1: Inspect how ground speed is computed**

Open `ABP_Manny_Combat`'s event graph (or its `BlueprintThreadSafeUpdateAnimation` function). Find the variable that feeds the idle/walk/run blend, usually `GroundSpeed`, and the node that computes it.

If it uses `Vector Length XY` (or `VSizeXY`) of the velocity, running up a wall reads as about 0 and plays idle, so continue to Step 2. If it already uses the full `Vector Length`, skip to Step 3.

- [ ] **Step 2: Make it gravity-relative**

Replace `Vector Length XY` with `Vector Length`, keeping the same velocity input. This is exact on walls: walking velocity there lies in the wall plane. On level ground it gives the same value. On slopes it is a few percent higher, which is harmless for the blend. Compile and save the Blueprint.

- [ ] **Step 3: Verify visually**

Play-in-editor on `Lvl_London`. Run into a building wall and climb it.
Expected: the run cycle plays, with the body sideways and the feet on the wall. Standing still on the wall plays idle.

- [ ] **Step 4: Commit (only if the asset changed)**

```bash
git add Content/Variant_Combat/Anims/ABP_Manny_Combat.uasset
git commit -m "fix: ground speed counts wall-running velocity"
```

---

### Task 7: In-editor acceptance pass

**Files:**
- Review only. Fix in the owning task's files if a check fails.

- [ ] **Step 1: Build and run the feature tests once more**

Run: `.\Scripts\build.ps1`, then `.\Scripts\run_tests.ps1 -Filter ShinobiCoop.Movement.SurfaceTraversal`
Expected: 11 PASS. Do not run the full suite.

- [ ] **Step 2: Two-player PIE on `Lvl_London`**

Use a listen server plus one client. Check each acceptance criterion from the spec:
1. Run into a building, then climb up and sideways. Jump kicks off. Climbing past the top lands on the roof. Walking down stands you up on the street.
2. Try to get inside a building: run in, climb along window recesses, round an outer corner, kick off. The character never ends up inside.
3. Walk from the embankment onto the Thames and across it without sinking.
4. On a wall, use melee, a projectile jutsu, a dash jutsu (Chidori or Rasengan) and dodge. None of them snaps the character upright.
5. The blue foot glow is visible on both screens while on a wall or the water, and gone after leaving.
6. Ground run and sprint, summoning and riding the dragon, and enemies' movement all behave as before. Mounting from a wall detaches cleanly.
7. Use `p.NetShowCorrections 1`: no persistent corrections while climbing.

- [ ] **Step 3: One-player PIE on `Lvl_HiddenLeafValley`**

Walk on the river. The outer edge walls still block and cannot be climbed.

- [ ] **Step 4: Tune and commit if needed**

If the foot glow looks wrong, adjust the `UShinobiSurfaceFxComponent` defaults (`ShellSize`, `LightIntensity`) and commit the change with a message naming the tuned values.
