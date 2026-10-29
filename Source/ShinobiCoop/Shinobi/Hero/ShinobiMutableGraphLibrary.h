#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShinobiMutableGraphLibrary.generated.h"

class UCustomizableObject;
class UEdGraph;
class UEdGraphNode;

/**
 * Editor-only helper for Scripts/setup_mutable.py: builds a Mutable Customizable Object's graph from Python. Mutable's
 * node classes are private to its plugin, so nodes are made by class name and edited by reflection, and pins are joined
 * through the graph schema (Mutable's own connection rules apply). Outside the editor every function does nothing.
 *
 * Mutable 5.8 pin names (node class: pins), as wired in ShinobiCoop.Mutable.GraphHelper and the Mutable Sample:
 *   CustomizableObjectNodeSkeletalMesh: out "LOD <l> - Section <s> - Mesh" (set SkeletalMesh first; pins follow it)
 *   CONodeSkeletalMeshSection: in "Mesh_Input_Pin", out "Mesh Section_Output_Pin"
 *   CONodeSkeletalMeshMake_V2: in "LOD <l>", out "Skeletal Mesh"
 *   CONodeSkeletalMeshMorph: in "Skeletal Mesh", "Factor"; out "Skeletal Mesh"
 *   CONodeSwitch: in "Switch Parameter", one in per enum value; out "Skeletal Mesh"
 *   CustomizableObjectNodeSkeletalMeshParameter: out "Skeletal Mesh", "Passthrough Skeletal Mesh"
 *   CONodeClipSkeletalMeshWithSkeletalMesh: in "Base Skeletal Mesh", "Clip Skeletal Mesh"; out "Skeletal Mesh"
 *   CONodeSkeletalMeshMerge: in "Base", "Added"; out "Skeletal Mesh"
 *   CONodeSkeletalMeshObjectMake: in "Skeletal Mesh", out "Passthrough Skeletal Mesh"
 *   CONodeComponentSkeletalMesh (set ReferenceSkeletalMesh): in "Passthrough Skeletal Mesh", out "Component"
 *   CustomizableObjectNodeObject: in "Components", "Children"
 *   CustomizableObjectNodeMeshMorph: in "Mesh", "Factor", "Morph Target Name" (a StaticString node's "Value"); out "Mesh"
 *   CustomizableObjectNodeMeshReshape: in "Base Mesh", "Base Shape", "Target Shape"; out "Mesh"
 *   CONodeModifierMutableSkeletalMeshMerge: in "Added", "Skeletal Mesh Name"; out "Modifier"
 *   CustomizableObjectNodeModifierClipWithMesh: in "Clip Mesh"; out "Modifier"
 *   CustomizableObjectNodeObjectGroup: out "Group" (into Object "Children"), in "Objects"
 *   CustomizableObjectNodeObjectChild: out "Object", in "Modifiers"
 * Gotchas (ShinobiCoop.MutableSpike, 2026-10-29): a section node fed through a morph or reshape needs its Material set;
 * a merge modifier only merges into the object-make whose SkeletalMeshName equals its ParentSkeletalMeshName (hidden
 * property); a group is a set of toggles unless GroupType is COGT_ONE (then one Int parameter named GroupName whose
 * options are the children's ObjectName); Remove Mesh Blocks removed nothing on a script-built body (no layouts), Clip
 * With Mesh works. Creating a Customizable Object needs Slate: run scripts with Scripts/run_python.ps1 -Editor.
 * DescribeGraph prints any existing object's wiring (Scripts/mutable/describe_graphs.py).
 */
UCLASS()
class SHINOBICOOP_API UShinobiMutableGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The object's source graph (its reflected Source property), or null. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static UEdGraph* GetGraph(UCustomizableObject* Object);

	/** A new node of NodeClass (short class name, e.g. "CustomizableObjectNodeSkeletalMesh") at (X, Y), or null. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static UEdGraphNode* AddNode(UCustomizableObject* Object, const FString& NodeClass, int32 X, int32 Y);

	/** The Index-th node of NodeClass in the object's graph, or null. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static UEdGraphNode* FindNode(UCustomizableObject* Object, const FString& NodeClass, int32 Index = 0);

	/**
	 * Sets a node property from its text form (as the Details panel would), then rebuilds the node's pins. Property may be
	 * a dotted path into subobjects ("Layout.Blocks" for a remove-blocks node's layout).
	 */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool SetNodeProperty(UEdGraphNode* Node, const FString& Property, const FString& Value);

	/** A node property (or dotted path, as SetNodeProperty) in its text form; empty if there is none. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static FString GetNodeProperty(UEdGraphNode* Node, const FString& Property);

	/** Types Value into the node's unlinked input Pin (as in the graph editor); false if there is no such pin. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool SetPinDefault(UEdGraphNode* Node, const FString& Pin, const FString& Value);

	/** The node's pins as "<In|Out>|<PinName>|<PinCategory>". */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static TArray<FString> ListPins(UEdGraphNode* Node);

	/** Every node ("[i] Class key=value… Pin='typed value'…") and every link ("Class.Pin -> Class.Pin"), one per line: how a graph is wired. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static FString DescribeGraph(UCustomizableObject* Object);

	/** Joins From's output FromPin to To's input ToPin through the graph schema; false if either is missing or refused. */
	UFUNCTION(BlueprintCallable, Category = "Shinobi|Mutable")
	static bool LinkPins(UEdGraphNode* From, const FString& FromPin, UEdGraphNode* To, const FString& ToPin);
};
