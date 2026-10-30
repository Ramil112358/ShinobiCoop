#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shinobi/Hero/ShinobiCreatorCatalog.h"
#include "ShinobiMenuStage.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UShinobiBodyComponent;

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
	/** Head and shoulders of the body worn, for a portrait (a 3:4 view); the character stands centred at once. */
	void FramePortrait();
	/** Eases the character sideways to Focus's spot. */
	void SetFocus(EShinobiMenuStageFocus Focus);
	/** Turns the character (not the camera) by Degrees, easing there. */
	void AddYaw(float Degrees);
	/** Zooms by Delta: 0 frames the whole character, 1 the face (the aim sliding from chest to face). Clamped. */
	void AddZoom(float Delta);
	/** A tab's framing (CREATE's FACE: 1, else 0); ResetView returns to it. */
	void SetAutoZoom(float Zoom);
	/** Facing front at the last SetAutoZoom framing. */
	void ResetView();
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
};
