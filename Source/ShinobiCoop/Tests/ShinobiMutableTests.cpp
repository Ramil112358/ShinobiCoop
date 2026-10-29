#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: Mutable graphs and compiles are editor-only)

#include "AnimationRuntime.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/CustomizableObjectFactory.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Shinobi/Hero/ShinobiBodyComponent.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiMutableLook.h"
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
		for (const TCHAR* Morph : {TEXT("Build_Under"), TEXT("Build_Over"), TEXT("Build_Own")})
		{
			TestNotNull(FString::Printf(TEXT("%s base has %s"), Sex, Morph), Base->FindMorphTarget(Morph));
		}
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
	TArray<FString> Expected = {ShinobiMutable::Body, ShinobiMutable::Build, ShinobiMutable::BuildBlend};
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
			TestFalse(FString::Printf(TEXT("%s.%s has both sexes' meshes"), Slot, *Row.ToString()), Piece.MaleMesh.IsNull() || Piece.FemaleMesh.IsNull());
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
