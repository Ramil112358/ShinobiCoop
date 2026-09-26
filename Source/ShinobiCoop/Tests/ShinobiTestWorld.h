#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "EngineUtils.h"
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
		// There is no GameMode in this throwaway world, so UWorld::BeginPlay() never calls
		// GameMode->StartPlay(), which is what normally reaches AWorldSettings::NotifyBeginPlay().
		// Without it World::HasBegunPlay() stays false and spawned actors never get DispatchBeginPlay.
		World->GetWorldSettings()->NotifyBeginPlay();
	}

	FShinobiTestWorld(const FShinobiTestWorld&) = delete;
	FShinobiTestWorld& operator=(const FShinobiTestWorld&) = delete;

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

	/** Press and release (jutsu fire on press). Not named Cast: that would hide UE's Cast<T> inside this namespace. */
	inline void Tap(AShinobiCharacterBase* Character, EShinobiAbilityInputID InputID)
	{
		Press(Character, InputID);
		Release(Character, InputID);
	}

	template <class T>
	int32 CountActors(const FShinobiTestWorld& W)
	{
		int32 Count = 0;
		for (TActorIterator<T> It(W.World); It; ++It)
		{
			++Count;
		}
		return Count;
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

	/** Counts gameplay events with Tag delivered to Character's ability system and remembers the last
	 * instigator. Unbinds its lambda on destruction, so it no longer needs to be destroyed before the ASC. */
	struct FEventCounter
	{
		int32 Count = 0;
		const AActor* LastInstigator = nullptr;

		FEventCounter(AShinobiCharacterBase* Character, const FGameplayTag& Tag)
			: ASC(Character->GetAbilitySystemComponent())
			, EventTag(Tag)
		{
			Handle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda([this](const FGameplayEventData* Payload)
			{
				++Count;
				LastInstigator = Payload->Instigator.Get();
			});
		}

		~FEventCounter()
		{
			if (UAbilitySystemComponent* ASCPtr = ASC.Get())
			{
				ASCPtr->GenericGameplayEventCallbacks.FindOrAdd(EventTag).Remove(Handle);
			}
		}

		FEventCounter(const FEventCounter&) = delete;
		FEventCounter& operator=(const FEventCounter&) = delete;

	private:
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FGameplayTag EventTag;
		FDelegateHandle Handle;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
