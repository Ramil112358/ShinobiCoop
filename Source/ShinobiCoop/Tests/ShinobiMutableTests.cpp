#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: Mutable graphs and compiles are editor-only)

#include "Algo/Count.h"
#include "AnimationRuntime.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/CustomizableObjectFactory.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Retargeter/IKRetargeter.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
#include "Shinobi/Hero/ShinobiMutableMeshLibrary.h"
#include "SkeletalMeshAttributes.h"
#include "Shinobi/Hero/ShinobiFaceBlendLibrary.h"
#include "Shinobi/Hero/ShinobiMutableGraphLibrary.h"
#include "Shinobi/Hero/ShinobiMutableNames.h"
#include "Shinobi/Hero/ShinobiMutableTypes.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/** The first pin of Node going Dir ("In"/"Out") whose name contains Part (ShinobiMutableGraphLibrary::ListPins form). */
	FString FindPin(UEdGraphNode* Node, const TCHAR* Dir, const TCHAR* Part)
	{
		for (const FString& Pin : UShinobiMutableGraphLibrary::ListPins(Node))
		{
			TArray<FString> Fields;
			Pin.ParseIntoArray(Fields, TEXT("|"), false);
			if (Fields.Num() == 3 && Fields[0] == Dir && Fields[1].Contains(Part))
			{
				return Fields[1];
			}
		}
		return FString();
	}

	/** A new transient Customizable Object made by the editor's factory (its graph holds the base Object node). */
	UCustomizableObject* NewTransientObject()
	{
		UCustomizableObjectFactory* Factory = NewObject<UCustomizableObjectFactory>();
		return Cast<UCustomizableObject>(Factory->FactoryCreateNew(UCustomizableObject::StaticClass(), GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UCustomizableObject::StaticClass(), TEXT("CO_GraphHelperTest")),
			RF_Transient, nullptr, GWarn));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableGraphHelperTest, "ShinobiCoop.Mutable.GraphHelper", ShinobiTestFlags)
bool FShinobiMutableGraphHelperTest::RunTest(const FString& Parameters)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	const FShinobiCreatorFrame* Frame = Catalog ? Catalog->FindFrame(EShinobiSex::Male, EShinobiBuild::Normal) : nullptr;
	if (!TestNotNull(TEXT("the male normal frame"), Frame))
	{
		return false;
	}

	UCustomizableObject* Object = NewTransientObject();
	if (!TestNotNull(TEXT("a new object"), Object) || !TestNotNull(TEXT("with a graph"), UShinobiMutableGraphLibrary::GetGraph(Object)))
	{
		return false;
	}
	UEdGraphNode* Base = UShinobiMutableGraphLibrary::FindNode(Object, TEXT("CustomizableObjectNodeObject"), 0);
	UEdGraphNode* Component = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeComponentSkeletalMesh"), -400, 0);
	UEdGraphNode* Mesh = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CustomizableObjectNodeSkeletalMesh"), -800, 0);
	if (!TestNotNull(TEXT("the base node"), Base) || !TestNotNull(TEXT("a component node"), Component) || !TestNotNull(TEXT("a mesh node"), Mesh))
	{
		return false;
	}
	TestTrue(TEXT("the mesh is set"), UShinobiMutableGraphLibrary::SetNodeProperty(Mesh, TEXT("SkeletalMesh"), Frame->Body.ToString()));
	TestTrue(TEXT("the component's reference mesh is set"),
		UShinobiMutableGraphLibrary::SetNodeProperty(Component, TEXT("ReferenceSkeletalMesh"), Frame->Body.ToString()));
	TestTrue(TEXT("component joins the object"), UShinobiMutableGraphLibrary::LinkPins(
		Component, FindPin(Component, TEXT("Out"), TEXT("")), Base, FindPin(Base, TEXT("In"), TEXT("Component"))));
	TestTrue(TEXT("the graph description shows that link"), UShinobiMutableGraphLibrary::DescribeGraph(Object).Contains(
		TEXT("CONodeComponentSkeletalMesh") + FString(TEXT(".Component -> CustomizableObjectNodeObject"))));
	// Mutable 5.8's chain for a constant mesh (as in the Mutable Sample's CO_ReshapeMesh): mesh section -> section node ->
	// make skeletal mesh (LOD 0) -> object make -> passthrough into the component.
	UEdGraphNode* Section = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeSkeletalMeshSection"), -600, 0);
	UEdGraphNode* Make = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeSkeletalMeshMake_V2"), -500, 0);
	UEdGraphNode* ObjectMake = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeSkeletalMeshObjectMake"), -450, 0);
	TestTrue(TEXT("mesh section feeds a section node"), UShinobiMutableGraphLibrary::LinkPins(Mesh, TEXT("LOD 0 - Section 0 - Mesh"), Section, TEXT("Mesh_Input_Pin")));
	TestTrue(TEXT("section feeds LOD 0"), UShinobiMutableGraphLibrary::LinkPins(Section, TEXT("Mesh Section_Output_Pin"), Make, TEXT("LOD 0")));
	TestTrue(TEXT("skeletal mesh feeds the object make"), UShinobiMutableGraphLibrary::LinkPins(Make, TEXT("Skeletal Mesh"), ObjectMake, TEXT("Skeletal Mesh")));
	TestTrue(TEXT("object make feeds the component"), UShinobiMutableGraphLibrary::LinkPins(
		ObjectMake, TEXT("Passthrough Skeletal Mesh"), Component, TEXT("Passthrough Skeletal Mesh")));

	TestEqual(TEXT("the object compiles"), UCustomizableObjectEditorFunctionLibrary::CompileCustomizableObjectSynchronously(Object),
		ECustomizableObjectCompilationState::Completed);

	UCustomizableObjectInstance* Instance = Object->CreateInstance();
	TSharedRef<TOptional<EUpdateResult>> Result = MakeShared<TOptional<EUpdateResult>>();
	FInstanceUpdateNativeDelegate Done;
	Done.AddLambda([Result](const FUpdateContext& Context) { *Result = Context.UpdateResult; });
	Instance->UpdateSkeletalMeshAsyncResult(Done, true, true);
	const double Deadline = FPlatformTime::Seconds() + 30.0;
	// Keep the transient object and instance alive across the frames the build takes.
	TSharedRef<TStrongObjectPtr<UCustomizableObjectInstance>> Keep = MakeShared<TStrongObjectPtr<UCustomizableObjectInstance>>(Instance);
	TSharedRef<TStrongObjectPtr<UCustomizableObject>> KeepObject = MakeShared<TStrongObjectPtr<UCustomizableObject>>(Object);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Result, Deadline, Keep, KeepObject]()
	{
		if (!Result->IsSet() && FPlatformTime::Seconds() < Deadline)
		{
			return false;
		}
		TestTrue(TEXT("the instance builds"), Result->IsSet() && Result->GetValue() == EUpdateResult::Success);
		return true;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableNodePropertyPathTest, "ShinobiCoop.Mutable.NodePropertyPath", ShinobiTestFlags)
bool FShinobiMutableNodePropertyPathTest::RunTest(const FString& Parameters)
{
	UCustomizableObject* Object = NewTransientObject();
	UEdGraphNode* Remove = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CustomizableObjectNodeModifierRemoveMeshBlocks"), 0, 0);
	if (!TestNotNull(TEXT("a remove-blocks node"), Remove))
	{
		return false;
	}
	// The blocks live on the node's layout subobject, which the Details panel edits through its own widget.
	TestTrue(TEXT("a block set through the layout"), UShinobiMutableGraphLibrary::SetNodeProperty(Remove, TEXT("Layout.Blocks"),
		TEXT("((Min=(X=1,Y=2),Max=(X=3,Y=4)))")));
	TestTrue(TEXT("reads back"), UShinobiMutableGraphLibrary::GetNodeProperty(Remove, TEXT("Layout.Blocks")).Contains(TEXT("Min=(X=1,Y=2),Max=(X=3,Y=4)")));
	AddExpectedError(TEXT("SetNodeProperty: no property"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("a missing path fails"), UShinobiMutableGraphLibrary::SetNodeProperty(Remove, TEXT("Layout.NoSuchProperty"), TEXT("1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableClearGraphTest, "ShinobiCoop.Mutable.ClearGraph", ShinobiTestFlags)
bool FShinobiMutableClearGraphTest::RunTest(const FString& Parameters)
{
	UCustomizableObject* Object = NewTransientObject();
	UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeSkeletalMeshMerge"), 0, 0);
	UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CustomizableObjectNodeFloatParameter"), 0, 100);
	UShinobiMutableGraphLibrary::ClearGraph(Object);
	TestNotNull(TEXT("the root object node stays"), UShinobiMutableGraphLibrary::FindNode(Object, TEXT("CustomizableObjectNodeObject"), 0));
	TestNull(TEXT("other nodes go"), UShinobiMutableGraphLibrary::FindNode(Object, TEXT("CONodeSkeletalMeshMerge"), 0));
	TestNull(TEXT("all of them"), UShinobiMutableGraphLibrary::FindNode(Object, TEXT("CustomizableObjectNodeFloatParameter"), 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutablePinDefaultTest, "ShinobiCoop.Mutable.PinDefault", ShinobiTestFlags)
bool FShinobiMutablePinDefaultTest::RunTest(const FString& Parameters)
{
	UCustomizableObject* Object = NewTransientObject();
	UEdGraphNode* Merge = UShinobiMutableGraphLibrary::AddNode(Object, TEXT("CONodeModifierMutableSkeletalMeshMerge"), 0, 0);
	if (!TestNotNull(TEXT("a merge modifier"), Merge))
	{
		return false;
	}
	TestTrue(TEXT("its target name pin takes a typed value"), UShinobiMutableGraphLibrary::SetPinDefault(Merge, TEXT("Skeletal Mesh Name"), TEXT("Body")));
	TestTrue(TEXT("the description shows it"), UShinobiMutableGraphLibrary::DescribeGraph(Object).Contains(TEXT("Skeletal Mesh Name='Body'")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableAppearanceV2Test, "ShinobiCoop.Mutable.AppearanceV2", ShinobiTestFlags)
bool FShinobiMutableAppearanceV2Test::RunTest(const FString& Parameters)
{
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex::Female;
	Look.Build = EShinobiBuild::Own;
	Look.BuildBlend = 77;
	Look.Head = 4;
	Look.Slot(EShinobiWardrobeSlot::Top) = 2;
	Look.Slot(EShinobiWardrobeSlot::Outer) = 1;
	Look.Slot(EShinobiWardrobeSlot::Accessory) = 0;
	Look.Slot(EShinobiWardrobeSlot::Cloak) = 3;
	Look.SlotColour[0] = 5;
	Look.SlotColour[5] = 11;
	Look.SlotWear[1] = 200;
	Look.SkinTone = 40;
	Look.Aged = 90;
	Look.MakeUp = 2;
	Look.Freckles = 1;
	Look.Tattoos = {{1, 2, -30, 40, 128, 200}, {3, 0, 0, 0, 0, 128}};
	Look.FaceShape = {12};
	Look.BodyShape = {-5, 5};
	FShinobiAppearance Back;
	TestTrue(TEXT("a version-2 code reads back"), FShinobiAppearance::FromCode(Look.ToCode(), Back));
	TestTrue(TEXT("to the same look"), Back == Look);
	TestEqual(TEXT("Slot() is the named field"), Look.Outer, uint8(1));

	FShinobiAppearance Wild = Look;
	Wild.Build = EShinobiBuild(uint8(EShinobiBuild::Own) + 1);
	Wild.Tattoos.SetNum(5);
	Wild.Tattoos[4].Region = 250;
	Wild.Sanitize();
	TestEqual(TEXT("a build past Own is the normal one"), Wild.Build, EShinobiBuild::Normal);
	TestEqual(TEXT("at most three tattoos"), Wild.Tattoos.Num(), ShinobiMutable::MaxTattoos);
	TestEqual(TEXT("wear untouched"), Wild.SlotWear[1], uint8(200));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableBasesTest, "ShinobiCoop.Mutable.Bases", ShinobiTestFlags)
bool FShinobiMutableBasesTest::RunTest(const FString& Parameters)
{
	for (const TCHAR* Sex : {TEXT("Male"), TEXT("Female")})
	{
		const FString Path = FString::Printf(TEXT("/Game/Shinobi/Mutable/Bases/SK_MutableBase_%s.SK_MutableBase_%s"), Sex, Sex);
		const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *Path);
		if (!TestNotNull(FString::Printf(TEXT("%s base"), Sex), Base))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s base skeleton"), Sex), GetNameSafe(Base->GetSkeleton()), FString(TEXT("metahuman_base_skel")));
		for (const TCHAR* Morph : {TEXT("Build_Under"), TEXT("Build_Over")})
		{
			TestNotNull(FString::Printf(TEXT("%s base has %s"), Sex, Morph), Base->FindMorphTarget(Morph));
		}
		// The Own build is the roster character's own body, on the same skeleton.
		const USkeletalMesh* Own = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::OwnPath(Sex));
		TestTrue(FString::Printf(TEXT("%s own body on the base's skeleton"), Sex), Own && GetNameSafe(Own->GetSkeleton()) == GetNameSafe(Base->GetSkeleton()));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableObjectCompilesTest, "ShinobiCoop.Mutable.ObjectCompiles", ShinobiTestFlags)
bool FShinobiMutableObjectCompilesTest::RunTest(const FString& Parameters)
{
	UCustomizableObject* Object = LoadObject<UCustomizableObject>(nullptr, ShinobiMutable::ObjectPath);
	if (!TestNotNull(TEXT("CO_Shinobi"), Object))
	{
		return false;
	}
	TestEqual(TEXT("compiles"), UCustomizableObjectEditorFunctionLibrary::CompileCustomizableObjectSynchronously(Object),
		ECustomizableObjectCompilationState::Completed);
	TArray<FString> Expected = {ShinobiMutable::Body, ShinobiMutable::FitBase, ShinobiMutable::Build, ShinobiMutable::BuildBlend};
	for (const TCHAR* Slot : ShinobiMutable::SlotNames)
	{
		Expected.Append({Slot, FString(Slot) + ShinobiMutable::MeshSuffix, FString(Slot) + ShinobiMutable::ClipSuffix,
			FString(Slot) + ShinobiMutable::HidesSuffix});
	}
	for (const FString& Name : Expected)
	{
		TestTrue(FString::Printf(TEXT("has parameter '%s'"), *Name), Object->ContainsParameter(Name));
	}
	for (const TCHAR* Slot : ShinobiMutable::SlotNames)
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot));
		if (!TestNotNull(FString::Printf(TEXT("DT_Wardrobe_%s"), Slot), Table))
		{
			continue;
		}
		Table->ForeachRow<FShinobiWardrobeRow>(TEXT("ObjectCompiles"), [&](const FName& Row, const FShinobiWardrobeRow& Piece)
		{
			TestFalse(FString::Printf(TEXT("%s.%s has a mesh for a sex"), Slot, *Row.ToString()), Piece.MaleMesh.IsNull() && Piece.FemaleMesh.IsNull());
		});
	}
	const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath);
	TestTrue(TEXT("a 12-colour palette"), Palette && Palette->GetRowMap().Num() == 12);
	return true;
}

namespace
{
	/** A test world with an actor wearing a body component (driver mesh as root), kept alive across latent commands. */
	struct FMutableWearer
	{
		FShinobiTestWorld World;
		AActor* Actor = nullptr;
		UShinobiBodyComponent* Body = nullptr;

		FMutableWearer()
		{
			Actor = World.Spawn<AActor>();
			USkeletalMeshComponent* Driver = NewObject<USkeletalMeshComponent>(Actor, TEXT("Driver"));
			Actor->SetRootComponent(Driver);
			Driver->RegisterComponent();
			Body = NewObject<UShinobiBodyComponent>(Actor, TEXT("Body"));
			Body->SetupAttachment(Driver);
			Body->RegisterComponent();
		}

		/** The worn mesh's head height in its reference pose (a generated mesh's bounds are the object's reference mesh's,
		 * but its skeleton is the base's that was built), or 0. */
		float Height() const
		{
			const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
			const int32 Head = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
			return Head == INDEX_NONE ? 0.f : float(FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), Head).GetLocation().Z);
		}
	};

	/** Waits (up to 60 s) until Done is true, then runs Then. */
	void WaitThen(FAutomationTestBase& Test, TFunction<bool()> Done, TFunction<void()> Then)
	{
		const double Deadline = FPlatformTime::Seconds() + 60.0;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([&Test, Done, Then, Deadline]()
		{
			if (!Done() && FPlatformTime::Seconds() < Deadline)
			{
				return false;
			}
			Then();
			return true;
		}));
	}

	FShinobiAppearance MutableLook(EShinobiSex Sex, EShinobiBuild Build)
	{
		FShinobiAppearance Look;
		Look.Sex = Sex;
		Look.Build = Build;
		return Look; // Top, Bottom and Shoes 0: each table's first piece
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableWearBuildsTest, "ShinobiCoop.Mutable.WearBuilds", ShinobiTestFlags)
bool FShinobiMutableWearBuildsTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	TestTrue(TEXT("a Mutable look is accepted"), Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Female, EShinobiBuild::Over)));
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer]()
	{
		const USkeletalMesh* Mesh = Wearer->Body->GetSkeletalMeshAsset();
		if (TestNotNull(TEXT("a generated body"), Mesh))
		{
			TestTrue(TEXT("made by Mutable (transient)"), Mesh->HasAnyFlags(RF_Transient));
			for (const TCHAR* Bone : {TEXT("pelvis"), TEXT("spine_05"), TEXT("head"), TEXT("hand_r"), TEXT("foot_l")})
			{
				TestTrue(FString::Printf(TEXT("keeps %s for retargeting"), Bone), Mesh->GetRefSkeleton().FindBoneIndex(Bone) != INDEX_NONE);
			}
			TestTrue(TEXT("with its pieces merged in (more than the bare body's one section)"), Mesh->GetResourceForRendering()->LODRenderData[0].RenderSections.Num() > 1);
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableLastRequestWinsTest, "ShinobiCoop.Mutable.LastRequestWins", ShinobiTestFlags)
bool FShinobiMutableLastRequestWinsTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	TSharedRef<float> MaleHeight = MakeShared<float>(0.f);
	Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal));
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [Wearer, MaleHeight]()
	{
		*MaleHeight = Wearer->Height();
		// Two changes before either builds: the second (female, shorter) must be the one that ends up worn.
		Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Over));
		Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Female, EShinobiBuild::Normal));
	});
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, MaleHeight]()
	{
		TestTrue(TEXT("the male body was built first"), *MaleHeight > 100.f);
		TestTrue(FString::Printf(TEXT("the female body is worn last (%.0f cm vs male %.0f cm)"), Wearer->Height(), *MaleHeight),
			Wearer->Height() > 100.f && Wearer->Height() < *MaleHeight - 5.f);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableFailureKeepsMeshTest, "ShinobiCoop.Mutable.FailureKeepsMesh", ShinobiTestFlags)
bool FShinobiMutableFailureKeepsMeshTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	USkeletalMesh* Before = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(TEXT("Male")));
	Wearer->Body->SetSkeletalMeshAsset(Before);
	UShinobiMutableLook::ObjectPathOverride = TEXT("/Game/Shinobi/Mutable/CO_Missing.CO_Missing");
	AddExpectedError(TEXT("CO_Missing"), EAutomationExpectedErrorFlags::Contains, 1);
	Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal));
	Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Female, EShinobiBuild::Normal)); // logged once, not twice
	UShinobiMutableLook::ObjectPathOverride.Reset();
	TestFalse(TEXT("nothing pending"), Wearer->Body->IsMutableBuildPending());
	TestTrue(TEXT("the previous mesh stays"), Wearer->Body->GetSkeletalMeshAsset() == Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableRosterAfterMutableTest, "ShinobiCoop.Mutable.RosterAfterMutable", ShinobiTestFlags)
bool FShinobiMutableRosterAfterMutableTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal));
	// The roster pick comes before the build finishes: the build must not land on the roster body.
	const FShinobiRosterEntry& Hero = AShinobiPlayerCharacter::Roster[0];
	TestTrue(TEXT("the hero is worn"), Wearer->Body->Wear(Hero.Mesh, Hero.Retargeter));
	TestFalse(TEXT("the Mutable build is dropped"), Wearer->Body->IsMutableBuildPending());
	const double Until = FPlatformTime::Seconds() + 5.0;
	WaitThen(*this, [Until]() { return FPlatformTime::Seconds() > Until; }, [this, Wearer, Hero]()
	{
		TestEqual(TEXT("still the hero"), GetPathNameSafe(Wearer->Body->GetSkeletalMeshAsset()), FString(Hero.Mesh));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableV1ClothesKeptTest, "ShinobiCoop.Mutable.V1ClothesKept", ShinobiTestFlags)
bool FShinobiMutableV1ClothesKeptTest::RunTest(const FString& Parameters)
{
	// A version-1 look's Top/Bottom/Shoes index the catalog's styles within its sex: row N of each table is that piece.
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	if (!TestNotNull(TEXT("catalog"), Catalog))
	{
		return false;
	}
	for (const EShinobiWardrobeSlot Slot : {EShinobiWardrobeSlot::Top, EShinobiWardrobeSlot::Bottom, EShinobiWardrobeSlot::Shoes})
	{
		const TCHAR* SlotName = ShinobiMutable::SlotNames[int32(Slot)];
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(SlotName));
		const TArray<FName> Rows = Table ? Table->GetRowNames() : TArray<FName>();
		for (const EShinobiSex Sex : {EShinobiSex::Male, EShinobiSex::Female})
		{
			for (int32 Index = 0; Index < Catalog->CountStyles(Sex, Slot); ++Index)
			{
				const FShinobiCreatorStyle* Style = Catalog->FindStyle(Sex, Slot, uint8(Index));
				const FShinobiWardrobeRow* Row = Rows.IsValidIndex(Index) ? Table->FindRow<FShinobiWardrobeRow>(Rows[Index], TEXT("V1ClothesKept")) : nullptr;
				const TSoftObjectPtr<USkeletalMesh>* Worn = Row ? &(Sex == EShinobiSex::Female ? Row->FemaleMesh : Row->MaleMesh) : nullptr;
				TestTrue(FString::Printf(TEXT("%s %s %d is the same piece"), SlotName, Sex == EShinobiSex::Female ? TEXT("female") : TEXT("male"), Index),
					Worn && Style && Style->ByBuild.IsValidIndex(1) && Worn->ToSoftObjectPath() == Style->ByBuild[1].ToSoftObjectPath());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableMissingObjectFallsBackTest, "ShinobiCoop.Mutable.MissingObjectFallsBack", ShinobiTestFlags)
bool FShinobiMutableMissingObjectFallsBackTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	const FShinobiRosterEntry& Hero = AShinobiPlayerCharacter::Roster[0];
	Wearer->Body->Wear(Hero.Mesh, Hero.Retargeter);
	UIKRetargeter* HeroRetargeter = Wearer->Body->GetRetargeter();
	UShinobiMutableLook::ObjectPathOverride = TEXT("/Game/Shinobi/Mutable/CO_Missing.CO_Missing");
	AddExpectedError(TEXT("CO_Missing"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("a made look can't be worn without CO_Shinobi (the caller falls back to the hero)"),
		Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal)));
	UShinobiMutableLook::ObjectPathOverride.Reset();
	TestEqual(TEXT("the hero's body stays"), GetPathNameSafe(Wearer->Body->GetSkeletalMeshAsset()), FString(Hero.Mesh));
	TestTrue(TEXT("with the hero's retargeter"), Wearer->Body->GetRetargeter() == HeroRetargeter);
	TestEqual(TEXT("and no MetaHuman face on it"), Wearer->Body->GetMetaHumanParts().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutablePendingKeepsRosterTest, "ShinobiCoop.Mutable.PendingKeepsRoster", ShinobiTestFlags)
bool FShinobiMutablePendingKeepsRosterTest::RunTest(const FString& Parameters)
{
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	const FShinobiRosterEntry& Hero = AShinobiPlayerCharacter::Roster[0];
	Wearer->Body->Wear(Hero.Mesh, Hero.Retargeter);
	UIKRetargeter* HeroRetargeter = Wearer->Body->GetRetargeter();
	TSharedRef<int32> Built = MakeShared<int32>(0);
	Wearer->Body->OnBodyBuilt.AddLambda([Built]() { ++*Built; });
	TestTrue(TEXT("a made look is accepted"), Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal)));
	// Until the build lands, the roster body is worn as it was: its retargeter, no second (MetaHuman) head.
	TestEqual(TEXT("pending: the hero's body"), GetPathNameSafe(Wearer->Body->GetSkeletalMeshAsset()), FString(Hero.Mesh));
	TestTrue(TEXT("pending: the hero's retargeter"), Wearer->Body->GetRetargeter() == HeroRetargeter);
	TestEqual(TEXT("pending: no MetaHuman face yet"), Wearer->Body->GetMetaHumanParts().Num(), 0);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, HeroRetargeter, Built]()
	{
		TestTrue(TEXT("built: the generated body"), Wearer->Body->GetSkeletalMeshAsset() && Wearer->Body->GetSkeletalMeshAsset()->HasAnyFlags(RF_Transient));
		TestTrue(TEXT("built: the crowd retargeter"), Wearer->Body->GetRetargeter() && Wearer->Body->GetRetargeter() != HeroRetargeter);
		TestTrue(TEXT("built: its MetaHuman face"), Wearer->Body->GetMetaHumanParts().ContainsByPredicate([](const UActorComponent* Part) { return Part && Part->GetName().StartsWith(TEXT("Face")); }));
		TestEqual(TEXT("built: OnBodyBuilt once (the character re-dresses its outfit then)"), *Built, 1);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableCatalogPresetsTest, "ShinobiCoop.Mutable.CatalogPresets", ShinobiTestFlags)
bool FShinobiMutableCatalogPresetsTest::RunTest(const FString& Parameters)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	if (!TestNotNull(TEXT("catalog"), Catalog))
	{
		return false;
	}
	for (const FShinobiRosterEntry& Entry : AShinobiPlayerCharacter::Roster)
	{
		const FShinobiAppearance* Look = Catalog->FindPreset(Entry.Name);
		if (!TestNotNull(FString::Printf(TEXT("a preset for %s"), Entry.Name), Look))
		{
			continue;
		}
		if (Entry.MetaHuman)
		{
			// RamilMH and BikaMH: their own MetaHuman face on their own body.
			const FShinobiCreatorFace* Face = Catalog->FindFace(Look->Sex, Look->Face);
			TestEqual(FString::Printf(TEXT("%s has no static head"), Entry.Name), Look->Head, FShinobiAppearance::None);
			TestEqual(FString::Printf(TEXT("%s wears its own body"), Entry.Name), Look->Build, EShinobiBuild::Own);
			TestTrue(FString::Printf(TEXT("%s wears its own face"), Entry.Name), Face && Face->Id == FName(Entry.Name));
			continue;
		}
		const FShinobiCreatorHead* Head = Catalog->FindHead(Look->Head);
		if (TestNotNull(FString::Printf(TEXT("%s's head"), Entry.Name), Head))
		{
			TestEqual(FString::Printf(TEXT("%s's head is its own"), Entry.Name), Head->Id, FName(Entry.Name));
			TestEqual(FString::Printf(TEXT("%s's head on its sex's base"), Entry.Name), Head->Sex, Look->Sex);
			for (int32 Channel = 0; Channel < 3; ++Channel)
			{
				const float Value = Head->SkinTint.Component(Channel);
				TestTrue(FString::Printf(TEXT("%s's skin tint channel %d (%.2f) is plausible"), Entry.Name, Channel, Value), Value >= 0.25f && Value <= 4.f);
			}
		}
	}
	TestNull(TEXT("no head for None"), Catalog->FindHead(FShinobiAppearance::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableHeadsImportedTest, "ShinobiCoop.Mutable.HeadsImported", ShinobiTestFlags)
bool FShinobiMutableHeadsImportedTest::RunTest(const FString& Parameters)
{
	// The roster heads grafted onto the Mutable bases (Scripts/setup_mutable.py heads), on the base skeleton of their sex.
	const TPair<const TCHAR*, const TCHAR*> Heads[] = {{TEXT("Hero"), TEXT("Male")}, {TEXT("Bika"), TEXT("Female")}, {TEXT("Cat"), TEXT("Female")},
		{TEXT("Misha"), TEXT("Male")}, {TEXT("Pabu"), TEXT("Male")}, {TEXT("Ramil"), TEXT("Male")}, {TEXT("Elvin"), TEXT("Male")}};
	for (const TPair<const TCHAR*, const TCHAR*>& Head : Heads)
	{
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Shinobi/Mutable/Heads/SK_Head_%s.SK_Head_%s"), Head.Key, Head.Key));
		const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(Head.Value));
		if (!TestNotNull(FString::Printf(TEXT("SK_Head_%s"), Head.Key), Mesh) || !Base)
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s on the %s base's skeleton"), Head.Key, Head.Value), Mesh->GetSkeleton() == Base->GetSkeleton());
		TestTrue(FString::Printf(TEXT("%s has a head bone"), Head.Key), Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head")) != INDEX_NONE);
		const int32 Necks = Algo::CountIf(Mesh->GetMaterials(), [](const FSkeletalMaterial& Slot) { return Slot.MaterialSlotName.ToString().EndsWith(TEXT("_Neck")); });
		TestEqual(FString::Printf(TEXT("%s has one neck slot"), Head.Key), Necks, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableAddBoneSkinningTest, "ShinobiCoop.Mutable.AddBoneSkinning", ShinobiTestFlags)
bool FShinobiMutableAddBoneSkinningTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* Base = DuplicateObject<USkeletalMesh>(LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(TEXT("Male"))), GetTransientPackage());
	if (!TestNotNull(TEXT("a copy of the male base"), Base))
	{
		return false;
	}
	const int32 Added = UShinobiMutableMeshLibrary::AddBoneSkinning(Base, TEXT("head"), 8, 1.f / 255.f);
	TestEqual(TEXT("eight vertices take the head"), Added, 8);
	FMeshDescription Mesh;
	TestTrue(TEXT("LOD 0 description"), Base->CloneMeshDescription(0, Mesh));
	FSkeletalMeshConstAttributes Attributes(Mesh);
	const int32 HeadIndex = Base->GetRefSkeleton().FindBoneIndex(TEXT("head"));
	int32 Skinned = 0;
	for (const FVertexID Vertex : Mesh.Vertices().GetElementIDs())
	{
		for (const UE::AnimationCore::FBoneWeight& Weight : Attributes.GetVertexSkinWeights().Get(Vertex))
		{
			Skinned += Weight.GetBoneIndex() == HeadIndex ? 1 : 0;
		}
	}
	TestEqual(TEXT("the LOD 0 description skins head on those vertices"), Skinned, 8);
	TestEqual(TEXT("an unknown bone adds nothing"), UShinobiMutableMeshLibrary::AddBoneSkinning(Base, TEXT("no_such_bone"), 8, 0.01f), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableBodyKeepsSkeletonTest, "ShinobiCoop.Mutable.BodyKeepsSkeleton", ShinobiTestFlags)
bool FShinobiMutableBodyKeepsSkeletonTest::RunTest(const FString& Parameters)
{
	// Merged clothes must not move the body's bones: a City Sample shirt (skinned to the head) once put the male head at
	// its own legacy body's height, 7 cm below the base's.
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(TEXT("Male")));
	const int32 Head = Base ? Base->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
	const float BaseHead = Head == INDEX_NONE ? 0.f : float(FAnimationRuntime::GetComponentSpaceTransformRefPose(Base->GetRefSkeleton(), Head).GetLocation().Z);
	Wearer->Body->WearAppearance(MutableLook(EShinobiSex::Male, EShinobiBuild::Normal)); // first pieces of every slot
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, BaseHead]()
	{
		TestTrue(FString::Printf(TEXT("clothed head %.1f cm is the base's %.1f cm"), Wearer->Height(), BaseHead), FMath::Abs(Wearer->Height() - BaseHead) < 0.5f);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableOwnMovesHeadTest, "ShinobiCoop.Mutable.OwnMovesHead", ShinobiTestFlags)
bool FShinobiMutableOwnMovesHeadTest::RunTest(const FString& Parameters)
{
	// The Own build is RamilMH's or BikaMH's own body: its skeleton must follow, or the face floats off the neck.
	auto HeadHeight = [](const USkeletalMesh* Mesh)
	{
		const int32 Head = Mesh ? Mesh->GetRefSkeleton().FindBoneIndex(TEXT("head")) : INDEX_NONE;
		return Head == INDEX_NONE ? 0.f : float(FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetRefSkeleton(), Head).GetLocation().Z);
	};
	struct FCase { EShinobiSex Sex; const TCHAR* Sexes; const TCHAR* Own; };
	const FCase Cases[] = {
		{EShinobiSex::Male, TEXT("Male"), TEXT("/Game/Shinobi/MetaHumans/RamilMH/Build/MHC_RamilMH/Body/SKM_MHC_RamilMH_BodyMesh.SKM_MHC_RamilMH_BodyMesh")},
		{EShinobiSex::Female, TEXT("Female"), TEXT("/Game/Shinobi/MetaHumans/BikaMH/Build/MHC_BikaMH/Body/SKM_MHC_BikaMH_BodyMesh.SKM_MHC_BikaMH_BodyMesh")}};
	for (const FCase& Case : Cases)
	{
		const float OwnHead = HeadHeight(LoadObject<USkeletalMesh>(nullptr, Case.Own));
		const float BaseHead = HeadHeight(LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(Case.Sexes)));
		TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
		FShinobiAppearance Look = MutableLook(Case.Sex, EShinobiBuild::Own);
		Look.BuildBlend = 255;
		Wearer->Body->WearAppearance(Look);
		const FString Name = Case.Sexes;
		WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, OwnHead, BaseHead, Name]()
		{
			const float Built = Wearer->Height();
			TestTrue(FString::Printf(TEXT("%s own head differs from the base's (%.1f vs %.1f cm)"), *Name, OwnHead, BaseHead), FMath::Abs(OwnHead - BaseHead) > 1.5f);
			TestTrue(FString::Printf(TEXT("%s Own build's head %.1f cm is the own body's %.1f cm"), *Name, Built, OwnHead), FMath::Abs(Built - OwnHead) < 1.f);
		});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableBuildMorphBakeTest, "ShinobiCoop.Mutable.BuildMorphBake", ShinobiTestFlags)
bool FShinobiMutableBuildMorphBakeTest::RunTest(const FString& Parameters)
{
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	const FShinobiCreatorFrame* Normal = Catalog ? Catalog->FindFrame(EShinobiSex::Male, EShinobiBuild::Normal) : nullptr;
	const FShinobiCreatorFrame* Over = Catalog ? Catalog->FindFrame(EShinobiSex::Male, EShinobiBuild::Over) : nullptr;
	if (!TestTrue(TEXT("the male normal and over frames"), Normal && Over))
	{
		return false;
	}
	// A transient copy: the frame itself stays untouched.
	USkeletalMesh* Base = DuplicateObject<USkeletalMesh>(Normal->Body.LoadSynchronous(), GetTransientPackage());
	const FString Report = UShinobiFaceBlendLibrary::BakeFaceBlendMorphs(Base, {Over->Body.LoadSynchronous()}, {TEXT("Over")}, 1,
		TEXT("root"), TEXT("Build_"), 50.f);
	AddInfo(Report);
	TestNotNull(TEXT("the base gets a Build_Over morph"), Base->FindMorphTarget(TEXT("Build_Over")));
	TestNull(TEXT("and no face-style Blend_ morph"), Base->FindMorphTarget(TEXT("Blend_Over")));
	return true;
}

#endif

