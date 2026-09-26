#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "ShinobiSessionSubsystem.generated.h"

/** A map Host Game can open. */
struct FShinobiMap
{
	const TCHAR* DisplayName;
	const TCHAR* Path;
};

/** One joinable game from Find Games. */
struct FShinobiFoundGame
{
	FString HostName;
	FString MapName;
	int32 Players = 0;
	int32 MaxPlayers = 0;
	int32 PingMs = 0;
};

/**
 * Host / find / join / leave over the online subsystem (Steam; the Null subsystem, LAN, when Steam isn't running),
 * plus accepted Steam invites and network or travel failures, which return to the main menu with a message. Lives on
 * the game instance, so the message survives the travel back to the menu.
 */
UCLASS()
class SHINOBICOOP_API UShinobiSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxPlayers = 4;
	/** Session setting every ShinobiCoop game carries: Steam app 480 is shared by every developer's test games. */
	static const FName GameKey;
	static const TCHAR* MenuMap;
	/** Session setting carrying the map's display name, for Find Games. */
	static const FName MapKey;

	/** The maps Host Game offers; the first is the default. */
	static const TArray<FShinobiMap>& GetMaps();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True on the Null subsystem: Steam isn't running (or -nosteam), so games are LAN-only. */
	bool IsLanMode() const;

	/** A session operation is in flight (the menu disables its buttons). */
	bool IsBusy() const { return bBusy; }

	/** Hosts on GetMaps()[MapIndex]. */
	void HostGame(int32 MapIndex = 0);
	void FindGames();
	/** Index into GetFoundGames(). */
	void JoinGame(int32 Index);
	/** Ends the session (a host's clients drop back to their menus) and opens the main menu. */
	void LeaveGame();
	/** Steam overlay invite dialog for the current session. False when there's no overlay (LAN mode). */
	bool ShowInviteUI();

	const TArray<FShinobiFoundGame>& GetFoundGames() const { return FoundGames; }

	/** Status or failure for the menu ("Searching...", "Host left", ...); empty when there's nothing to say. */
	const FString& GetMessage() const { return Message; }

	/** Anything the menu shows changed. */
	FSimpleMulticastDelegate OnChanged;

private:
	IOnlineSessionPtr GetSessions() const;
	void SetMessage(const FString& NewMessage, bool bNowBusy);
	/** Runs Then once no session named NAME_GameSession exists (destroying a leftover one first). */
	void WithoutSession(TFunction<void()> Then);
	void Join(const FOnlineSessionSearchResult& Result);

	void OnCreated(FName SessionName, bool bOk);
	void OnFound(bool bOk);
	void OnJoined(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroyed(FName SessionName, bool bOk);
	void OnInviteAccepted(bool bOk, int32 ControllerId, FUniqueNetIdPtr UserId, const FOnlineSessionSearchResult& Result);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void OnMapLoaded(UWorld* World);
	void ReturnToMenu(const FString& Reason);

	TSharedPtr<FOnlineSessionSearch> Search;
	TArray<FOnlineSessionSearchResult> Results;
	TArray<FShinobiFoundGame> FoundGames;
	TFunction<void()> AfterDestroy;
	int32 HostMapIndex = 0;
	FString Message;
	bool bBusy = false;
	/** Set on a disconnect: the engine loads GameDefaultMap, and the next map load goes on to the menu. */
	bool bReturnToMenuOnLoad = false;
	TArray<FDelegateHandle> EngineHandles;
};
