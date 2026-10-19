#include "Shinobi/ShinobiInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Shinobi/Abilities/ShinobiLoadout.h"

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
	AddModifier<UInputModifierDeadZone>(Ctx->MapKey(Config->Move, EKeys::Gamepad_Left2D), Ctx);

	// Look: mouse + right stick.
	// Y must be negated on both devices: this project has bEnableLegacyInputScales=True
	// (DefaultInput.ini), so AddControllerPitchInput/AddControllerYawInput apply the engine's
	// default APlayerController::InputPitchScale=-2.5 / InputYawScale=2.5 (BaseGame.ini) on top of
	// whatever we send. Mouse2D also picks up an automatic Scalar(0.07) sensitivity modifier from
	// BaseInput.ini's AxisConfig, since EnhancedInput only auto-applies AxisConfig to mouse keys, not
	// gamepad axes (see EnhancedInputSubsystemInterface::ApplyAxisPropertyModifiers). Flipping
	// bEnableLegacyInputScales to False would change the pitch sign and both look speeds, so the
	// negation here is load-bearing, not a preference; only the stick's Scalar magnitude below is a
	// calibration knob.
	Config->Look = MakeAction(Config, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	{
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->Look, EKeys::Mouse2D), Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
	}
	{
		FEnhancedActionKeyMapping& Stick = Ctx->MapKey(Config->Look, EKeys::Gamepad_Right2D);
		// Dead zone must run first, before the stick is negated/scaled, so it sees the raw axis value.
		AddModifier<UInputModifierDeadZone>(Stick, Ctx);
		UInputModifierNegate* InvertY = AddModifier<UInputModifierNegate>(Stick, Ctx);
		InvertY->bX = false;
		InvertY->bZ = false;
		AddModifier<UInputModifierScalar>(Stick, Ctx)->Scalar = FVector(60.0, 40.0, 1.0);
		AddModifier<UInputModifierScaleByDeltaTime>(Stick, Ctx);
	}

	Config->CameraFreeLook = MakeAction(Config, TEXT("IA_CameraFreeLook"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->CameraFreeLook, EKeys::RightMouseButton);

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
	AddAbility(EShinobiAbilityInputID::Dismount, TEXT("IA_Dismount"), EKeys::X, EKeys::Gamepad_DPad_Down); // off the dragon, on any page
	AddAbility(EShinobiAbilityInputID::Revive, TEXT("IA_Revive"), EKeys::E, EKeys::Gamepad_FaceButton_Top); // RB + Y is jutsu 4 (JutsuContext wins)
	AddAbility(EShinobiAbilityInputID::Block, TEXT("IA_Block"), EKeys::F, EKeys::Gamepad_LeftTrigger); // hold

	Config->LockOn = MakeAction(Config, TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->LockOn, EKeys::Tab);
	Ctx->MapKey(Config->LockOn, EKeys::Gamepad_RightThumbstick);

	Config->Pause = MakeAction(Config, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->Pause, EKeys::Escape);
	Ctx->MapKey(Config->Pause, EKeys::Gamepad_Special_Right);

	// Jutsu slots 1-8: one keyboard key each here. The pad reaches the current pad page's four through PadJutsuActions,
	// face buttons in JutsuContext, which the player adds while RB is held.
	Config->JutsuModifier = MakeAction(Config, TEXT("IA_JutsuModifier"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->JutsuModifier, EKeys::Gamepad_RightShoulder);
	UInputMappingContext* JutsuCtx = NewObject<UInputMappingContext>(Config, TEXT("IMC_ShinobiJutsu"));
	Config->JutsuContext = JutsuCtx;
	const FKey SlotKeys[ShinobiLoadout::NumSlots] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::R, EKeys::T, EKeys::G, EKeys::CapsLock };
	for (int32 Slot = 0; Slot < ShinobiLoadout::NumSlots; ++Slot)
	{
		UInputAction* Action = MakeAction(Config, *FString::Printf(TEXT("IA_Jutsu%d"), Slot + 1), EInputActionValueType::Boolean);
		Ctx->MapKey(Action, SlotKeys[Slot]);
		Config->AbilityActions.Add(ShinobiLoadout::SlotInputID(Slot), Action);
	}
	const FKey FaceKeys[] = { EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top };
	for (int32 Face = 0; Face < UE_ARRAY_COUNT(FaceKeys); ++Face)
	{
		UInputAction* Action = MakeAction(Config, *FString::Printf(TEXT("IA_PadJutsu%d"), Face + 1), EInputActionValueType::Boolean);
		JutsuCtx->MapKey(Action, FaceKeys[Face]);
		Config->PadJutsuActions.Add(Action);
	}

	// Alt + key bindings chord on this (ShinobiKeyBindings).
	Config->AltModifier = MakeAction(Config, TEXT("IA_AltModifier"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->AltModifier, EKeys::LeftAlt);
	Ctx->MapKey(Config->AltModifier, EKeys::RightAlt);

	Config->SkillsScreen = MakeAction(Config, TEXT("IA_SkillsScreen"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->SkillsScreen, EKeys::K);

	// Pad jutsu pages (slots 1-4 / 5-8): the D-pad while RB is held. The keyboard has a key per slot.
	Config->JutsuPageNext = MakeAction(Config, TEXT("IA_JutsuPageNext"), EInputActionValueType::Boolean);
	JutsuCtx->MapKey(Config->JutsuPageNext, EKeys::Gamepad_DPad_Right);
	Config->JutsuPagePrev = MakeAction(Config, TEXT("IA_JutsuPagePrev"), EInputActionValueType::Boolean);
	JutsuCtx->MapKey(Config->JutsuPagePrev, EKeys::Gamepad_DPad_Left);

	// Mouse wheel axis: positive values zoom in, negative values zoom out.
	Config->CameraZoom = MakeAction(Config, TEXT("IA_CameraZoom"), EInputActionValueType::Axis1D);
	Ctx->MapKey(Config->CameraZoom, EKeys::MouseWheelAxis);

	Config->MapToggle = MakeAction(Config, TEXT("IA_MapToggle"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->MapToggle, EKeys::M);
	Ctx->MapKey(Config->MapToggle, EKeys::Gamepad_Special_Left);
	// D-pad down is Dismount, so the pad only zooms in: past the closest level it wraps back to the whole map.
	Config->MapZoom = MakeAction(Config, TEXT("IA_MapZoom"), EInputActionValueType::Axis1D);
	Ctx->MapKey(Config->MapZoom, EKeys::Equals);
	Ctx->MapKey(Config->MapZoom, EKeys::Add);
	Ctx->MapKey(Config->MapZoom, EKeys::Gamepad_DPad_Up);
	AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->MapZoom, EKeys::Hyphen), Ctx);
	AddModifier<UInputModifierNegate>(Ctx->MapKey(Config->MapZoom, EKeys::Subtract), Ctx);

	return Config;
}

// Cached for the process: after Live Coding changes to Build() or to this class's layout, restart the editor.
const UShinobiInputConfig& UShinobiInputConfig::Get()
{
	static UShinobiInputConfig* Instance = []
	{
		UShinobiInputConfig* Config = Build(GetTransientPackage());
		Config->AddToRoot(); // lives for the process, like an input asset
		return Config;
	}();
	return *Instance;
}
