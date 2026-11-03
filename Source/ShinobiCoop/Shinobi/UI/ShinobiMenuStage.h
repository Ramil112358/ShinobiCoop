#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiMenuStage.generated.h"

class UCameraComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;
class UShinobiBodyComponent;
class AShinobiStudioSet;

/** Where the stage's character stands on screen, so a tab's panels don't cover it. */
enum class EShinobiMenuStageFocus : uint8 { Center, Left, Right };

/**
 * The main menu's live character (Lvl_MainMenu; Scripts/setup_menu.py places it): a hidden Manny idling, the picked roster
 * body copying its pose like on the pawn, and the picked outfit over it. Its camera is the menu's view.
 */
UCLASS()
class SHINOBICOOP_API AShinobiMenuStage : public AActor
{
	GENERATED_BODY()

public:
	AShinobiMenuStage();

	/** Wears Roster[RosterIndex] (the hero when it isn't imported) with its Face-th face, and Outfits[OutfitIndex]. */
	void Show(int32 RosterIndex, int32 OutfitIndex, int32 Face = 0);
	/** Wears a creator look (its frame, face, hair and clothes) and Outfits[OutfitIndex] fitted to that frame; the hero
	 * when the creator catalog isn't built. Live slider edits go to GetBody()->UpdateFace / UpdateBody instead. */
	void ShowAppearance(const FShinobiAppearance& Look, int32 OutfitIndex);
	int32 GetShownBody() const { return ShownBody; }
	/** The look last given to ShowAppearance (its build may still be pending). */
	const FShinobiAppearance& GetShownLook() const { return ShownLook; }
	/** Head and shoulders of the body worn, for a portrait (a 3:4 view); the character stands centred at once. */
	void FramePortrait();
	/**
	 * Films a made character's portrait off screen (the menu's own view doesn't move): head and shoulders like the roster's
	 * (FramePortrait's lens, the face at ShinobiMenuArt::MadePortraitFaceV), from the front, without the outfit, against
	 * whatever stands behind (the studio in the creator, else the menu's backdrop). It waits until Look is worn (built, with
	 * no live edit since) and the character has turned to face front, lets it settle, then hands Done the picture as a PNG,
	 * or an empty one when it can't be had (the look isn't being built, the picture can't be read). Showing another
	 * character, another request or CancelPortrait drops it without calling Done.
	 */
	void RequestPortrait(const FShinobiAppearance& Look, TFunction<void(TArray<uint8>&&)> Done);
	void CancelPortrait();
	bool IsPortraitPending() const { return bPortraitPending; }
	/** The look RequestPortrait waits for (meaningful while IsPortraitPending). */
	const FShinobiAppearance& GetPortraitLook() const { return PortraitLook; }
	/** A made character's portrait size in pixels (3:4, like the roster cards). */
	static constexpr int32 PortraitWidth = 384;
	static constexpr int32 PortraitHeight = 512;
	/** Eases the character sideways to Focus's spot. */
	void SetFocus(EShinobiMenuStageFocus Focus);
	/** Turns the character (not the camera) by Degrees, easing there. */
	void AddYaw(float Degrees);
	/** Zooms by Delta: 0 frames the whole character, 1 the face (the aim sliding from chest to face). Clamped. */
	void AddZoom(float Delta);
	/** A tab's framing (the creator's FACE: 1, else 0); ResetView returns to it. */
	void SetAutoZoom(float Zoom);
	/** Facing front at the last SetAutoZoom framing. */
	void ResetView();
	/** Moves the stage (character and camera) into the level's studio set, or back where it stood; without a studio set it
	 * stays. */
	void SetStudio(bool bInStudio);
	bool IsInStudio() const { return bInStudio; }
	/** The level's studio set (the creator), or null. */
	AShinobiStudioSet* GetStudio() const;
	float GetZoom() const { return Zoom; }
	float GetYaw() const { return Yaw; }

	UCameraComponent* GetCamera() const { return Camera; }
	UShinobiBodyComponent* GetBody() const { return Body; }
	USkeletalMeshComponent* GetOutfit() const { return Outfit; }
	USkeletalMeshComponent* GetDriver() const { return Driver; }

	/** Sideways offset of the character for Left / Right focus (cm along the camera's right). */
	static constexpr float FocusOffsetCm = 110.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** The hidden Manny idling, which the worn body copies. */
	void StartDriver();
	/** The worn body's head bone height above the stage (cm). */
	float HeadHeight() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Driver;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UShinobiBodyComponent> Body;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Outfit;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;
	/** RequestPortrait's camera. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneCaptureComponent2D> PortraitCapture;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PortraitTarget;

	/** RequestPortrait: waits for the look, warms the capture up, then takes the picture. */
	void TickPortrait(float DeltaSeconds);
	/** Puts PortraitCapture in front of the worn face. */
	void PlacePortraitCapture();
	/** PortraitTarget's pixels as a PNG (empty if it can't be read). */
	TArray<uint8> ReadPortrait() const;

	/** Places the camera for Zoom (between the full view and the face close-up). */
	void PlaceCamera();

	int32 ShownBody = INDEX_NONE;
	float TargetOffset = 0.f;
	float Yaw = 0.f;
	float TargetYaw = 0.f;
	float Zoom = 0.f;
	float TargetZoom = 0.f;
	float AutoZoom = 0.f;
	/** FramePortrait's view holds until the view is moved. */
	bool bPortrait = false;
	bool bInStudio = false;
	FShinobiAppearance ShownLook;
	bool bPortraitPending = false;
	FShinobiAppearance PortraitLook;
	TFunction<void(TArray<uint8>&&)> PortraitDone;
	/** Seconds the requested look has stood ready (none: it isn't). */
	float PortraitReadyTime = 0.f;
	/** Seconds the requested look has been neither worn nor being built (it won't be: give up). */
	float PortraitStuckTime = 0.f;
	/** Where the stage stood before it went into the studio. */
	FTransform Home;
};
