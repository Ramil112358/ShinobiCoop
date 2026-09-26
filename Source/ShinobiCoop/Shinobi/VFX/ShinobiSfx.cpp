#include "Shinobi/VFX/ShinobiSfx.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	USoundBase* Load(EShinobiSfx Sfx)
	{
		static const TCHAR* Names[] = {
			TEXT("Whoosh"), TEXT("Swish"), TEXT("Impact"), TEXT("Explosion"), TEXT("Zap"), TEXT("Whirr"), TEXT("Poof"),
			TEXT("Cast"), TEXT("Gong"), TEXT("Victory"), TEXT("Thud"),
		};
		static TArray<USoundBase*> Cache; // loaded once per process and rooted, so the raw pointers stay valid
		const int32 Index = static_cast<int32>(Sfx);
		if (Cache.Num() == 0)
		{
			for (const TCHAR* Name : Names)
			{
				USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Shinobi/Audio/SFX_%s.SFX_%s"), Name, Name));
				if (Sound)
				{
					Sound->AddToRoot();
				}
				Cache.Add(Sound);
			}
		}
		return Cache.IsValidIndex(Index) ? Cache[Index] : nullptr;
	}

	bool CanPlay(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World && World->GetNetMode() != NM_DedicatedServer && World->IsGameWorld();
	}
}

void ShinobiSfx::Play(const UObject* WorldContext, EShinobiSfx Sfx, const FVector& Location, float Volume)
{
	if (CanPlay(WorldContext))
	{
		if (USoundBase* Sound = Load(Sfx))
		{
			UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume);
		}
	}
}

void ShinobiSfx::Play2D(const UObject* WorldContext, EShinobiSfx Sfx, float Volume)
{
	if (CanPlay(WorldContext))
	{
		if (USoundBase* Sound = Load(Sfx))
		{
			UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume);
		}
	}
}
