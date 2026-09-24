#include "Shinobi/ShinobiInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, FName Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	template <class TModifier>
	TModifier* AddModifier(FEnhancedActionKeyMapping& Mapping, UObject* Outer)
	{
		TModifier* Modifier = NewObject<TModifier>(Outer);
		Mapping.Modifiers.Add(Modifier);
		return Modifier;
	}
}

UShinobiInputConfig* UShinobiInputConfig::Build(UObject* Outer)
{
	UShinobiInputConfig* Config = NewObject<UShinobiInputConfig>(Outer);
	UInputMappingContext* Ctx = NewObject<UInputMappingContext>(Config, TEXT("IMC_Shinobi"));
	Config->Context = Ctx;

	// Move: WASD + left stick. X = right, Y = forward.
	Config->Move = MakeAction(Config, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	AddModifier<UInputModifierSwizzleAxis>(Ctx->MapKey(Config->Move, EKeys::W), Ctx);
	{
		FEnhancedActionKeyMapping& Back = Ctx->MapKey(Config->Move, EKeys::S);
		AddModifier<UInputModifierSwizzleAxis>(Back, Ctx);
		AddModifier<UInputModifierNegate>(Back, Ctx);
	}
	AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->Move, EKeys::A), Ctx);
	Ctx->MapKey(Config->Move, EKeys::D);
	Ctx->MapKey(Config->Move, EKeys::Gamepad_Left2D);

	// Look: mouse + right stick.
	// ponytail: Y inversion and stick speed are calibration knobs (project has bEnableLegacyInputScales=True)
	Config->Look = MakeAction(Config, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	{
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->Look, EKeys::Mouse2D), Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
	}
	{
		FEnhancedActionKeyMapping& Stick = Ctx->MapKey(Config->Look, EKeys::Gamepad_Right2D);
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Stick, Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
		AddModifier<UInputModifierScalar>(Stick, Ctx)->Scalar = FVector(60.0, 40.0, 1.0);
		AddModifier<UInputModifierScaleByDeltaTime>(Stick, Ctx);
	}

	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->Jump, EKeys::SpaceBar);
	Ctx->MapKey(Config->Jump, EKeys::Gamepad_FaceButton_Bottom);

	const auto AddAbility = [Config, Ctx](EShinobiAbilityInputID ID, FName Name, const FKey& Key, const FKey& PadKey)
	{
		UInputAction* Action = MakeAction(Config, Name, EInputActionValueType::Boolean);
		Ctx->MapKey(Action, Key);
		Ctx->MapKey(Action, PadKey);
		Config->AbilityActions.Add(ID, Action);
	};
	AddAbility(EShinobiAbilityInputID::Melee, TEXT("IA_Melee"), EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Left);
	AddAbility(EShinobiAbilityInputID::Dodge, TEXT("IA_Dodge"), EKeys::LeftControl, EKeys::Gamepad_FaceButton_Right);
	AddAbility(EShinobiAbilityInputID::Sprint, TEXT("IA_Sprint"), EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick);
	AddAbility(EShinobiAbilityInputID::ChakraCharge, TEXT("IA_ChakraCharge"), EKeys::C, EKeys::Gamepad_LeftShoulder);

	return Config;
}
