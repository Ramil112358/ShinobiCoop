#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "Shinobi/ShinobiCharacterBase.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Abilities/ShinobiGameplayAbility.h"

constexpr EAutomationTestFlags ShinobiTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

/** Throwaway game world for GAS integration tests. Mirrors the engine's GameplayEffectsTest setup. */
struct FShinobiTestWorld
{
	UWorld* World = nullptr;

	FShinobiTestWorld()
		: InitialFrameCounter(GFrameCounter)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
	}

	~FShinobiTestWorld()
	{
		GFrameCounter = InitialFrameCounter;
		World->EndPlay(EEndPlayReason::Quit);
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	template <class T>
	T* Spawn(const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<T>(Location, Rotation, Params);
	}

	/** Advances the world in 50 ms steps so timers and periodic effects fire. */
	void Tick(float Seconds)
	{
		while (Seconds > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Seconds, 0.05f);
			World->Tick(LEVELTICK_All, Step);
			Seconds -= Step;
			++GFrameCounter;
		}
	}

private:
	uint64 InitialFrameCounter;
};

namespace ShinobiTest
{
	inline float Get(const AShinobiCharacterBase* Character, const FGameplayAttribute& Attribute)
	{
		return Character->GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
	}

	inline void Set(AShinobiCharacterBase* Character, const FGameplayAttribute& Attribute, float Value)
	{
		Character->GetAbilitySystemComponent()->SetNumericAttributeBase(Attribute, Value);
	}

	/** Applies an instant effect whose single modifier is SetByCaller(DataTag). */
	inline void ApplySetByCaller(AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& DataTag, float Magnitude)
	{
		UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(DataTag, Magnitude);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	inline void Damage(AShinobiCharacterBase* Target, float Amount)
	{
		ApplySetByCaller(Target, UGE_Damage::StaticClass(), ShinobiTags::Data_Damage, Amount);
	}

	inline void ApplyEffect(AShinobiCharacterBase* Target, TSubclassOf<UGameplayEffect> EffectClass)
	{
		UAbilitySystemComponent* ASC = Target->GetAbilitySystemComponent();
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, ASC->MakeEffectContext());
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	inline void Press(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Character->GetAbilitySystemComponent()->AbilityLocalInputPressed(static_cast<int32>(InputID));
	}

	inline void Release(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Character->GetAbilitySystemComponent()->AbilityLocalInputReleased(static_cast<int32>(InputID));
	}

	inline bool IsActive(AShinobiCharacterBase* Character, TSubclassOf<UGameplayAbility> AbilityClass)
	{
		const FGameplayAbilitySpec* Spec = Character->GetAbilitySystemComponent()->FindAbilitySpecFromClass(AbilityClass);
		return Spec && Spec->IsActive();
	}

	inline bool HasTag(const AShinobiCharacterBase* Character, const FGameplayTag& Tag)
	{
		return Character->GetAbilitySystemComponent()->HasMatchingGameplayTag(Tag);
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
