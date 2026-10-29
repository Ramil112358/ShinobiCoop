#include "ShinobiTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR // (run in the editor only: Mutable graphs and compiles are editor-only)

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorFunctionLibrary.h"
#include "MuCOE/CustomizableObjectFactory.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "Shinobi/Hero/ShinobiMutableGraphLibrary.h"
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

#endif
