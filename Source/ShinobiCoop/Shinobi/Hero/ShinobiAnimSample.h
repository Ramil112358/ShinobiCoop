#pragma once

#include "CoreMinimal.h"

class UAnimInstance;
class UAnimMontage;
class UChooserTable;
class UScriptStruct;

/** Using the Game Animation Sample's chooser tables (traversal montages, getups) from C++. */
namespace ShinobiAnimSample
{
	/** Copies one Blueprint field into another of the same type (the AnimBP's gait into a chooser's input, ...). */
	void CopyField(const FProperty* To, void* ToContainer, const FProperty* From, const void* FromContainer);

	/** Calls the AnimBP's Get_PoseHistory and copies its result into Field: the choosers' pose-match columns read it. */
	void CopyPoseHistory(UAnimInstance* AnimInstance, const FProperty* Field, void* Container);

	/** Copies the AnimBP's CharacterProperties.<Name> (filled by UShinobiLocomotionAnimInstance) into Field. */
	void CopyCharacterProperty(UAnimInstance* AnimInstance, FName Name, const FProperty* Field, void* Container);

	/** Evaluates a chooser whose context is (the AnimBP, Input, Output), the sample's convention, and returns the first
	 * montage it picks; the chooser's output columns fill Output. */
	UAnimMontage* ChooseMontage(const UChooserTable* Chooser, UAnimInstance* AnimInstance, const UScriptStruct* InputType, void* Input,
		const UScriptStruct* OutputType, void* Output);
}
