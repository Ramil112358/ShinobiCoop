#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: Mutable graphs and compiles are editor-only)

#include "Algo/Count.h"
#include "Algo/Find.h"
#include "AnimationRuntime.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Components/LODSyncComponent.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	TestTrue(TEXT("a 12-colour palette after Original"), Palette && Palette->GetRowMap().Num() == 13 && Palette->GetRowNames()[0] == TEXT("Original"));
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
			// (City Sample pieces reshape from their fit morph onto the own body since P3.)
			TestTrue(FString::Printf(TEXT("%s wears the first piece of each slot"), Entry.Name), Look->Top == 0 && Look->Bottom == 0 && Look->Shoes == 0);
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


namespace
{
	/** The roster preset of Name from the catalog (a copy; asserts it exists). */
	TOptional<FShinobiAppearance> Preset(FAutomationTestBase& Test, const TCHAR* Name)
	{
		const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
		const FShinobiAppearance* Look = Catalog ? Catalog->FindPreset(Name) : nullptr;
		Test.TestNotNull(FString::Printf(TEXT("%s's preset"), Name), Look);
		return Look ? TOptional<FShinobiAppearance>(*Look) : TOptional<FShinobiAppearance>();
	}

	/** Whether any body material slot carries the skin tint Tint. */
	bool BodyTinted(const UShinobiBodyComponent* Body, const FLinearColor& Tint)
	{
		for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index)
		{
			const UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(Index));
			FLinearColor Value;
			if (Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(ShinobiMutable::SkinTintParameter), Value) && Value.Equals(Tint, 0.001f))
			{
				return true;
			}
		}
		return false;
	}

	bool WearsMetaHumanFace(const UShinobiBodyComponent* Body)
	{
		return Body->GetMetaHumanParts().ContainsByPredicate([](const UActorComponent* Part) { return Part && Part->GetName().StartsWith(TEXT("Face")); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableStaticHeadWornTest, "ShinobiCoop.Mutable.StaticHeadWorn", ShinobiTestFlags)
bool FShinobiMutableStaticHeadWornTest::RunTest(const FString& Parameters)
{
	const TOptional<FShinobiAppearance> Cat = Preset(*this, TEXT("Cat"));
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	const FShinobiCreatorHead* Head = Cat && Catalog ? Catalog->FindHead(Cat->Head) : nullptr;
	if (!TestNotNull(TEXT("Cat's head"), Head))
	{
		return false;
	}
	const FLinearColor Tint = Head->SkinTint;
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	TestTrue(TEXT("Cat's preset is accepted"), Wearer->Body->WearAppearance(*Cat));
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, Tint]()
	{
		const USkeletalMeshComponent* Static = Wearer->Body->GetStaticHead();
		TestTrue(TEXT("wears SK_Head_Cat"), Static && Static->GetSkeletalMeshAsset() && Static->GetSkeletalMeshAsset()->GetName() == TEXT("SK_Head_Cat"));
		TestEqual(TEXT("no MetaHuman parts"), Wearer->Body->GetMetaHumanParts().Num(), 0);
		TestTrue(FString::Printf(TEXT("the body is tinted %s"), *Tint.ToString()), BodyTinted(Wearer->Body, Tint));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableOneHeadAtATimeTest, "ShinobiCoop.Mutable.OneHeadAtATime", ShinobiTestFlags)
bool FShinobiMutableOneHeadAtATimeTest::RunTest(const FString& Parameters)
{
	const TOptional<FShinobiAppearance> Cat = Preset(*this, TEXT("Cat"));
	const TOptional<FShinobiAppearance> RamilMH = Preset(*this, TEXT("RamilMH"));
	if (!Cat || !RamilMH)
	{
		return false;
	}
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	auto Check = [this, Wearer](const TCHAR* Step)
	{
		const USkeletalMeshComponent* Static = Wearer->Body->GetStaticHead();
		const bool bStatic = Static && Static->GetSkeletalMeshAsset();
		TestTrue(FString::Printf(TEXT("%s: exactly one head (static %d, MetaHuman face %d)"), Step, bStatic, WearsMetaHumanFace(Wearer->Body)), bStatic != WearsMetaHumanFace(Wearer->Body));
	};
	const FShinobiAppearance Looks[] = {*Cat, *RamilMH, *Cat};
	const TCHAR* Steps[] = {TEXT("Cat"), TEXT("then RamilMH"), TEXT("then Cat")};
	for (int32 Step = 0; Step < 3; ++Step)
	{
		const FShinobiAppearance Look = Looks[Step];
		const TCHAR* Name = Steps[Step];
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Wearer, Look]() { Wearer->Body->WearAppearance(Look); return true; }));
		WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [Check, Name]() { Check(Name); });
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableRosterPresetWornTest, "ShinobiCoop.Mutable.RosterPresetWorn", ShinobiTestFlags)
bool FShinobiMutableRosterPresetWornTest::RunTest(const FString& Parameters)
{
	TSharedRef<FShinobiTestWorld> World = MakeShared<FShinobiTestWorld>();
	const uint8 Cat = uint8(AShinobiPlayerCharacter::FindRosterIndex(TEXT("Cat")));
	AShinobiPlayerCharacter* A = World->Spawn<AShinobiPlayerCharacter>();
	AShinobiPlayerCharacter* B = World->Spawn<AShinobiPlayerCharacter>();
	IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.RosterPresets"));
	const int32 Previous = Switch ? Switch->GetInt() : 0;
	if (!TestNotNull(TEXT("Shinobi.RosterPresets"), Switch) || !TestNotNull(TEXT("A's body"), A->GetHeroMesh()))
	{
		return false;
	}
	Switch->Set(1, ECVF_SetByCode);
	A->SetCharacterIndex(Cat);
	B->SetCharacterIndex(Cat);
	auto BodyOf = [](AShinobiPlayerCharacter* P) { return CastChecked<UShinobiBodyComponent>(P->GetHeroMesh()); };
	WaitThen(*this, [World, A, B, BodyOf]() { return !BodyOf(A)->IsMutableBuildPending() && !BodyOf(B)->IsMutableBuildPending(); },
		[this, World, A, B, BodyOf, Switch, Previous]()
	{
		Switch->Set(Previous, ECVF_SetByCode);
		const UShinobiBodyComponent* Body = BodyOf(A);
		const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
		TestTrue(TEXT("Cat wears a generated body"), Mesh && Mesh->GetOutermost() == GetTransientPackage());
		const USkeletalMeshComponent* Head = Body->GetStaticHead();
		TestTrue(TEXT("with SK_Head_Cat"), Head && Head->GetSkeletalMeshAsset() && Head->GetSkeletalMeshAsset()->GetName() == TEXT("SK_Head_Cat"));
		TestEqual(TEXT("two Cats look the same"), BodyOf(B)->GetAppearance().ToCode(), Body->GetAppearance().ToCode());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableApprovedPresetsWornTest, "ShinobiCoop.Mutable.ApprovedPresetsWorn", ShinobiTestFlags)
bool FShinobiMutableApprovedPresetsWornTest::RunTest(const FString& Parameters)
{
	// Shinobi.RosterPresets at its default: the characters the user approved wear their preset (a static head's body tinted
	// to its neck, outfit dressed or not); the rest keep their own body.
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	if (!TestNotNull(TEXT("the creator catalog"), Catalog))
	{
		return false;
	}
	TSharedRef<FShinobiTestWorld> World = MakeShared<FShinobiTestWorld>();
	TArray<TPair<FString, AShinobiPlayerCharacter*>> Players;
	for (const TCHAR* Name : {TEXT("Ramil"), TEXT("Elvin"), TEXT("RamilMH"), TEXT("BikaMH"), TEXT("Hero"), TEXT("Bika"), TEXT("Cat"), TEXT("Misha"), TEXT("Pabu")})
	{
		AShinobiPlayerCharacter* P = World->Spawn<AShinobiPlayerCharacter>();
		const int32 Index = AShinobiPlayerCharacter::FindRosterIndex(Name);
		P->SetCharacterIndex(uint8(Index));
		// Seen at once: its own body stands in while the preset builds (and stays if the build fails).
		TestEqual(FString(Name) + TEXT(" shows its own body at once"), GetPathNameSafe(P->GetHeroMesh()->GetSkeletalMeshAsset()), FString(AShinobiPlayerCharacter::Roster[Index].Mesh));
		Players.Emplace(Name, P);
	}
	auto BodyOf = [](AShinobiPlayerCharacter* P) { return CastChecked<UShinobiBodyComponent>(P->GetHeroMesh()); };
	WaitThen(*this, [World, Players, BodyOf]()
	{
		return !Players.ContainsByPredicate([BodyOf](const auto& Each) { return BodyOf(Each.Value)->IsMutableBuildPending(); });
	}, [this, World, Players, BodyOf, Catalog]()
	{
		for (const auto& [Name, P] : Players)
		{
			const UShinobiBodyComponent* Body = BodyOf(P);
			const USkeletalMesh* Mesh = Body->GetSkeletalMeshAsset();
			const int32 Index = AShinobiPlayerCharacter::FindRosterIndex(Name);
			if (Name == TEXT("Ramil") || Name == TEXT("Elvin") || Name == TEXT("RamilMH") || Name == TEXT("BikaMH"))
			{
				TestTrue(Name + TEXT(" wears its preset's generated body"), Mesh && Mesh->GetOutermost() == GetTransientPackage());
				const FShinobiAppearance* Look = Catalog->FindPreset(*Name);
				if (const FShinobiCreatorHead* Head = Look ? Catalog->FindHead(Look->Head) : nullptr)
				{
					TestTrue(FString::Printf(TEXT("%s's body is tinted %s"), *Name, *Head->SkinTint.ToString()), BodyTinted(Body, Head->SkinTint));
					const USkeletalMeshComponent* Static = Body->GetStaticHead();
					TestTrue(Name + TEXT("'s head is outlined as its body"), Static && Body->bRenderCustomDepth && Static->bRenderCustomDepth
						&& Static->CustomDepthStencilValue == Body->CustomDepthStencilValue);
				}
			}
			else
			{
				TestEqual(Name + TEXT(" keeps its own body"), GetPathNameSafe(Mesh), FString(AShinobiPlayerCharacter::Roster[Index].Mesh));
			}
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutablePresetTintStaysTest, "ShinobiCoop.Mutable.PresetTintStaysWithItsBody", ShinobiTestFlags)
bool FShinobiMutablePresetTintStaysTest::RunTest(const FString& Parameters)
{
	// Elvin's preset (a body tinted to his neck), then BikaMH's own MetaHuman body: Elvin's tint must not come along.
	const TOptional<FShinobiAppearance> Elvin = Preset(*this, TEXT("Elvin"));
	const UShinobiCreatorCatalog* Catalog = LoadObject<UShinobiCreatorCatalog>(nullptr, UShinobiCreatorCatalog::Path);
	const FShinobiCreatorHead* Head = Elvin && Catalog ? Catalog->FindHead(Elvin->Head) : nullptr;
	IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.RosterPresets"));
	if (!TestNotNull(TEXT("Elvin's head"), Head) || !TestNotNull(TEXT("Shinobi.RosterPresets"), Switch))
	{
		return false;
	}
	const FLinearColor Tint = Head->SkinTint;
	const int32 Previous = Switch->GetInt();
	TSharedRef<FShinobiTestWorld> World = MakeShared<FShinobiTestWorld>();
	AShinobiPlayerCharacter* P = World->Spawn<AShinobiPlayerCharacter>();
	UShinobiBodyComponent* Body = CastChecked<UShinobiBodyComponent>(P->GetHeroMesh());
	Switch->Set(1, ECVF_SetByCode);
	P->SetCharacterIndex(uint8(AShinobiPlayerCharacter::FindRosterIndex(TEXT("Elvin"))));
	WaitThen(*this, [Body]() { return !Body->IsMutableBuildPending(); }, [this, World, P, Body, Switch, Previous, Tint]()
	{
		TestTrue(TEXT("Elvin's preset is tinted"), BodyTinted(Body, Tint));
		Switch->Set(0, ECVF_SetByCode);
		P->SetCharacterIndex(uint8(AShinobiPlayerCharacter::FindRosterIndex(TEXT("BikaMH"))));
		Switch->Set(Previous, ECVF_SetByCode);
		TestTrue(TEXT("BikaMH wears her own body"), GetPathNameSafe(Body->GetSkeletalMeshAsset()).Contains(TEXT("BikaMH")));
		TestFalse(TEXT("without Elvin's tint"), BodyTinted(Body, Tint));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableRosterPresetFallsBackTest, "ShinobiCoop.Mutable.RosterPresetFallsBack", ShinobiTestFlags)
bool FShinobiMutableRosterPresetFallsBackTest::RunTest(const FString& Parameters)
{
	FShinobiTestWorld World;
	const int32 Cat = AShinobiPlayerCharacter::FindRosterIndex(TEXT("Cat"));
	AShinobiPlayerCharacter* P = World.Spawn<AShinobiPlayerCharacter>();
	IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("Shinobi.RosterPresets"));
	if (!TestNotNull(TEXT("Shinobi.RosterPresets"), Switch) || !TestNotNull(TEXT("the body"), P->GetHeroMesh()))
	{
		return false;
	}
	const int32 Previous = Switch->GetInt();
	Switch->Set(1, ECVF_SetByCode);
	UShinobiMutableLook::ObjectPathOverride = TEXT("/Game/Shinobi/Mutable/CO_MissingRoster.CO_MissingRoster");
	AddExpectedError(TEXT("CO_MissingRoster"), EAutomationExpectedErrorFlags::Contains, 1);
	P->SetCharacterIndex(uint8(Cat));
	UShinobiMutableLook::ObjectPathOverride.Reset();
	Switch->Set(Previous, ECVF_SetByCode);
	TestEqual(TEXT("Cat falls back to its legacy body"), GetPathNameSafe(P->GetHeroMesh()->GetSkeletalMeshAsset()), FString(AShinobiPlayerCharacter::Roster[Cat].Mesh));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableSameBodyNewHeadTest, "ShinobiCoop.Mutable.SameBodyNewHead", ShinobiTestFlags)
bool FShinobiMutableSameBodyNewHeadTest::RunTest(const FString& Parameters)
{
	// Bika and Cat share a body (female, Normal): Mutable skips the second, identical build, and the head must still change.
	const TOptional<FShinobiAppearance> Bika = Preset(*this, TEXT("Bika"));
	const TOptional<FShinobiAppearance> Cat = Preset(*this, TEXT("Cat"));
	if (!Bika || !Cat)
	{
		return false;
	}
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->WearAppearance(*Bika);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [Wearer, Cat]() { Wearer->Body->WearAppearance(*Cat); });
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer]()
	{
		const USkeletalMeshComponent* Head = Wearer->Body->GetStaticHead();
		TestTrue(TEXT("Cat's head after Bika's"), Head && Head->GetSkeletalMeshAsset() && Head->GetSkeletalMeshAsset()->GetName() == TEXT("SK_Head_Cat"));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableMetaHumanPresetPartsTest, "ShinobiCoop.Mutable.MetaHumanPresetParts", ShinobiTestFlags)
bool FShinobiMutableMetaHumanPresetPartsTest::RunTest(const FString& Parameters)
{
	// RamilMH's preset wears his blueprint's own parts, as his roster body does: the Fab hair (strands, ShinobiStrands)
	// and the LODSync that keeps the grooms on the face's LODs.
	const TOptional<FShinobiAppearance> RamilMH = Preset(*this, TEXT("RamilMH"));
	if (!RamilMH)
	{
		return false;
	}
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->WearAppearance(*RamilMH);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer]()
	{
		const TArray<TObjectPtr<UActorComponent>>& Parts = Wearer->Body->GetMetaHumanParts();
		TestTrue(TEXT("the Fab hair"), Parts.ContainsByPredicate([](const UActorComponent* Part) { return Part && Part->ComponentHasTag(TEXT("ShinobiStrands")); }));
		TestTrue(TEXT("the LODSync"), Parts.ContainsByPredicate([](const UActorComponent* Part) { return Part && Part->IsA<ULODSyncComponent>(); }));
		// On his own body, his own retargeter (the crowd's pitched his head differently from his roster look).
		TestEqual(TEXT("RamilMH's retargeter"), GetPathNameSafe(Wearer->Body->GetRetargeter()), FString(AShinobiPlayerCharacter::Roster[AShinobiPlayerCharacter::FindRosterIndex(TEXT("RamilMH"))].Retargeter));
		// Every groom stays on the head (a groom bound wrong stretches its strands across the scene).
		for (const UActorComponent* Part : Parts)
		{
			if (const UPrimitiveComponent* Groom = Cast<UPrimitiveComponent>(Part); Groom && Part->GetClass()->GetName() == TEXT("GroomComponent"))
			{
				TestTrue(FString::Printf(TEXT("%s stays on the head (bounds radius %.0f cm)"), *Part->GetName(), Groom->Bounds.SphereRadius), Groom->Bounds.SphereRadius < 60.f);
			}
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableOwnHeadUndistortedTest, "ShinobiCoop.Mutable.OwnHeadUndistorted", ShinobiTestFlags)
bool FShinobiMutableOwnHeadUndistortedTest::RunTest(const FString& Parameters)
{
	// The Own build's face bones must sit as on the own body: grooms ride the face's skin, so a turned or scaled head
	// bone flings the hair across the scene.
	struct FCase { EShinobiSex Sex; const TCHAR* Own; };
	const FCase Cases[] = {
		{EShinobiSex::Male, TEXT("/Game/Shinobi/MetaHumans/RamilMH/Build/MHC_RamilMH/Body/SKM_MHC_RamilMH_BodyMesh.SKM_MHC_RamilMH_BodyMesh")},
		{EShinobiSex::Female, TEXT("/Game/Shinobi/MetaHumans/BikaMH/Build/MHC_BikaMH/Body/SKM_MHC_BikaMH_BodyMesh.SKM_MHC_BikaMH_BodyMesh")}};
	for (const FCase& Case : Cases)
	{
		const USkeletalMesh* Own = LoadObject<USkeletalMesh>(nullptr, Case.Own);
		TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
		FShinobiAppearance Look = MutableLook(Case.Sex, EShinobiBuild::Own);
		Look.BuildBlend = 255;
		Wearer->Body->WearAppearance(Look);
		WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, Own]()
		{
			const USkeletalMesh* Built = Wearer->Body->GetSkeletalMeshAsset();
			if (!TestTrue(TEXT("built"), Built && Own))
			{
				return;
			}
			for (const TCHAR* Bone : {TEXT("spine_05"), TEXT("neck_01"), TEXT("neck_02"), TEXT("head")})
			{
				const int32 A = Own->GetRefSkeleton().FindBoneIndex(Bone), B = Built->GetRefSkeleton().FindBoneIndex(Bone);
				if (!TestTrue(FString::Printf(TEXT("%s in both"), Bone), A != INDEX_NONE && B != INDEX_NONE))
				{
					continue;
				}
				const FTransform Want = FAnimationRuntime::GetComponentSpaceTransformRefPose(Own->GetRefSkeleton(), A);
				const FTransform Got = FAnimationRuntime::GetComponentSpaceTransformRefPose(Built->GetRefSkeleton(), B);
				const float Degrees = FMath::RadiansToDegrees(Want.GetRotation().AngularDistance(Got.GetRotation()));
				TestTrue(FString::Printf(TEXT("%s %s turned %.1f deg"), *Own->GetName(), Bone, Degrees), Degrees < 3.f);
				TestTrue(FString::Printf(TEXT("%s %s scale %s"), *Own->GetName(), Bone, *Got.GetScale3D().ToString()), Got.GetScale3D().Equals(Want.GetScale3D(), 0.02f));
				TestTrue(FString::Printf(TEXT("%s %s moved %.1f cm"), *Own->GetName(), Bone, FVector::Dist(Want.GetLocation(), Got.GetLocation())), FVector::Dist(Want.GetLocation(), Got.GetLocation()) < 1.5f);
			}
		});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableOwnFaceBonesTest, "ShinobiCoop.Mutable.OwnFaceBones", ShinobiTestFlags)
bool FShinobiMutableOwnFaceBonesTest::RunTest(const FString& Parameters)
{
	// The face copies every bone it shares with the body: on the Own build those sit as on RamilMH's roster body.
	const USkeletalMesh* Face = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Shinobi/MetaHumans/RamilMH/Build/MHC_RamilMH/Face/SKM_MHC_RamilMH_FaceMesh.SKM_MHC_RamilMH_FaceMesh"));
	const USkeletalMesh* Legacy = LoadObject<USkeletalMesh>(nullptr, AShinobiPlayerCharacter::Roster[AShinobiPlayerCharacter::FindRosterIndex(TEXT("RamilMH"))].Mesh);
	const TOptional<FShinobiAppearance> RamilMH = Preset(*this, TEXT("RamilMH"));
	if (!TestTrue(TEXT("RamilMH's face and body"), Face && Legacy) || !RamilMH)
	{
		return false;
	}
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->WearAppearance(*RamilMH);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, Face, Legacy]()
	{
		const USkeletalMesh* Built = Wearer->Body->GetSkeletalMeshAsset();
		if (!TestNotNull(TEXT("built"), Built))
		{
			return;
		}
		const FReferenceSkeleton& Bones = Built->GetRefSkeleton();
		for (int32 Bone = 0; Bone < Bones.GetNum(); ++Bone)
		{
			const FName Name = Bones.GetBoneName(Bone);
			const int32 InLegacy = Legacy->GetRefSkeleton().FindBoneIndex(Name);
			if (Face->GetRefSkeleton().FindBoneIndex(Name) == INDEX_NONE || InLegacy == INDEX_NONE)
			{
				continue;
			}
			const float Distance = FVector::Dist(FAnimationRuntime::GetComponentSpaceTransformRefPose(Bones, Bone).GetLocation(),
				FAnimationRuntime::GetComponentSpaceTransformRefPose(Legacy->GetRefSkeleton(), InLegacy).GetLocation());
			TestTrue(FString::Printf(TEXT("%s %.1f cm from the roster body's"), *Name.ToString(), Distance), Distance < 0.5f);
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableLODSyncFindsPartsTest, "ShinobiCoop.Mutable.LODSyncFindsParts", ShinobiTestFlags)
bool FShinobiMutableLODSyncFindsPartsTest::RunTest(const FString& Parameters)
{
	// RamilMH's roster look, then his preset: the preset's parts are made while the last ones (destroyed, not yet
	// collected) still hold their names, and the LODSync must still find them (it drives the brows onto their cards).
	const TOptional<FShinobiAppearance> RamilMH = Preset(*this, TEXT("RamilMH"));
	if (!RamilMH)
	{
		return false;
	}
	const FShinobiRosterEntry& Roster = AShinobiPlayerCharacter::Roster[AShinobiPlayerCharacter::FindRosterIndex(TEXT("RamilMH"))];
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->Wear(Roster.Mesh, Roster.Retargeter, Roster.MetaHuman);
	Wearer->Body->WearAppearance(*RamilMH);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer]()
	{
		const TArray<TObjectPtr<UActorComponent>>& Parts = Wearer->Body->GetMetaHumanParts();
		const TObjectPtr<UActorComponent>* Sync = Parts.FindByPredicate([](const UActorComponent* Part) { return Part && Part->IsA<ULODSyncComponent>(); });
		if (!TestNotNull(TEXT("the LODSync"), Sync))
		{
			return;
		}
		// Every worn part the LODSync names (by its blueprint name) is named as it was placed, here and in the LOD mappings.
		const ULODSyncComponent* LODSync = CastChecked<ULODSyncComponent>(*Sync);
		int32 Checked = 0;
		for (const UActorComponent* Part : Parts)
		{
			const FString Plain = Part ? Part->GetFName().GetPlainNameString() : FString();
			for (const FComponentSync& Entry : LODSync->ComponentsToSync)
			{
				if (Part && Part != LODSync && Entry.Name.GetPlainNameString() == Plain)
				{
					++Checked;
					TestEqual(FString::Printf(TEXT("the LODSync's %s"), *Plain), Entry.Name, Part->GetFName());
				}
			}
			for (const TPair<FName, FLODMappingData>& Mapping : LODSync->CustomLODMapping)
			{
				if (Part && Mapping.Key.GetPlainNameString() == Plain)
				{
					TestEqual(FString::Printf(TEXT("the LOD mapping of %s"), *Plain), Mapping.Key, Part->GetFName());
				}
			}
		}
		TestTrue(TEXT("the face and grooms are synced"), Checked >= 5);
	});
	return true;
}

namespace
{
	/** Whether Mesh has morph target Name. */
	bool HasMorph(const USkeletalMesh* Mesh, const TCHAR* Name)
	{
		return Mesh && Mesh->FindMorphTarget(Name) != nullptr;
	}

	/** The meshes of a slot's wardrobe rows for a sex (City Sample rows only: CitySampleNN). */
	TArray<USkeletalMesh*> CitySamplePieces(const TCHAR* Slot, bool bMale)
	{
		TArray<USkeletalMesh*> Pieces;
		if (const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot)))
		{
			Table->ForeachRow<FShinobiWardrobeRow>(TEXT("fit test"), [&](const FName& Name, const FShinobiWardrobeRow& Row)
			{
				if (USkeletalMesh* Mesh = Name.ToString().StartsWith(TEXT("CitySample")) ? (bMale ? Row.MaleMesh : Row.FemaleMesh).LoadSynchronous() : nullptr)
				{
					Pieces.Add(Mesh);
				}
			});
		}
		return Pieces;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableFitMorphsOnBasesTest, "ShinobiCoop.Mutable.FitMorphsOnBases", ShinobiTestFlags)
bool FShinobiMutableFitMorphsOnBasesTest::RunTest(const FString& Parameters)
{
	// A piece is reshaped from its source's fit morph of the base (Scripts/setup_mutable.py stage fits) to the built body:
	// the base keeps the MetaHuman template's 32334 render vertices (Scripts/mutable/base_bodies.py), so both shapes share
	// Reshape's topology.
	for (const TCHAR* Sex : {TEXT("Male"), TEXT("Female")})
	{
		const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(Sex));
		if (!TestNotNull(TEXT("the base"), Base))
		{
			continue;
		}
		TestEqual(Base->GetName() + TEXT(": the template's vertex count"), int32(Base->GetResourceForRendering()->LODRenderData[0].GetNumVertices()), 32334);
		for (const TCHAR* Source : {TEXT("CitySample"), TEXT("MHSample")})
		{
			TestTrue(FString::Printf(TEXT("%s has Fit_%s"), *Base->GetName(), Source), HasMorph(Base, *(FString(TEXT("Fit_")) + Source)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableFitShapeHugsSourceTest, "ShinobiCoop.Mutable.FitShapeHugsSource", ShinobiTestFlags)
bool FShinobiMutableFitShapeHugsSourceTest::RunTest(const FString& Parameters)
{
	// City Sample tops and bottoms were made on the legacy MetaHuman tal_nrw bodies: they sit outside the fit shape baked
	// from those bodies, and clip into the plain base (the shoulders and crotch the P2 sheets showed).
	for (const bool bMale : {true, false})
	{
		const TCHAR* Sex = bMale ? TEXT("Male") : TEXT("Female");
		USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(Sex));
		if (!TestNotNull(TEXT("the base"), Base) || !TestTrue(TEXT("the base has Fit_CitySample"), HasMorph(Base, TEXT("Fit_CitySample"))))
		{
			continue;
		}
		for (const TCHAR* Slot : {TEXT("Top"), TEXT("Bottom")})
		{
			for (USkeletalMesh* Piece : CitySamplePieces(Slot, bMale))
			{
				const float OnFit = UShinobiMutableMeshLibrary::FractionInside(Piece, Base, 0.5f, TEXT("Fit_CitySample"));
				const float OnBase = UShinobiMutableMeshLibrary::FractionInside(Piece, Base);
				AddInfo(FString::Printf(TEXT("%s: %.1f%% inside its fit shape, %.1f%% inside the base"), *Piece->GetName(), OnFit * 100.f, OnBase * 100.f));
				// (a few percent stay inside: the base's vertices sample the source body, and tight cloth dips below the flat
				// triangles between them; 0-9% with or without smoothing)
				TestTrue(FString::Printf(TEXT("%s sits on its fit shape (%.1f%% inside)"), *Piece->GetName(), OnFit * 100.f), OnFit < 0.08f);
				TestTrue(FString::Printf(TEXT("%s fits its fit shape better than the base"), *Piece->GetName()), OnFit < OnBase);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableSlotFitParameterTest, "ShinobiCoop.Mutable.SlotFitParameter", ShinobiTestFlags)
bool FShinobiMutableSlotFitParameterTest::RunTest(const FString& Parameters)
{
	// Each slot's piece is reshaped from its source's fit morph of the base: the row's fit names it, no piece gets "None".
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex::Male;
	Look.Top = 0;
	Look.Outer = FShinobiAppearance::None;
	TStrongObjectPtr<UShinobiMutableLook> Builder(NewObject<UShinobiMutableLook>());
	if (!TestTrue(TEXT("the build starts"), Builder->Request(Look)))
	{
		return false;
	}
	WaitThen(*this, [Builder]() { return !Builder->IsPending(); }, [this, Builder]()
	{
		UCustomizableObjectInstance* Instance = Builder->GetInstance();
		if (TestNotNull(TEXT("the instance"), Instance))
		{
			TestEqual(TEXT("City Sample Top 0 reshapes from Fit_CitySample"), Instance->GetEnumParameterSelectedOption(TEXT("Top Fit")), FString(TEXT("CitySample")));
			TestEqual(TEXT("no Outer: no fit"), Instance->GetEnumParameterSelectedOption(TEXT("Outer Fit")), FString(TEXT("None")));
		}
	});
	return true;
}

namespace
{
	/** Every Top/Bottom/Shoes row's mesh and clip per sex: Visit(row name, sex, mesh, clip). */
	void ForEachRowPiece(TFunctionRef<void(const FString&, const TCHAR*, USkeletalMesh*, USkeletalMesh*)> Visit)
	{
		for (const TCHAR* Slot : {TEXT("Top"), TEXT("Bottom"), TEXT("Shoes")})
		{
			if (const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot)))
			{
				Table->ForeachRow<FShinobiWardrobeRow>(TEXT("coverage test"), [&](const FName& Name, const FShinobiWardrobeRow& Row)
				{
					const FString Who = FString(Slot) + TEXT(" ") + Name.ToString();
					Visit(Who, TEXT("Male"), Row.MaleMesh.LoadSynchronous(), Row.MaleClip.LoadSynchronous());
					Visit(Who, TEXT("Female"), Row.FemaleMesh.LoadSynchronous(), Row.FemaleClip.LoadSynchronous());
				});
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableRowsHaveClipsTest, "ShinobiCoop.Mutable.RowsHaveClips", ShinobiTestFlags)
bool FShinobiMutableRowsHaveClipsTest::RunTest(const FString& Parameters)
{
	// Every piece hides the skin it covers (Scripts/setup_mutable.py stage coverage).
	int32 Pieces = 0;
	ForEachRowPiece([&](const FString& Who, const TCHAR* Sex, USkeletalMesh* Mesh, USkeletalMesh* Clip)
	{
		if (Mesh)
		{
			TestNotNull(FString::Printf(TEXT("%s (%s) has a clip"), *Who, Sex), Clip);
			++Pieces;
		}
	});
	TestTrue(TEXT("some pieces"), Pieces > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableClipVolumesClosedTest, "ShinobiCoop.Mutable.ClipVolumesClosed", ShinobiTestFlags)
bool FShinobiMutableClipVolumesClosedTest::RunTest(const FString& Parameters)
{
	// Clip With Mesh removes what lies inside a closed volume: no clip has an open edge, and each is on the base's
	// skeleton (Reshape moves it from the piece's fit shape to the built body).
	const USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(TEXT("Male")));
	int32 Checked = 0;
	ForEachRowPiece([&](const FString& Who, const TCHAR* Sex, USkeletalMesh* Mesh, USkeletalMesh* Clip)
	{
		TArray<FVector> Positions;
		TArray<int32> Triangles;
		if (!Clip || !TestTrue(Who + TEXT(": clip surface"), UShinobiMutableMeshLibrary::ReadMeshSurface(Clip, Positions, Triangles)))
		{
			return;
		}
		TMap<TPair<int32, int32>, int32> Edges;
		for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
		{
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 A = Triangles[Index + Corner], B = Triangles[Index + (Corner + 1) % 3];
				++Edges.FindOrAdd(TPair<int32, int32>(FMath::Min(A, B), FMath::Max(A, B)));
			}
		}
		const int32 Open = Algo::CountIf(Edges, [](const TPair<TPair<int32, int32>, int32>& Edge) { return Edge.Value == 1; });
		const int32 Shared = Algo::CountIf(Edges, [](const TPair<TPair<int32, int32>, int32>& Edge) { return Edge.Value > 2; });
		// (an edge shared by 3+ triangles is a wall edge where the covered region pinches to one vertex: still closed)
		TestEqual(FString::Printf(TEXT("%s (%s): open edges (%d of %d edges at pinch points)"), *Who, Sex, Shared, Edges.Num()), Open, 0);
		TestTrue(Who + TEXT(": the base's skeleton"), Base && Clip->GetSkeleton() == Base->GetSkeleton());
		++Checked;
	});
	TestTrue(TEXT("some clips"), Checked > 0);
	return true;
}

namespace
{
	/** The index of palette row Name in DT_Palette, or -1. */
	int32 PaletteIndex(const TCHAR* Name)
	{
		const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath);
		return Palette ? Palette->GetRowNames().IndexOfByKey(FName(Name)) : -1;
	}

	/** The body material slots that wear a tinted copy of a piece's material (Scripts/setup_mutable.py "_Tint"). */
	TArray<const UMaterialInstanceDynamic*> TintedSlots(const UShinobiBodyComponent* Body)
	{
		TArray<const UMaterialInstanceDynamic*> Found;
		for (int32 Index = 0; Index < Body->GetNumMaterials(); ++Index)
		{
			const UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(Index));
			if (Material && Material->Parent && Material->Parent->GetName().EndsWith(TEXT("_Tint")))
			{
				Found.Add(Material);
			}
		}
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableSlotColourAppliedTest, "ShinobiCoop.Mutable.SlotColourApplied", ShinobiTestFlags)
bool FShinobiMutableSlotColourAppliedTest::RunTest(const FString& Parameters)
{
	// A slot's palette colour goes onto its piece's tint-enabled material copy (City Sample: A_CrowdColor_main).
	const int32 Red = PaletteIndex(TEXT("Red"));
	const UDataTable* Palette = LoadObject<UDataTable>(nullptr, ShinobiMutable::PalettePath);
	if (!TestTrue(TEXT("the palette has Red"), Red > 0 && Palette))
	{
		return false;
	}
	const FLinearColor Colour = Palette->FindRow<FShinobiPaletteRow>(TEXT("Red"), TEXT("test"))->Colour;
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex::Male;
	Look.Top = 0;
	Look.SlotColour[int32(EShinobiWardrobeSlot::Top)] = uint8(Red);
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	Wearer->Body->WearAppearance(Look);
	WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, Colour]()
	{
		const TArray<const UMaterialInstanceDynamic*> Tinted = TintedSlots(Wearer->Body);
		FLinearColor Value;
		TestTrue(TEXT("the top wears a tinted copy"), Tinted.Num() > 0);
		TestTrue(FString::Printf(TEXT("in red %s"), *Colour.ToString()), Tinted.ContainsByPredicate([&](const UMaterialInstanceDynamic* Material)
		{
			return Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("A_CrowdColor_main")), Value) && Value.Equals(Colour, 0.001f);
		}));
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableUntintedPieceUnchangedTest, "ShinobiCoop.Mutable.UntintedPieceUnchanged", ShinobiTestFlags)
bool FShinobiMutableUntintedPieceUnchangedTest::RunTest(const FString& Parameters)
{
	// Palette row 0 ("Original") and an index past the palette keep the piece's own materials.
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	for (const uint8 Colour : {uint8(0), uint8(200)})
	{
		FShinobiAppearance Look;
		Look.Top = 0;
		Look.BuildBlend = Colour; // (a different build each time, so Mutable doesn't skip the second as unchanged)
		Look.SlotColour[int32(EShinobiWardrobeSlot::Top)] = Colour;
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Wearer, Look]() { Wearer->Body->WearAppearance(Look); return true; }));
		WaitThen(*this, [Wearer]() { return !Wearer->Body->IsMutableBuildPending(); }, [this, Wearer, Colour]()
		{
			TestEqual(FString::Printf(TEXT("colour %d: no tinted copies"), Colour), TintedSlots(Wearer->Body).Num(), 0);
		});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableVersionOneRowsKeptTest, "ShinobiCoop.Mutable.VersionOneRowsKept", ShinobiTestFlags)
bool FShinobiMutableVersionOneRowsKeptTest::RunTest(const FString& Parameters)
{
	// Version-1 looks index City Sample styles: new sources are appended after the 12 tops, 6 bottoms and 3 shoes.
	for (const TPair<const TCHAR*, int32>& Slot : {TPair<const TCHAR*, int32>(TEXT("Top"), 12), {TEXT("Bottom"), 6}, {TEXT("Shoes"), 3}})
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot.Key));
		const TArray<FName> Rows = Table ? Table->GetRowNames() : TArray<FName>();
		if (!TestTrue(FString::Printf(TEXT("%s keeps its %d City Sample rows"), Slot.Key, Slot.Value), Rows.Num() >= Slot.Value))
		{
			continue;
		}
		for (int32 Index = 0; Index < Slot.Value; ++Index)
		{
			TestEqual(FString::Printf(TEXT("%s row %d"), Slot.Key, Index), Rows[Index].ToString(), FString::Printf(TEXT("CitySample%02d"), Index + 1));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableSamplePiecesTest, "ShinobiCoop.Mutable.SamplePiecesOnMetaHumanSkeleton", ShinobiTestFlags)
bool FShinobiMutableSamplePiecesTest::RunTest(const FString& Parameters)
{
	// The Mutable Sample's Cyborg and MetaHuman clothing and the Fab mannequin jacket: on the MetaHuman skeleton (Mutable
	// merges by bone), with a fit, sitting on that fit shape.
	TMap<FString, int32> Sources;
	for (const TCHAR* Slot : {TEXT("Top"), TEXT("Bottom"), TEXT("Shoes"), TEXT("Outer")})
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot));
		if (!Table)
		{
			continue;
		}
		Table->ForeachRow<FShinobiWardrobeRow>(TEXT("sample test"), [&](const FName& Name, const FShinobiWardrobeRow& Row)
		{
			const FString Who = FString(Slot) + TEXT(" ") + Name.ToString();
			for (const bool bMale : {true, false})
			{
				USkeletalMesh* Mesh = (bMale ? Row.MaleMesh : Row.FemaleMesh).LoadSynchronous();
				const FName Fit = bMale ? Row.MaleFit : Row.FemaleFit; // (none: rebound onto the base already)
				static const TArray<FString> Prefixes = {TEXT("Cyborg"), TEXT("MHSample"), TEXT("Mannequin"), TEXT("Fab")};
				const FString* Source = Algo::FindByPredicate(Prefixes, [&](const FString& Prefix) { return Name.ToString().StartsWith(Prefix); });
				if (!Mesh || !Source)
				{
					continue;
				}
				++Sources.FindOrAdd(*Source);
				USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(bMale ? TEXT("Male") : TEXT("Female")));
				TestTrue(Who + TEXT(": on the MetaHuman skeleton"), Mesh->GetRefSkeleton().FindBoneIndex(TEXT("spine_05")) != INDEX_NONE
					&& Mesh->GetRefSkeleton().FindBoneIndex(TEXT("hand_l")) != INDEX_NONE);
				const float Inside = Base ? UShinobiMutableMeshLibrary::FractionInside(Mesh, Base, 0.5f, Fit.IsNone() ? NAME_None : FName(*(TEXT("Fit_") + Fit.ToString()))) : 1.f;
				AddInfo(FString::Printf(TEXT("%s (%s, fit %s): %.1f%% inside"), *Who, bMale ? TEXT("male") : TEXT("female"), *Fit.ToString(), Inside * 100.f));
				// (a piece rebound onto the base keeps its collar, hood and cuffs where its own body's neck and hands were,
				// partly inside the base's there, under its coverage volume: judged by the wardrobe shots; 0-20% measured)
				const float Bar = Fit.IsNone() ? 0.25f : 0.08f;
				TestTrue(FString::Printf(TEXT("%s sits on its fit shape (%.1f%% inside)"), *Who, Inside * 100.f), Inside >= 0.f && Inside < Bar);
			}
		});
	}
	for (const TCHAR* Source : {TEXT("Cyborg"), TEXT("MHSample"), TEXT("Mannequin"), TEXT("Fab")})
	{
		TestTrue(FString::Printf(TEXT("rows from %s"), Source), Sources.Contains(Source));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableRowReplacesSlotsTest, "ShinobiCoop.Mutable.RowReplacesSlots", ShinobiTestFlags)
bool FShinobiMutableRowReplacesSlotsTest::RunTest(const FString& Parameters)
{
	// A whole outfit (a Fab outfit in Outer) takes the slots it replaces off: no jeans through its trousers.
	const UDataTable* Outer = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(TEXT("Outer")));
	const int32 Survivor = Outer ? Outer->GetRowNames().IndexOfByKey(FName(TEXT("FabSurvivor"))) : INDEX_NONE;
	if (!TestTrue(TEXT("Outer has FabSurvivor"), Survivor != INDEX_NONE))
	{
		return false;
	}
	const FShinobiWardrobeRow* Row = Outer->FindRow<FShinobiWardrobeRow>(TEXT("FabSurvivor"), TEXT("test"));
	TestTrue(TEXT("it replaces Top, Bottom and Shoes"), Row->Replaces.Contains(EShinobiWardrobeSlot::Top) && Row->Replaces.Contains(EShinobiWardrobeSlot::Bottom)
		&& Row->Replaces.Contains(EShinobiWardrobeSlot::Shoes));
	FShinobiAppearance Look;
	Look.Sex = Row->MaleMesh.IsNull() ? EShinobiSex::Female : EShinobiSex::Male;
	Look.Top = Look.Bottom = Look.Shoes = 0;
	Look.Outer = uint8(Survivor);
	TStrongObjectPtr<UShinobiMutableLook> Builder(NewObject<UShinobiMutableLook>());
	if (!TestTrue(TEXT("the build starts"), Builder->Request(Look)))
	{
		return false;
	}
	WaitThen(*this, [Builder]() { return !Builder->IsPending(); }, [this, Builder]()
	{
		UCustomizableObjectInstance* Instance = Builder->GetInstance();
		if (TestNotNull(TEXT("the instance"), Instance))
		{
			TestEqual(TEXT("Outer worn"), Instance->GetEnumParameterSelectedOption(TEXT("Outer")), FString(ShinobiMutable::On));
			for (const TCHAR* Slot : {TEXT("Top"), TEXT("Bottom"), TEXT("Shoes")})
			{
				TestEqual(FString::Printf(TEXT("%s replaced"), Slot), Instance->GetEnumParameterSelectedOption(Slot), FString(ShinobiMutable::Off));
			}
		}
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableClipUnderSkinTest, "ShinobiCoop.Mutable.ClipShellUnderTheSkin", ShinobiTestFlags)
bool FShinobiMutableClipUnderSkinTest::RunTest(const FString& Parameters)
{
	// A coverage volume reaches 0.3 cm out of its fit shape and 3 cm into it (so a slimmer built body's skin is still
	// inside it): no clip vertex lies more than 1 cm outside the fit surface.
	int32 Checked = 0;
	for (const TCHAR* Slot : {TEXT("Top"), TEXT("Bottom")})
	{
		const UDataTable* Table = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(Slot));
		const FShinobiWardrobeRow* Row = Table ? Table->FindRow<FShinobiWardrobeRow>(TEXT("CitySample01"), TEXT("test")) : nullptr;
		USkeletalMesh* Clip = Row ? Row->MaleClip.LoadSynchronous() : nullptr;
		USkeletalMesh* Base = LoadObject<USkeletalMesh>(nullptr, *ShinobiMutable::BasePath(TEXT("Male")));
		if (!TestNotNull(FString::Printf(TEXT("%s CitySample01's male clip"), Slot), Clip) || !Base)
		{
			continue;
		}
		const float Within = UShinobiMutableMeshLibrary::FractionInside(Clip, Base, -1.f, *(TEXT("Fit_") + Row->MaleFit.ToString()));
		TestTrue(FString::Printf(TEXT("%s: %.1f%% of the clip within 1 cm outside its fit shape"), Slot, Within * 100.f), Within > 0.95f); // (inside out: ~50%; a few rim vertices at concave spots measure just outside)
		++Checked;
	}
	TestEqual(TEXT("clips checked"), Checked, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShinobiMutableUnfittedUntintedRowTest, "ShinobiCoop.Mutable.UnfittedUntintedRow", ShinobiTestFlags)
bool FShinobiMutableUnfittedUntintedRowTest::RunTest(const FString& Parameters)
{
	// A row with no fit (a rebound Cyborg piece) reshapes from the base ("None"), and one without tinted copies keeps
	// its own materials whatever colour is asked for.
	const UDataTable* Tops = LoadObject<UDataTable>(nullptr, *ShinobiMutable::WardrobeTablePath(TEXT("Top")));
	const int32 Shirt = Tops ? Tops->GetRowNames().IndexOfByKey(FName(TEXT("CyborgTShirt"))) : INDEX_NONE;
	const int32 Red = PaletteIndex(TEXT("Red"));
	if (!TestTrue(TEXT("Top has CyborgTShirt and the palette Red"), Shirt != INDEX_NONE && Red > 0))
	{
		return false;
	}
	FShinobiAppearance Look;
	Look.Sex = EShinobiSex::Male;
	Look.Top = uint8(Shirt);
	Look.SlotColour[int32(EShinobiWardrobeSlot::Top)] = uint8(Red);
	TStrongObjectPtr<UShinobiMutableLook> Builder(NewObject<UShinobiMutableLook>());
	TSharedRef<FMutableWearer> Wearer = MakeShared<FMutableWearer>();
	if (!TestTrue(TEXT("the builds start"), Builder->Request(Look) && Wearer->Body->WearAppearance(Look)))
	{
		return false;
	}
	WaitThen(*this, [Builder, Wearer]() { return !Builder->IsPending() && !Wearer->Body->IsMutableBuildPending(); }, [this, Builder, Wearer]()
	{
		UCustomizableObjectInstance* Instance = Builder->GetInstance();
		TestEqual(TEXT("no fit"), Instance ? Instance->GetEnumParameterSelectedOption(TEXT("Top Fit")) : FString(), FString(ShinobiMutable::NoFit));
		TestEqual(TEXT("no tinted copies"), TintedSlots(Wearer->Body).Num(), 0);
	});
	return true;
}

#endif
