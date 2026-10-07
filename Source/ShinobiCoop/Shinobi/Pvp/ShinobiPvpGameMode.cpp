#include "Shinobi/Pvp/ShinobiPvpGameMode.h"
#include "AbilitySystemComponent.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "ShinobiCoop.h"
#include "Shinobi/ShinobiAttributeSet.h"
#include "Shinobi/ShinobiEffects.h"
#include "Shinobi/ShinobiGameState.h"
#include "Shinobi/ShinobiPlayerCharacter.h"
#include "Shinobi/ShinobiPlayerController.h"
#include "Shinobi/ShinobiTags.h"
#include "Shinobi/Enemies/ShinobiEnemyCharacter.h"
#include "Shinobi/Enemies/ShinobiSummon.h"
#include "Shinobi/Pvp/ShinobiPickup.h"

namespace
{
	/** A fighter slot: a player or a bot, never a summon (nor any other enemy) on the side. */
	bool IsFighter(const AShinobiCharacterBase* Character)
	{
		return Character && (Character->IsA<AShinobiPlayerCharacter>() || Character->IsA<AShinobiBotFighter>());
	}

	/** What a round's reset takes off every fighter: the mount, armour, statuses, pickup boosts and shield, and the hold. */
	const FGameplayTagContainer& RoundClearedTags()
	{
		static const FGameplayTagContainer Tags = []
		{
			FGameplayTagContainer Container;
			for (const FGameplayTag Tag : {
				ShinobiTags::State_PvpHold.GetTag(), ShinobiTags::State_Mounted.GetTag(), ShinobiTags::State_Susanoo.GetTag(),
				ShinobiTags::State_Phased.GetTag(), ShinobiTags::State_TeleportMarked.GetTag(), ShinobiTags::State_Burning.GetTag(),
				ShinobiTags::State_Shocked.GetTag(), ShinobiTags::State_ShockBonus.GetTag(), ShinobiTags::State_Staggered.GetTag(),
				ShinobiTags::State_Chilled.GetTag(), ShinobiTags::State_AttackBoost.GetTag(), ShinobiTags::State_SpeedBoost.GetTag(),
				ShinobiTags::State_Shield.GetTag() })
			{
				Container.AddTag(Tag);
			}
			return Container;
		}();
		return Tags;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdKillTeam(
		TEXT("Shinobi.KillTeam"),
		TEXT("Dev aid (host only, PvP arena): Shinobi.KillTeam A|B kills every fighter, bots included, on that side."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AShinobiPvpGameMode* Pvp = World ? World->GetAuthGameMode<AShinobiPvpGameMode>() : nullptr;
			if (Pvp && Args.Num() == 1 && (Args[0].Equals(TEXT("A"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("B"), ESearchCase::IgnoreCase)))
			{
				Pvp->KillTeam(Args[0].Equals(TEXT("A"), ESearchCase::IgnoreCase) ? EShinobiTeam::TeamA : EShinobiTeam::TeamB);
			}
		}));

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
	SpawnPickups();
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
	if (!IsPvpTeam(Team))
	{
		return Starts; // not on a side yet (Login picks a start before PostLogin picks the side)
	}
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

bool AShinobiPvpGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	return false;
}

AActor* AShinobiPvpGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const TArray<APlayerStart*> Starts = GetTeamStarts(GetTeamOf(Player));
	if (Starts.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player); // no side yet, or an untagged map: any start
	}
	return PickFreeStart(Starts);
}

APlayerStart* AShinobiPvpGameMode::PickFreeStart(const TArray<APlayerStart*>& Starts) const
{
	// The first nobody stands on, so teammates don't spawn inside each other.
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
	return Starts.Num() > 0 ? Starts[FMath::RandRange(0, Starts.Num() - 1)] : nullptr;
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

	// Smoke hook, inert without the switch: -ShinobiPvpSmokeKill=B@5 kills team B five seconds into each fight.
	FString Spec;
	if (FParse::Value(FCommandLine::Get(), TEXT("ShinobiPvpSmokeKill="), Spec))
	{
		FString Side, Delay;
		if (Spec.Split(TEXT("@"), &Side, &Delay) && (Side == TEXT("A") || Side == TEXT("B")))
		{
			const EShinobiTeam Team = Side == TEXT("A") ? EShinobiTeam::TeamA : EShinobiTeam::TeamB;
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Team] { KillTeam(Team); }), FMath::Max(0.1f, FCString::Atof(*Delay)), false);
		}
	}
}

void AShinobiPvpGameMode::KillTeam(EShinobiTeam Team)
{
	int32 Killed = 0;
	for (AShinobiCharacterBase* Member : GetTeamMembers(Team))
	{
		if (Member->IsAlive())
		{
			Member->GetAbilitySystemComponent()->SetNumericAttributeBase(UShinobiAttributeSet::GetHealthAttribute(), 0.f);
			++Killed;
		}
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: KillTeam %s killed %d"), Team == EShinobiTeam::TeamA ? TEXT("A") : TEXT("B"), Killed);
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

void AShinobiPvpGameMode::SpawnPickups()
{
	static const TPair<FName, EShinobiPickupType> Kinds[] = {
		{ TEXT("Pickup:Health"), EShinobiPickupType::Health },
		{ TEXT("Pickup:Attack"), EShinobiPickupType::Attack },
		{ TEXT("Pickup:Speed"), EShinobiPickupType::Speed },
		{ TEXT("Pickup:Shield"), EShinobiPickupType::Shield },
	};
	TArray<const ATargetPoint*> Points;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		Points.Add(*It);
	}
	int32 Spawned = 0;
	for (const ATargetPoint* Point : Points)
	{
		for (const TPair<FName, EShinobiPickupType>& Kind : Kinds)
		{
			if (Point->ActorHasTag(Kind.Key) && AShinobiPickup::Spawn(GetWorld(), Kind.Value, Point->GetActorLocation()))
			{
				++Spawned;
			}
		}
	}
	UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: %d pickups"), Spawned);
}

void AShinobiPvpGameMode::OnRoundReset()
{
	for (TActorIterator<AShinobiPickup> It(GetWorld()); It; ++It)
	{
		It->ResetNow();
	}
	// ponytail: every bot is replaced, alive or not, rather than resetting the living ones and topping up the rest
	DespawnBots();
	FillBots();
}

void AShinobiPvpGameMode::FillBots()
{
	Bots.RemoveAll([](const TWeakObjectPtr<AShinobiBotFighter>& Bot) { return !Bot.IsValid(); });
	for (const EShinobiTeam Team : { EShinobiTeam::TeamA, EShinobiTeam::TeamB })
	{
		int32 Have = 0;
		for (const TWeakObjectPtr<AShinobiBotFighter>& Bot : Bots)
		{
			Have += Bot->GetTeam() == Team ? 1 : 0;
		}
		const int32 Wanted = FShinobiPvpMatch::SlotsPerTeam(PvpFormat) - CountHumans(Team) - Have;
		const TArray<APlayerStart*> Starts = GetTeamStarts(Team);
		for (int32 Index = 0; Index < Wanted; ++Index)
		{
			// The side's tagged starts (an untagged map: any start); the bot isn't in Teams, so it is placed here, by its side.
			const AActor* Start = Starts.Num() > 0 ? PickFreeStart(Starts) : Super::ChoosePlayerStart_Implementation(nullptr);
			if (!Start)
			{
				UE_LOG(LogShinobiCoop, Warning, TEXT("Pvp: no PlayerStart to put a bot at"));
				return;
			}
			const FVector Spot = Start->GetActorLocation();
			AShinobiBotFighter* Bot = Cast<AShinobiBotFighter>(BotSpawner.Spawn(this, AShinobiBotFighter::StaticClass(), Spot,
				Spot + Start->GetActorForwardVector(), 1));
			if (!Bot)
			{
				continue;
			}
			Bot->SetTeam(Team);
			Bots.Add(Bot);
			UE_LOG(LogShinobiCoop, Log, TEXT("Pvp: %s fills a slot on team %s"), *Bot->GetName(), Team == EShinobiTeam::TeamA ? TEXT("A") : TEXT("B"));
		}
	}
}

void AShinobiPvpGameMode::DespawnBots()
{
	// Through Bots, not BotSpawner.DespawnAll: the spawner forgets a bot when it dies, and its body lingers 3 s.
	for (const TWeakObjectPtr<AShinobiBotFighter>& Bot : Bots)
	{
		if (Bot.IsValid())
		{
			Bot->Destroy();
		}
	}
	Bots.Reset();
	BotSpawner.Prune();
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
			ASC->SetNumericAttributeBase(UShinobiAttributeSet::GetShieldAttribute(), 0.f);
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
