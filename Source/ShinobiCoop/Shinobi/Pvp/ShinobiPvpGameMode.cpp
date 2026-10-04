#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiSummon.h"

namespace
{
	/** A fighter slot: a player (bots join them later), never a summon on the side. */
	bool IsFighter(const AShinobiCharacterBase* Character)
	{
		return Character && Character->IsA<AShinobiPlayerCharacter>();
	}

	/** What a round's reset takes off every fighter: the mount, armour, statuses and the hold. */
	const FGameplayTagContainer& RoundClearedTags()
	{
		static const FGameplayTagContainer Tags = []
		{
			FGameplayTagContainer Container;
			for (const FGameplayTag Tag : {
				ShinobiTags::State_PvpHold.GetTag(), ShinobiTags::State_Mounted.GetTag(), ShinobiTags::State_Susanoo.GetTag(),
				ShinobiTags::State_Phased.GetTag(), ShinobiTags::State_TeleportMarked.GetTag(), ShinobiTags::State_Burning.GetTag(),
				ShinobiTags::State_Shocked.GetTag(), ShinobiTags::State_ShockBonus.GetTag(), ShinobiTags::State_Staggered.GetTag(),
				ShinobiTags::State_Chilled.GetTag() })
			{
				Container.AddTag(Tag);
			}
			return Container;
		}();
		return Tags;
	}

	const TCHAR* ResultName(EShinobiRoundResult Result)
	{
		switch (Result)
		{
		case EShinobiRoundResult::TeamAWins: return TEXT("A");
		case EShinobiRoundResult::TeamBWins: return TEXT("B");
		case EShinobiRoundResult::Tie: return TEXT("draw");
		default: return TEXT("nobody");
		}
	}
}

const FName AShinobiPvpGameMode::TeamStartTagA(TEXT("Team:A"));
const FName AShinobiPvpGameMode::TeamStartTagB(TEXT("Team:B"));

AShinobiPvpGameMode::AShinobiPvpGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AShinobiPlayerCharacter::StaticClass();
	PlayerControllerClass = AShinobiPlayerController::StaticClass();
	GameStateClass = AShinobiGameState::StaticClass();
}

AShinobiGameState* AShinobiPvpGameMode::GetPvpState() const
{
	return GetGameState<AShinobiGameState>();
}

void AShinobiPvpGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	const FString Format = UGameplayStatics::ParseOption(Options, TEXT("Format"));
	PvpFormat = Format == TEXT("2v2") ? EShinobiPvpFormat::TwoVsTwo : EShinobiPvpFormat::OneVsOne;
	if (Format != TEXT("1v1") && Format != TEXT("2v2"))
	{
		UE_LOG(LogShinobiCoop, Warning, TEXT("Pvp: no Format=1v1|2v2 option (got '%s'), playing 1v1"), *Format);
	}
}

void AShinobiPvpGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (PvpFormat != EShinobiPvpFormat::None && GetPvpState())
	{
		StartWarmup();
	}
}

void AShinobiPvpGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty() && PvpFormat != EShinobiPvpFormat::None
		&& !FShinobiPvpMatch::TeamForJoin(PvpFormat, CountHumans(EShinobiTeam::TeamA), CountHumans(EShinobiTeam::TeamB)).IsSet())
	{
		ErrorMessage = TEXT("Match full");
	}
}

void AShinobiPvpGameMode::PostLogin(APlayerController* NewPlayer)
{
	AssignTeam(NewPlayer); // before Super, which spawns the pawn at a start of this side
	Super::PostLogin(NewPlayer);
}

void AShinobiPvpGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	AssignTeam(NewPlayer); // the pawn exists now
}

void AShinobiPvpGameMode::Logout(AController* Exiting)
{
	Teams.Remove(Exiting);
	Super::Logout(Exiting);
}

void AShinobiPvpGameMode::AssignTeam(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (!Teams.Contains(Controller))
	{
		// PreLogin turned away whoever would not fit; the listen server's own player always does.
		const EShinobiTeam Team = FShinobiPvpMatch::TeamForJoin(PvpFormat, CountHumans(EShinobiTeam::TeamA), CountHumans(EShinobiTeam::TeamB))
			.Get(EShinobiTeam::TeamA);
		Teams.Add(Controller, Team);
		UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: %s joins team %s"), *Controller->GetName(), Team == EShinobiTeam::TeamA ? TEXT("A") : TEXT("B"));
	}
	if (AShinobiCharacterBase* Body = Cast<AShinobiCharacterBase>(Controller->GetPawn()))
	{
		Body->SetTeam(Teams[Controller]);
	}
}

EShinobiTeam AShinobiPvpGameMode::GetTeamOf(AController* Controller) const
{
	const EShinobiTeam* Team = Teams.Find(Controller);
	return Team ? *Team : EShinobiTeam::Players;
}

int32 AShinobiPvpGameMode::CountHumans(EShinobiTeam Team) const
{
	int32 Count = 0;
	for (const TPair<TWeakObjectPtr<AController>, EShinobiTeam>& Entry : Teams)
	{
		Count += Entry.Key.IsValid() && Entry.Value == Team ? 1 : 0;
	}
	return Count;
}

TArray<APlayerStart*> AShinobiPvpGameMode::GetTeamStarts(EShinobiTeam Team) const
{
	TArray<APlayerStart*> Starts;
	const FName Tag = Team == EShinobiTeam::TeamA ? TeamStartTagA : TeamStartTagB;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			Starts.Add(*It);
		}
	}
	Starts.Sort([](const APlayerStart& A, const APlayerStart& B) { return A.GetName() < B.GetName(); });
	return Starts;
}

AActor* AShinobiPvpGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const TArray<APlayerStart*> Starts = GetTeamStarts(GetTeamOf(Player));
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player); // an untagged map: any start
	}
	// The first of the side's starts nobody stands on, so teammates don't spawn inside each other.
	for (APlayerStart* Start : Starts)
	{
		bool bTaken = false;
		for (TActorIterator<APawn> Pawn(GetWorld()); Pawn && !bTaken; ++Pawn)
		{
			bTaken = FVector::Dist(Pawn->GetActorLocation(), Start->GetActorLocation()) < 150.f;
		}
		if (!bTaken)
		{
			return Start;
		}
	}
	return Starts[FMath::RandRange(0, Starts.Num() - 1)];
}

TArray<AShinobiCharacterBase*> AShinobiPvpGameMode::GetTeamMembers(EShinobiTeam Team) const
{
	TArray<AShinobiCharacterBase*> Members;
	for (TActorIterator<AShinobiCharacterBase> It(GetWorld()); It; ++It)
	{
		if (IsFighter(*It) && It->GetTeam() == Team)
		{
			Members.Add(*It);
		}
	}
	return Members;
}

FShinobiTeamStatus AShinobiPvpGameMode::GatherStatus(EShinobiTeam Team) const
{
	FShinobiTeamStatus Status;
	const TArray<AShinobiCharacterBase*> Members = GetTeamMembers(Team);
	float Health = 0.f;
	float MaxHealth = 0.f;
	for (const AShinobiCharacterBase* Member : Members)
	{
		MaxHealth = FMath::Max(MaxHealth, Member->GetAttributes()->GetMaxHealth());
		if (Member->IsAlive())
		{
			++Status.Alive;
			Health += Member->GetAttributes()->GetHealth();
		}
	}
	// Over the side's slots: a missing fighter (left, fell out of the world) is an empty, 0 HP slot.
	const int32 Slots = FMath::Max(FShinobiPvpMatch::SlotsPerTeam(PvpFormat), Members.Num());
	Status.HealthFraction = Slots > 0 && MaxHealth > 0.f ? Health / (Slots * MaxHealth) : 0.f;
	return Status;
}

void AShinobiPvpGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AShinobiGameState* State = GetPvpState();
	if (!State || PvpFormat == EShinobiPvpFormat::None || State->PvpPhase == EShinobiPvpPhase::None || bTravelling)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (GetNumPlayers() == 0)
	{
		// Nobody here: idle, and the warmup waits for the first player.
		if (State->PvpPhase == EShinobiPvpPhase::Warmup)
		{
			State->PhaseEndTime = Now + FShinobiPvpMatch::WarmupSeconds;
		}
		return;
	}
	const bool bTimeUp = State->PhaseEndTime > 0.f && Now >= State->PhaseEndTime;
	switch (State->PvpPhase)
	{
	case EShinobiPvpPhase::Warmup:
		if (bTimeUp || CountHumans(EShinobiTeam::TeamA) + CountHumans(EShinobiTeam::TeamB) >= 2 * FShinobiPvpMatch::SlotsPerTeam(PvpFormat))
		{
			FillBots();
			StartRound();
		}
		break;
	case EShinobiPvpPhase::Countdown:
		if (bTimeUp)
		{
			StartFight();
		}
		break;
	case EShinobiPvpPhase::Fighting:
		if (const EShinobiRoundResult Result = FShinobiPvpMatch::Judge(GatherStatus(EShinobiTeam::TeamA), GatherStatus(EShinobiTeam::TeamB), bTimeUp);
			Result != EShinobiRoundResult::Ongoing)
		{
			EndRound(Result);
		}
		break;
	case EShinobiPvpPhase::RoundEnd:
		if (bTimeUp)
		{
			Match.IsOver() ? EndMatch() : StartRound();
		}
		break;
	case EShinobiPvpPhase::MatchEnd:
		if (bTimeUp)
		{
			Rematch();
		}
		break;
	default:
		break;
	}
}

void AShinobiPvpGameMode::SetPhase(EShinobiPvpPhase NewPhase, float Seconds)
{
	if (AShinobiGameState* State = GetPvpState())
	{
		State->PvpPhase = NewPhase;
		State->PhaseEndTime = Seconds > 0.f ? GetWorld()->GetTimeSeconds() + Seconds : 0.f;
	}
}

void AShinobiPvpGameMode::StartWarmup()
{
	Match = FShinobiPvpMatch();
	AShinobiGameState* State = GetPvpState();
	State->PvpFormat = PvpFormat;
	State->PvpRound = Match.Round;
	State->PvpWinsA = 0;
	State->PvpWinsB = 0;
	State->PvpRoundResult = EShinobiRoundResult::Ongoing;
	SetPhase(EShinobiPvpPhase::Warmup, FShinobiPvpMatch::WarmupSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: warmup (%s)"), PvpFormat == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : TEXT("1v1"));
}

void AShinobiPvpGameMode::StartRound()
{
	ResetCharacters();
	OnRoundReset();
	if (AShinobiGameState* State = GetPvpState())
	{
		State->PvpRound = Match.Round;
		State->PvpRoundResult = EShinobiRoundResult::Ongoing;
	}
	SetHold(true);
	SetPhase(EShinobiPvpPhase::Countdown, FShinobiPvpMatch::CountdownSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: round %d countdown"), Match.Round);
}

void AShinobiPvpGameMode::StartFight()
{
	SetHold(false);
	SetPhase(EShinobiPvpPhase::Fighting, FShinobiPvpMatch::RoundSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: round %d start"), Match.Round);
}

void AShinobiPvpGameMode::EndRound(EShinobiRoundResult Result)
{
	if (Match.IsOver()) // Record does not guard itself
	{
		return;
	}
	const int32 Round = Match.Round;
	Match.Record(Result);
	if (AShinobiGameState* State = GetPvpState())
	{
		State->PvpWinsA = Match.WinsA;
		State->PvpWinsB = Match.WinsB;
		State->PvpRoundResult = Result;
	}
	SetPhase(EShinobiPvpPhase::RoundEnd, FShinobiPvpMatch::RoundEndSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: round %d won by %s (A %d - B %d)"), Round, ResultName(Result), Match.WinsA, Match.WinsB);
}

void AShinobiPvpGameMode::EndMatch()
{
	if (AShinobiGameState* State = GetPvpState())
	{
		State->PvpRoundResult = Match.MatchResult();
	}
	SetPhase(EShinobiPvpPhase::MatchEnd, FShinobiPvpMatch::ResultSeconds);
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: match won by %s (A %d - B %d)"), ResultName(Match.MatchResult()), Match.WinsA, Match.WinsB);
}

void AShinobiPvpGameMode::Rematch()
{
	bTravelling = true;
	const FString Map = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	const FString Url = Map + TEXT("?game=/Script/ShinobiCoop.ShinobiPvpGameMode?Format=")
		+ (PvpFormat == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : TEXT("1v1"));
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: rematch, travelling to %s"), *Url);
	GetWorld()->ServerTravel(Url);
}

void AShinobiPvpGameMode::SetHold(bool bHold)
{
	for (const EShinobiTeam Team : { EShinobiTeam::TeamA, EShinobiTeam::TeamB })
	{
		for (AShinobiCharacterBase* Member : GetTeamMembers(Team))
		{
			UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent();
			if (bHold)
			{
				ASC->ApplyGameplayEffectToSelf(GetDefault<UGE_PvpHold>(), 1.f, ASC->MakeEffectContext());
			}
			else
			{
				ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ShinobiTags::State_PvpHold));
			}
		}
	}
}

void AShinobiPvpGameMode::ResetCharacters()
{
	// The dead and the missing (a body that fell out of the world) get a fresh pawn at their side's start.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Body = PC ? PC->GetPawn() : nullptr;
		const AShinobiCharacterBase* Shinobi = Cast<AShinobiCharacterBase>(Body);
		if (!PC || (Body && !(Shinobi && Shinobi->GetAbilitySystemComponent()->HasMatchingGameplayTag(ShinobiTags::State_Dead))))
		{
			continue;
		}
		if (Body)
		{
			PC->UnPossess();
			Body->Destroy();
		}
		RestartPlayer(PC);
		AssignTeam(PC);
	}

	// Summons leave with the round (the dragon goes with the mount tag below).
	TArray<AShinobiSummon*> Summons;
	for (TActorIterator<AShinobiSummon> It(GetWorld()); It; ++It)
	{
		Summons.Add(*It);
	}
	for (AShinobiSummon* Summon : Summons)
	{
		Summon->Dismiss();
	}

	for (const EShinobiTeam Team : { EShinobiTeam::TeamA, EShinobiTeam::TeamB })
	{
		const TArray<APlayerStart*> Starts = GetTeamStarts(Team);
		const TArray<AShinobiCharacterBase*> Members = GetTeamMembers(Team);
		for (int32 Index = 0; Index < Members.Num(); ++Index)
		{
			AShinobiCharacterBase* Member = Members[Index];
			UAbilitySystemComponent* ASC = Member->GetAbilitySystemComponent();
			if (ASC->HasMatchingGameplayTag(ShinobiTags::State_Dead))
			{
				continue; // a dead body without a controller to respawn it (OnRoundReset deals with bots)
			}
			ASC->CancelAllAbilities();
			if (AShinobiPlayerCharacter* Player = Cast<AShinobiPlayerCharacter>(Member))
			{
				Player->Revive();
			}
			ASC->RemoveActiveEffectsWithGrantedTags(RoundClearedTags());
			const UShinobiAttributeSet* Stats = Member->GetAttributes();
			ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), Stats->GetMaxHealth());
			ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetChakraAttribute(), Stats->GetMaxChakra());
			ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetStaminaAttribute(), Stats->GetMaxStamina());
			if (Starts.Num() == 0)
			{
				continue;
			}
			const APlayerStart* Start = Starts[Index % Starts.Num()];
			const FVector Spot = Start->GetActorLocation();
			const FRotator Facing(0.f, Start->GetActorRotation().Yaw, 0.f);
			if (!Member->TeleportTo(Spot, Facing))
			{
				Member->TeleportTo(Spot, Facing, false, true); // more fighters than starts: share one
			}
			Member->GetCharacterMovement()->StopMovementImmediately();
			if (APlayerController* PC = Cast<APlayerController>(Member->GetController()))
			{
				PC->ClientSetRotation(Facing, true);
			}
		}
	}
}
