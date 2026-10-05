#include "Shinobi/Online/ShinobiSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "ShinobiCoop.h"
#include "UObject/UObjectGlobals.h"

const FName UShinobiSessionSubsystem::GameKey(TEXT("SHINOBICOOP"));
const TCHAR* UShinobiSessionSubsystem::MenuMap = TEXT("/Game/Shinobi/Maps/Lvl_MainMenu");
const FName UShinobiSessionSubsystem::MapKey(TEXT("SHINOBIMAP"));
const FName UShinobiSessionSubsystem::FormatKey(TEXT("SHINOBIFORMAT"));

const TArray<FShinobiMap>& UShinobiSessionSubsystem::GetMaps()
{
	static const TArray<FShinobiMap> Maps = {
		{ TEXT("Hidden Leaf Valley"), TEXT("/Game/Shinobi/Maps/Lvl_HiddenLeafValley") },
		{ TEXT("London"), TEXT("/Game/Shinobi/Maps/Lvl_London") },
		{ TEXT("Training Grounds"), TEXT("/Game/Shinobi/Maps/Lvl_TrainingGrounds") },
		{ TEXT("Test Level"), TEXT("/Game/ThirdPerson/Lvl_ThirdPerson") },
		{ TEXT("Combat Level"), TEXT("/Game/Variant_Combat/Lvl_Combat") },
		// Appended after the co-op maps so their indices (and the default) don't move.
		{ TEXT("Arena: West End"), TEXT("/Game/Shinobi/Maps/Lvl_Arena_WestEnd"), true },
		{ TEXT("Arena: Camden"), TEXT("/Game/Shinobi/Maps/Lvl_Arena_Camden"), true },
		{ TEXT("Arena: Westminster"), TEXT("/Game/Shinobi/Maps/Lvl_Arena_Westminster"), true },
		{ TEXT("Arena: Tower"), TEXT("/Game/Shinobi/Maps/Lvl_Arena_Tower"), true },
	};
	return Maps;
}

FString UShinobiSessionSubsystem::MakeTravelOptions(const FShinobiMap& Map, EShinobiPvpFormat Format)
{
	if (!Map.bArena)
	{
		return TEXT("listen");
	}
	return FString(TEXT("listen?game=/Script/ShinobiCoop.ShinobiPvpGameMode?Format="))
		+ (Format == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : TEXT("1v1"));
}

void UShinobiSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->OnCreateSessionCompleteDelegates.AddUObject(this, &UShinobiSessionSubsystem::OnCreated);
		Sessions->OnFindSessionsCompleteDelegates.AddUObject(this, &UShinobiSessionSubsystem::OnFound);
		Sessions->OnJoinSessionCompleteDelegates.AddUObject(this, &UShinobiSessionSubsystem::OnJoined);
		Sessions->OnDestroySessionCompleteDelegates.AddUObject(this, &UShinobiSessionSubsystem::OnDestroyed);
		Sessions->OnSessionUserInviteAcceptedDelegates.AddUObject(this, &UShinobiSessionSubsystem::OnInviteAccepted);
	}
	if (GEngine)
	{
		EngineHandles.Add(GEngine->OnNetworkFailure().AddUObject(this, &UShinobiSessionSubsystem::OnNetworkFailure));
		EngineHandles.Add(GEngine->OnTravelFailure().AddUObject(this, &UShinobiSessionSubsystem::OnTravelFailure));
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UShinobiSessionSubsystem::OnMapLoaded);
	UE_LOG(LogShinobiCoop, Log, TEXT("Online subsystem: %s"), IsLanMode() ? TEXT("Null (LAN)") : TEXT("Steam"));
}

void UShinobiSessionSubsystem::Deinitialize()
{
	if (const IOnlineSessionPtr Sessions = GetSessions())
	{
		Sessions->OnCreateSessionCompleteDelegates.RemoveAll(this);
		Sessions->OnFindSessionsCompleteDelegates.RemoveAll(this);
		Sessions->OnJoinSessionCompleteDelegates.RemoveAll(this);
		Sessions->OnDestroySessionCompleteDelegates.RemoveAll(this);
		Sessions->OnSessionUserInviteAcceptedDelegates.RemoveAll(this);
	}
	if (GEngine && EngineHandles.Num() == 2)
	{
		GEngine->OnNetworkFailure().Remove(EngineHandles[0]);
		GEngine->OnTravelFailure().Remove(EngineHandles[1]);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	Super::Deinitialize();
}

IOnlineSessionPtr UShinobiSessionSubsystem::GetSessions() const
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	return Online ? Online->GetSessionInterface() : nullptr;
}

bool UShinobiSessionSubsystem::IsLanMode() const
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	return !Online || Online->GetSubsystemName() == NULL_SUBSYSTEM;
}

void UShinobiSessionSubsystem::SetMessage(const FString& NewMessage, bool bNowBusy)
{
	Message = NewMessage;
	bBusy = bNowBusy;
	if (!Message.IsEmpty())
	{
		UE_LOG(LogShinobiCoop, Log, TEXT("Session: %s"), *Message);
	}
	OnChanged.Broadcast();
}

void UShinobiSessionSubsystem::WithoutSession(TFunction<void()> Then)
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		AfterDestroy = MoveTemp(Then);
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	Then();
}

void UShinobiSessionSubsystem::HostGame(int32 MapIndex, EShinobiPvpFormat Format)
{
	HostMapIndex = GetMaps().IsValidIndex(MapIndex) ? MapIndex : 0;
	// Co-op has no format; an arena with none given is 1v1, which is also what the game mode defaults to.
	HostFormat = !GetMaps()[HostMapIndex].bArena ? EShinobiPvpFormat::None
		: Format == EShinobiPvpFormat::TwoVsTwo ? EShinobiPvpFormat::TwoVsTwo : EShinobiPvpFormat::OneVsOne;
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || bBusy)
	{
		SetMessage(Sessions ? Message : TEXT("No online subsystem"), bBusy);
		return;
	}
	SetMessage(TEXT("Creating game..."), true);
	WithoutSession([this]
	{
		FOnlineSessionSettings Settings;
		Settings.NumPublicConnections = HostFormat == EShinobiPvpFormat::OneVsOne ? 2 : MaxPlayers;
		Settings.bShouldAdvertise = true;
		Settings.bAllowJoinInProgress = true;
		Settings.bIsLANMatch = IsLanMode();
		Settings.bUsesPresence = true;
		Settings.bUseLobbiesIfAvailable = true;
		Settings.bAllowJoinViaPresence = true;
		Settings.bAllowInvites = true;
		Settings.Set(GameKey, true, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(MapKey, FString(GetMaps()[HostMapIndex].DisplayName), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings.Set(FormatKey, FString(HostFormat == EShinobiPvpFormat::TwoVsTwo ? TEXT("2v2") : HostFormat == EShinobiPvpFormat::OneVsOne ? TEXT("1v1") : TEXT("")),
			EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		if (!GetSessions()->CreateSession(0, NAME_GameSession, Settings))
		{
			SetMessage(TEXT("Couldn't create the game"), false);
		}
	});
}

void UShinobiSessionSubsystem::OnCreated(FName SessionName, bool bOk)
{
	if (!bOk)
	{
		SetMessage(TEXT("Couldn't create the game"), false);
		return;
	}
	SetMessage(FString(), false);
	UGameplayStatics::OpenLevel(GetWorld(), FName(GetMaps()[HostMapIndex].Path), true, MakeTravelOptions(GetMaps()[HostMapIndex], HostFormat));
}

void UShinobiSessionSubsystem::FindGames()
{
	const IOnlineSessionPtr Sessions = GetSessions();
	if (!Sessions || bBusy)
	{
		return;
	}
	Results.Empty();
	FoundGames.Empty();
	Search = MakeShared<FOnlineSessionSearch>();
	Search->bIsLanQuery = IsLanMode();
	Search->MaxSearchResults = 200; // app 480's lobby list is shared with everyone's test games; ours are filtered below
	Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SetMessage(TEXT("Searching..."), true);
	if (!Sessions->FindSessions(0, Search.ToSharedRef()))
	{
		SetMessage(TEXT("Search failed"), false);
	}
}

void UShinobiSessionSubsystem::OnFound(bool bOk)
{
	Results.Empty();
	FoundGames.Empty();
	if (bOk && Search.IsValid())
	{
		for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
		{
			bool bOurs = false;
			if (!Result.IsValid() || !Result.Session.SessionSettings.Get(GameKey, bOurs) || !bOurs)
			{
				continue;
			}
			const int32 Max = Result.Session.SessionSettings.NumPublicConnections;
			FString MapName;
			Result.Session.SessionSettings.Get(MapKey, MapName);
			FString Format;
			Result.Session.SessionSettings.Get(FormatKey, Format);
			FoundGames.Add({ Result.Session.OwningUserName, MapName, Max - Result.Session.NumOpenPublicConnections, Max, Result.PingInMs, Format });
			Results.Add(Result);
		}
	}
	SetMessage(!bOk ? TEXT("Search failed") : FoundGames.IsEmpty() ? TEXT("No games found") : FString(), false);
}

void UShinobiSessionSubsystem::JoinGame(int32 Index)
{
	if (Results.IsValidIndex(Index) && !bBusy)
	{
		Join(Results[Index]);
	}
}

void UShinobiSessionSubsystem::Join(const FOnlineSessionSearchResult& Result)
{
	SetMessage(TEXT("Joining..."), true);
	WithoutSession([this, Result]
	{
		if (!GetSessions()->JoinSession(0, NAME_GameSession, Result))
		{
			SetMessage(TEXT("Couldn't join the game"), false);
		}
	});
}

void UShinobiSessionSubsystem::OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	FString Address;
	const IOnlineSessionPtr Sessions = GetSessions();
	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions || !Sessions->GetResolvedConnectString(SessionName, Address))
	{
		const TCHAR* Reason = Result == EOnJoinSessionCompleteResult::SessionIsFull ? TEXT("The game is full")
			: Result == EOnJoinSessionCompleteResult::SessionDoesNotExist ? TEXT("The game no longer exists")
			: TEXT("Couldn't join the game");
		SetMessage(Reason, false);
		return;
	}
	SetMessage(FString(), false);
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		PC->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void UShinobiSessionSubsystem::OnDestroyed(FName SessionName, bool bOk)
{
	if (TFunction<void()> Then = MoveTemp(AfterDestroy))
	{
		AfterDestroy = nullptr;
		Then();
	}
}

void UShinobiSessionSubsystem::OnInviteAccepted(bool bOk, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result)
{
	if (bOk && Result.IsValid())
	{
		Join(Result);
	}
	else
	{
		SetMessage(TEXT("Couldn't accept the invite"), false);
	}
}

void UShinobiSessionSubsystem::LeaveGame()
{
	WithoutSession([this] { ReturnToMenu(FString()); });
}

bool UShinobiSessionSubsystem::ShowInviteUI()
{
	const IOnlineSubsystem* Online = Online::GetSubsystem(GetWorld());
	const IOnlineExternalUIPtr ExternalUI = Online ? Online->GetExternalUIInterface() : nullptr;
	return !IsLanMode() && ExternalUI.IsValid() && ExternalUI->ShowInviteUI(0, NAME_GameSession);
}

void UShinobiSessionSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		return; // a client dropping is not the host's failure; the game mode cleans up after it
	}
	const FString Reason = FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout
		? TEXT("Lost the connection: the host left or the network dropped")
		: FString::Printf(TEXT("Network error: %s"), *Error);
	// The engine now travels to GameDefaultMap; OnMapLoaded continues to the menu if that isn't it.
	bReturnToMenuOnLoad = true;
	WithoutSession([] {});
	SetMessage(Reason, false);
}

void UShinobiSessionSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	bReturnToMenuOnLoad = true;
	WithoutSession([] {});
	SetMessage(FString::Printf(TEXT("Couldn't reach the game: %s"), *Error), false);
}

void UShinobiSessionSubsystem::OnMapLoaded(UWorld* World)
{
	if (!bReturnToMenuOnLoad || !World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	bReturnToMenuOnLoad = false;
	if (!World->GetOutermost()->GetName().EndsWith(TEXT("Lvl_MainMenu")))
	{
		UGameplayStatics::OpenLevel(World, FName(MenuMap));
	}
}

void UShinobiSessionSubsystem::ReturnToMenu(const FString& Reason)
{
	SetMessage(Reason, false);
	UGameplayStatics::OpenLevel(GetWorld(), FName(MenuMap));
}
