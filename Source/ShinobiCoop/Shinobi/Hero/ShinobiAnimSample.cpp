#include "Shinobi/Hero/ShinobiAnimSample.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Chooser.h"
#include "Shinobi/Hero/ShinobiBlueprintStruct.h"

void ShinobiAnimSample::CopyField(const FProperty* To, void* ToContainer, const FProperty* From, const void* FromContainer)
{
	if (To && From && To->SameType(From))
	{
		To->CopyCompleteValue(To->ContainerPtrToValuePtr<void>(ToContainer), From->ContainerPtrToValuePtr<void>(FromContainer));
	}
}

void ShinobiAnimSample::CopyPoseHistory(UAnimInstance* AnimInstance, const FProperty* Field, void* Container)
{
	UFunction* Function = Field ? AnimInstance->FindFunction(TEXT("Get_PoseHistory")) : nullptr;
	if (!Function)
	{
		return;
	}
	uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
	FMemory::Memzero(Params, Function->ParmsSize);
	Function->InitializeStruct(Params);
	AnimInstance->ProcessEvent(Function, Params);
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm))
		{
			CopyField(Field, Container, *It, Params);
			break;
		}
	}
	Function->DestroyStruct(Params);
}

void ShinobiAnimSample::CopyCharacterProperty(UAnimInstance* AnimInstance, FName Name, const FProperty* Field, void* Container)
{
	if (const FStructProperty* Properties = CastField<FStructProperty>(AnimInstance->GetClass()->FindPropertyByName(TEXT("CharacterProperties"))))
	{
		CopyField(Field, Container, ShinobiBlueprintStruct::Find(Properties->Struct, Name), Properties->ContainerPtrToValuePtr<void>(AnimInstance));
	}
}

UAnimMontage* ShinobiAnimSample::ChooseMontage(const UChooserTable* Chooser, UAnimInstance* AnimInstance, const UScriptStruct* InputType, void* Input,
	const UScriptStruct* OutputType, void* Output)
{
	UAnimMontage* Montage = nullptr;
	FChooserEvaluationContext Context(AnimInstance);
	Context.AddStructViewParam(FStructView(InputType, static_cast<uint8*>(Input)));
	Context.AddStructViewParam(FStructView(OutputType, static_cast<uint8*>(Output)));
	UChooserTable::EvaluateChooser(Context, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda([&Montage](UObject* Result)
	{
		Montage = Cast<UAnimMontage>(Result);
		return Montage ? FObjectChooserBase::EIteratorStatus::Stop : FObjectChooserBase::EIteratorStatus::Failed;
	}));
	return Montage;
}
