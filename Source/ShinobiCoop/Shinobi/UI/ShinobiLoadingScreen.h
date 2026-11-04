#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "ShinobiLoadingScreen.generated.h"

class IInputProcessor;
class SWidget;
class UGameViewportClient;
class UTexture2D;
struct FShinobiLoadingView;
struct FWorldContext;

/**
 * The loading screen (docs/superpowers/specs/2026-11-04-loading-screen-design.md): the destination map's art, name, mode
 * and blurb, a rotating gameplay tip, and in the corner a spinner, what is being waited for and a progress bar. It goes up
 * when travel starts and stays until the world is actually ready to play, not just loaded, so the player never sees a
 * frozen frame, an empty world popping in, or a hitch while shaders compile.
 */
namespace ShinobiLoadingScreen
{
	/** What the screen is waiting for, in the order it waits. */
	enum class EStep : uint8
	{
		Connecting,       // a pending net game: reaching the host
		LoadingMap,       // travel requested or the map loading (the engine blocks here)
		StartingGame,     // world loaded, game not begun, no game state yet, or the local player has no pawn yet
		StreamingWorld,   // streaming levels / World Partition cells around the player
		PreparingShaders, // shader compiles, pipeline cache precompiles and PSO precaching
		PreparingCharacters, // Mutable bodies still building
		StreamingTextures, // textures still wanting mips (only held for a few seconds)
		Ready,
	};

	/** One tick's look at the world (gathered by the subsystem; plain data so tests can build it). */
	struct FChecks
	{
		bool bConnecting = false;
		bool bTravelling = false;
		bool bWorldStarted = false;
		bool bWaitingForPawn = false;
		int32 LevelsStreaming = 0;
		int32 ShadersCompiling = 0;
		/** The most shaders seen pending on this screen, for the bar (0: none seen). */
		int32 ShadersPeak = 0;
		int32 CharactersBuilding = 0;
		int32 TexturesStreaming = 0;
		/** Texture streaming has had its time (TextureHoldSeconds): don't wait for it any more. */
		bool bTexturesTimedOut = false;
	};

	struct FStatus
	{
		EStep Step = EStep::LoadingMap;
		/** 0-1, only ever rises within one screen (Evaluate is monotonic in Step; the bar never runs backwards). */
		float Progress = 0.f;
		/** "LOADING MAP", "COMPILING SHADERS"... */
		FText Label;
		/** "212 left", "Waiting for the host"..., or empty. */
		FText Detail;
	};

	SHINOBICOOP_API FStatus Evaluate(const FChecks& Checks);

	/** The tips shown for a map (co-op or arena), with the player's current bindings in place of {Dodge}-style tokens. */
	SHINOBICOOP_API TArray<FText> Tips(bool bArena);

	/** GetMaps() index of the map at PackagePath ("/Game/Shinobi/Maps/Lvl_London", a PIE "UEDPIE_0_Lvl_London", or with
	 * ".Lvl_London" on the end), or INDEX_NONE (the main menu, an unknown map). */
	SHINOBICOOP_API int32 FindMapByPath(const FString& PackagePath);

	/** Never take the screen down sooner than this after it went up (no one-frame flash on a quick load). */
	constexpr double MinShowSeconds = 1.0;
	/** Once everything is ready, a few more frames under the screen so the first visible frame is a settled one. */
	constexpr double SettleSeconds = 0.35;
	constexpr double FadeSeconds = 0.45;
	/** How long the screen waits for the local pawn after the game began (spectating, a slow spawn). */
	constexpr double PawnWaitSeconds = 10.0;
	/** How long texture streaming may hold the screen once everything else is ready. */
	constexpr double TextureHoldSeconds = 4.0;
	/** Safety: after the map loaded, the screen comes down after this however much is still pending (logged). */
	constexpr double MaxHoldSeconds = 45.0;
	/** Seconds each tip is shown. */
	constexpr double TipSeconds = 8.0;
}

/**
 * Shows the loading screen through travel. Travel started by the session subsystem calls Begin (so the destination
 * and mode are known while still connecting); any other map load (rematch, disconnect, returning to the menu, the first
 * map at start-up) brings it up from PreLoadMap. In a packaged or -game build the engine's MoviePlayer draws it on the
 * Slate loading thread during the blocking map load, so it keeps animating; after the load (and in PIE, where there is no
 * MoviePlayer) the same screen sits on top of the game viewport until EvaluateNow says Ready. The world renders behind it
 * meanwhile, which is what compiles its shaders and streams its textures. Input to the game is held while it is up
 * (outside the editor). Off with Shinobi.LoadingScreen 0, -NoLoadingScreen, -unattended (the shot and smoke scripts),
 * -nullrhi and automation tests.
 */
UCLASS()
class SHINOBICOOP_API UShinobiLoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Brings the screen up for travel to MapPath (a package path; empty or the menu: the main menu). Format is "1v1",
	 * "2v2" or empty (co-op, or not known). Already up for the same map: keeps it (and its format, unless a new one is
	 * given), so a rematch or a reconnect doesn't restart the tips. */
	void Begin(const FString& MapPath, const FString& Format = FString());

	bool IsShowing() const { return bShowing; }
	/** What the screen is on now (Ready while fading out or not showing). */
	ShinobiLoadingScreen::FStatus GetStatus() const { return Status; }
	/** False when the screen is switched off (see the class comment). */
	static bool IsEnabled();

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	void OnPreLoadMap(const FWorldContext& Context, const FString& MapName);
	void OnPostLoadMap(UWorld* World);

	/** A fresh view (art, title, tips) for MapPath / Format, and a fresh widget showing it. */
	void BuildView();
	void ShowWidget();
	void HideWidget();
	void SetInputBlocked(bool bBlocked);
	ShinobiLoadingScreen::FChecks GatherChecks();
	void UpdateStatus(double Now);

	TSharedPtr<FShinobiLoadingView> View;
	TSharedPtr<SWidget> Widget;
	TWeakObjectPtr<UGameViewportClient> WidgetViewport;
	TSharedPtr<IInputProcessor> InputBlocker;

	/** The map art the view's brush draws (kept from GC across the map load). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Art;

	FString MapPath;
	FString Format;
	ShinobiLoadingScreen::FStatus Status;

	bool bShowing = false;
	bool bFading = false;
	/** Between PreLoadMap and PostLoadMap: the engine is loading the map. */
	bool bInLoadMap = false;
	/** This subsystem started the MoviePlayer for the current load and finishes it after. */
	bool bOwnMovie = false;
	double ShownAt = 0.0;
	/** When the map was loaded (0 while travelling). */
	double LoadedAt = 0.0;
	/** When the game began with the map loaded (0 until then). */
	double StartedAt = 0.0;
	/** When everything but texture streaming was ready (0 until then). */
	double AlmostReadyAt = 0.0;
	double ReadyAt = 0.0;
	double FadeFrom = 0.0;
	int32 ShadersPeak = 0;
	bool bLoggedTimeout = false;
};
