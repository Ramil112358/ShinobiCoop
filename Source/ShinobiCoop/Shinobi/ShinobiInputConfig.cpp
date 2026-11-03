#include "Shinobi/ShinobiInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Shinobi/UI/ShinobiKeyBindings.h"
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

	// Everything a player can rebind (ShinobiKeyBindings) starts unmapped: ApplyKeyBindings maps each action's keyboard/mouse
	// key and controller button(s).
	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	const auto AddAbility = [Config](EShinobiAbilityInputID ID, FName Name)
	{
		Config->AbilityActions.Add(ID, MakeAction(Config, Name, EInputActionValueType::Boolean));
	};
	AddAbility(EShinobiAbilityInputID::Melee, TEXT("IA_Melee"));
	AddAbility(EShinobiAbilityInputID::Dodge, TEXT("IA_Dodge"));
	AddAbility(EShinobiAbilityInputID::Sprint, TEXT("IA_Sprint"));
	AddAbility(EShinobiAbilityInputID::ChakraCharge, TEXT("IA_ChakraCharge"));
	AddAbility(EShinobiAbilityInputID::Dismount, TEXT("IA_Dismount")); // off the dragon, whatever is in the slots
	AddAbility(EShinobiAbilityInputID::Revive, TEXT("IA_Revive"));
	AddAbility(EShinobiAbilityInputID::Block, TEXT("IA_Block")); // hold
	for (int32 Slot = 0; Slot < ShinobiLoadout::NumSlots; ++Slot)
	{
		AddAbility(ShinobiLoadout::SlotInputID(Slot), *FString::Printf(TEXT("IA_Jutsu%d"), Slot + 1));
	}
	Config->LockOn = MakeAction(Config, TEXT("IA_LockOn"), EInputActionValueType::Boolean);
	Config->SkillsScreen = MakeAction(Config, TEXT("IA_SkillsScreen"), EInputActionValueType::Boolean);
	Config->MapToggle = MakeAction(Config, TEXT("IA_MapToggle"), EInputActionValueType::Boolean);
	Config->MapZoomIn = MakeAction(Config, TEXT("IA_MapZoomIn"), EInputActionValueType::Boolean);
	Config->MapZoomOut = MakeAction(Config, TEXT("IA_MapZoomOut"), EInputActionValueType::Boolean);

	// Fixed: Esc / Menu pause, so there's always a way back to CONTROLS.
	Config->Pause = MakeAction(Config, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->Pause, EKeys::Escape);
	Ctx->MapKey(Config->Pause, EKeys::Gamepad_Special_Right);

	// The chord actions: Alt for Alt + key, RB / RT for a controller combo. None does anything by itself.
	Config->AltModifier = MakeAction(Config, TEXT("IA_AltModifier"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->AltModifier, EKeys::LeftAlt);
	Ctx->MapKey(Config->AltModifier, EKeys::RightAlt);
	Config->PadHoldRB = MakeAction(Config, TEXT("IA_PadHoldRB"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->PadHoldRB, EKeys::Gamepad_RightShoulder);
	Config->PadHoldRT = MakeAction(Config, TEXT("IA_PadHoldRT"), EInputActionValueType::Boolean);
	Ctx->MapKey(Config->PadHoldRT, EKeys::Gamepad_RightTrigger);

	// Mouse wheel axis: positive values zoom in, negative values zoom out.
	Config->CameraZoom = MakeAction(Config, TEXT("IA_CameraZoom"), EInputActionValueType::Axis1D);
	Ctx->MapKey(Config->CameraZoom, EKeys::MouseWheelAxis);

	Config->ApplyKeyBindings(); // the defaults above, replaced by this machine's saved shortcuts
	return Config;
}

UInputAction* UShinobiInputConfig::ActionForBindable(uint8 Bindable) const
{
	using ShinobiKeyBindings::EBindable;
	const auto Ability = [this](EShinobiAbilityInputID ID) -> UInputAction*
	{
		const TObjectPtr<UInputAction>* Found = AbilityActions.Find(ID);
		return Found ? Found->Get() : nullptr;
	};
	const EBindable Which = static_cast<EBindable>(Bindable);
	if (Which <= EBindable::Slot8)
	{
		return Ability(ShinobiLoadout::SlotInputID(Bindable));
	}
	switch (Which)
	{
	case EBindable::SkillsScreen: return SkillsScreen;
	case EBindable::Melee: return Ability(EShinobiAbilityInputID::Melee);
	case EBindable::Dodge: return Ability(EShinobiAbilityInputID::Dodge);
	case EBindable::Sprint: return Ability(EShinobiAbilityInputID::Sprint);
	case EBindable::Block: return Ability(EShinobiAbilityInputID::Block);
	case EBindable::ChakraCharge: return Ability(EShinobiAbilityInputID::ChakraCharge);
	case EBindable::Revive: return Ability(EShinobiAbilityInputID::Revive);
	case EBindable::LockOn: return LockOn;
	case EBindable::Jump: return Jump;
	case EBindable::Dismount: return Ability(EShinobiAbilityInputID::Dismount);
	case EBindable::Map: return MapToggle;
	case EBindable::MapZoomIn: return MapZoomIn;
	case EBindable::MapZoomOut: return MapZoomOut;
	default: return nullptr;
	}
}

void UShinobiInputConfig::ApplyKeyBindings() const
{
	UInputMappingContext* Ctx = Context;
	if (!Ctx)
	{
		return;
	}
	constexpr int32 NumBindables = static_cast<int32>(ShinobiKeyBindings::EBindable::Num);
	// Off with every rebindable action's old keys and buttons first.
	for (int32 Index = 0; Index < NumBindables; ++Index)
	{
		UInputAction* Action = ActionForBindable(static_cast<uint8>(Index));
		TArray<FKey> OldKeys;
		for (const FEnhancedActionKeyMapping& Mapping : Ctx->GetMappings())
		{
			if (Action && Mapping.Action == Action)
			{
				OldKeys.Add(Mapping.Key);
			}
		}
		for (const FKey& Key : OldKeys)
		{
			Ctx->UnmapKey(Action, Key);
		}
	}
	// Then the combos (Alt + key, RB / RT + button), then the plain ones: Enhanced Input blocks a key's plain mappings while
	// a combo's first key is held only when they come after the combo, so RB + A doesn't also jump.
	for (const bool bCombos : { true, false })
	{
		for (int32 Index = 0; Index < NumBindables; ++Index)
		{
			const ShinobiKeyBindings::EBindable Which = static_cast<ShinobiKeyBindings::EBindable>(Index);
			UInputAction* Action = ActionForBindable(static_cast<uint8>(Index));
			if (!Action)
			{
				continue;
			}
			const ShinobiKeyBindings::FBinding Binding = ShinobiKeyBindings::Get(Which);
			if (Binding.bAlt == bCombos)
			{
				FEnhancedActionKeyMapping& Mapping = Ctx->MapKey(Action, Binding.Key);
				if (bCombos)
				{
					UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Ctx);
					Chord->ChordAction = AltModifier;
					Mapping.Triggers.Add(Chord);
				}
			}
			const ShinobiKeyBindings::FPadBinding Pad = ShinobiKeyBindings::GetPad(Which);
			if (Pad.IsBound() && Pad.Hold.IsValid() == bCombos)
			{
				FEnhancedActionKeyMapping& Mapping = Ctx->MapKey(Action, Pad.Button);
				if (bCombos)
				{
					UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Ctx);
					Chord->ChordAction = Pad.Hold == EKeys::Gamepad_RightTrigger ? PadHoldRT : PadHoldRB;
					Mapping.Triggers.Add(Chord);
				}
			}
		}
	}
	if (!GEngine)
	{
		return;
	}
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		const UGameInstance* GameInstance = WorldContext.OwningGameInstance;
		for (ULocalPlayer* Player : GameInstance ? GameInstance->GetLocalPlayers() : TArray<ULocalPlayer*>())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player))
			{
				Subsystem->RequestRebuildControlMappings();
			}
		}
	}
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
