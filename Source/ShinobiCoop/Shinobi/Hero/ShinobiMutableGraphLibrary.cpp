#include "Shinobi/Hero/ShinobiMutableGraphLibrary.h"

#include "ShinobiCoop.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "MuCO/CustomizableObject.h"

UEdGraph* UShinobiMutableGraphLibrary::GetGraph(UCustomizableObject* Object)
{
#if WITH_EDITOR
	const FObjectProperty* Source = CastField<FObjectProperty>(UCustomizableObject::StaticClass()->FindPropertyByName(TEXT("Source")));
	return Object && Source ? Cast<UEdGraph>(Source->GetObjectPropertyValue_InContainer(Object)) : nullptr;
#else
	return nullptr;
#endif
}

UEdGraphNode* UShinobiMutableGraphLibrary::AddNode(UCustomizableObject* Object, const FString& NodeClass, int32 X, int32 Y)
{
#if WITH_EDITOR
	UEdGraph* Graph = GetGraph(Object);
	UClass* Class = FindFirstObject<UClass>(*NodeClass, EFindFirstObjectOptions::ExactClass);
	if (!Graph || !Class || !Class->IsChildOf(UEdGraphNode::StaticClass()))
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("AddNode: no graph, or no node class %s"), *NodeClass);
		return nullptr;
	}
	// As the editor places a node from the palette (FEdGraphSchemaAction_NewNode).
	Graph->Modify();
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, Class, NAME_None, RF_Transactional);
	Graph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	return Node;
#else
	return nullptr;
#endif
}

UEdGraphNode* UShinobiMutableGraphLibrary::FindNode(UCustomizableObject* Object, const FString& NodeClass, int32 Index)
{
#if WITH_EDITOR
	if (const UEdGraph* Graph = GetGraph(Object))
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->GetClass()->GetName() == NodeClass && Index-- == 0)
			{
				return Node;
			}
		}
	}
#endif
	return nullptr;
}

bool UShinobiMutableGraphLibrary::SetNodeProperty(UEdGraphNode* Node, const FString& Property, const FString& Value)
{
#if WITH_EDITOR
	FProperty* Prop = Node ? Node->GetClass()->FindPropertyByName(FName(*Property)) : nullptr;
	if (!Prop)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("SetNodeProperty: no property %s on %s"), *Property, *GetNameSafe(Node));
		return false;
	}
	if (CastField<FObjectPropertyBase>(Prop) && !CastField<FSoftObjectProperty>(Prop) && !Value.IsEmpty() && Value != TEXT("None"))
	{
		LoadObject<UObject>(nullptr, *Value); // hard references only resolve objects already in memory
	}
	Node->PreEditChange(Prop);
	const bool bSet = Prop->ImportText_InContainer(*Value, Node, Node, PPF_None) != nullptr;
	FPropertyChangedEvent Changed(Prop, EPropertyChangeType::ValueSet);
	Node->PostEditChangeProperty(Changed);
	Node->ReconstructNode();
	return bSet;
#else
	return false;
#endif
}

TArray<FString> UShinobiMutableGraphLibrary::ListPins(UEdGraphNode* Node)
{
	TArray<FString> Pins;
#if WITH_EDITOR
	if (Node)
	{
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			Pins.Add(FString::Printf(TEXT("%s|%s|%s"), Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out"),
				*Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString()));
		}
	}
#endif
	return Pins;
}

FString UShinobiMutableGraphLibrary::DescribeGraph(UCustomizableObject* Object)
{
	FString Text;
#if WITH_EDITOR
	const UEdGraph* Graph = GetGraph(Object);
	if (!Graph)
	{
		return Text;
	}
	for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
	{
		const UEdGraphNode* Node = Graph->Nodes[Index];
		if (!Node)
		{
			continue;
		}
		// Edited properties only (those differing from the class default), as the Details panel would show them.
		FString Props;
		const UObject* Default = Node->GetClass()->GetDefaultObject();
		for (TFieldIterator<FProperty> It(Node->GetClass()); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Edit) && !It->Identical_InContainer(Node, Default))
			{
				FString Value;
				It->ExportText_InContainer(0, Value, Node, Default, nullptr, PPF_None);
				Props += FString::Printf(TEXT(" %s=%s"), *It->GetName(), *Value.Left(160));
			}
		}
		Text += FString::Printf(TEXT("[%d] %s%s\n"), Index, *Node->GetClass()->GetName(), *Props);
	}
	for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
	{
		const UEdGraphNode* Node = Graph->Nodes[Index];
		for (const UEdGraphPin* Pin : Node ? Node->Pins : TArray<UEdGraphPin*>())
		{
			if (Pin->Direction != EGPD_Output)
			{
				continue;
			}
			for (const UEdGraphPin* Other : Pin->LinkedTo)
			{
				const UEdGraphNode* To = Other->GetOwningNode();
				Text += FString::Printf(TEXT("%s.%s -> %s.%s [#%d -> #%d]\n"), *Node->GetClass()->GetName(), *Pin->PinName.ToString(),
					*To->GetClass()->GetName(), *Other->PinName.ToString(), Index, Graph->Nodes.IndexOfByKey(To));
			}
		}
	}
#endif
	return Text;
}

bool UShinobiMutableGraphLibrary::LinkPins(UEdGraphNode* From, const FString& FromPin, UEdGraphNode* To, const FString& ToPin)
{
#if WITH_EDITOR
	UEdGraphPin* Out = From ? From->FindPin(FName(*FromPin), EGPD_Output) : nullptr;
	UEdGraphPin* In = To ? To->FindPin(FName(*ToPin), EGPD_Input) : nullptr;
	const UEdGraphSchema* Schema = Out ? Out->GetSchema() : nullptr;
	if (!Out || !In || !Schema)
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("LinkPins: no pin %s.%s or %s.%s"), *GetNameSafe(From), *FromPin, *GetNameSafe(To), *ToPin);
		return false;
	}
	const FText Verdict = Schema->CanCreateConnection(Out, In).Message;
	Schema->TryCreateConnection(Out, In);
	// Mutable nodes rebuild their pins when a connection changes them (a section node takes its mesh's layout), so look
	// the pins up again rather than trusting the old pointers.
	Out = From->FindPin(FName(*FromPin), EGPD_Output);
	In = To->FindPin(FName(*ToPin), EGPD_Input);
	if (Out && In && Out->LinkedTo.Contains(In))
	{
		return true;
	}
	UE_LOG(LogShinobiCoop, Warning, TEXT("LinkPins: %s.%s -> %s.%s refused: %s"), *GetNameSafe(From), *FromPin, *GetNameSafe(To), *ToPin, *Verdict.ToString());
	return false;
#else
	return false;
#endif
}
