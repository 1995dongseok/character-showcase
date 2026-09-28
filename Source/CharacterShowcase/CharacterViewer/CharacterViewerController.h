#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/PlayerController.h"
#include "CharacterViewerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UCharacterViewerWidget;
class APortfolioCharacterActor;
class ACharacterViewerCameraPawn;
class UCharacterProfileData;
struct FInputActionValue;

// P0-1/P0-4/P1-2/P1-6: owns input state, UI display, drag/click judgement and
// forwards user intent to the Actor/Pawn/Widget. See
// Docs/CHARACTER_VIEWER_SETUP.md section 4 for the responsibility split.
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ACharacterViewerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACharacterViewerController();

	// --- Editor-authored input assets (optional: see bCreateFallbackInputAssets) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	// Axis2D: mouse delta while OrbitPressAction is held.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> OrbitAction;

	// Bool: left mouse button held, gates OrbitAction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> OrbitPressAction;

	// Axis1D: mouse wheel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	// Bool: R.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ResetCameraAction;

	// Bool: Space.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleTurntableAction;

	// Bool: H.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleCleanViewAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCharacterViewerWidget> WidgetClass;

	// Minimum pixel movement after a left-press before it counts as an Orbit drag instead of a click.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	float DragThresholdPixels = 6.f;

	// If true (default) and any of the input action/context properties above
	// are null at input setup, transient runtime UInputAction/
	// UInputMappingContext objects are created and mapped instead of relying
	// on Editor-authored assets. See Docs/CHARACTER_VIEWER_SETUP.md section 13
	// for the exact fallback key bindings and the Editor asset spec they mirror.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bCreateFallbackInputAssets = true;

	// --- Public API used by CharacterViewerWidget button/list clicks ---

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SelectCameraPreset(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SelectAnimation(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SelectExpression(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SelectMaterialVariant(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SetTurntableEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void ToggleTurntable();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void ToggleCleanView();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void ResetCamera();

	// Applies NewProfile to the viewer actor, reframes the camera to the new
	// profile's reset framing, and rebinds/notifies the widget.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SwitchProfile(UCharacterProfileData* NewProfile);

	// Preferred way to give the controller its viewer actor (called by
	// ACharacterViewerGameMode); falls back to TActorIterator in BeginPlay if never called.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SetViewerActor(APortfolioCharacterActor* InActor);

	// --- Read-only accessors (mainly for automation tests; see Tests/CharacterViewerGameSmokeTest.cpp) ---

	UFUNCTION(BlueprintPure, Category = "Viewer")
	APortfolioCharacterActor* GetViewerActor() const { return ViewerActor; }

	UFUNCTION(BlueprintPure, Category = "Viewer")
	ACharacterViewerCameraPawn* GetCameraPawn() const { return CameraPawn; }

	UFUNCTION(BlueprintPure, Category = "Viewer")
	UCharacterViewerWidget* GetViewerWidget() const { return ViewerWidget; }

	// False when zero or more-than-one APortfolioCharacterActor was found in the level (see FindViewerActorIfNeeded()/SetViewerActor()).
	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsInputEnabled() const { return bInputEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	void EnsureFallbackInputAssets();
	void EnsureWidgetCreated();
	void FindViewerActorIfNeeded();
	void ApplyFramingForCurrentActor(bool bInstant);
	void ReleaseDrag();

	void HandleOrbitPressStarted(const FInputActionValue& Value);
	void HandleOrbitPressCompleted(const FInputActionValue& Value);
	void HandleOrbitAxis(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleResetCamera(const FInputActionValue& Value);
	void HandleToggleTurntable(const FInputActionValue& Value);
	void HandleToggleCleanView(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<APortfolioCharacterActor> ViewerActor;

	UPROPERTY()
	TObjectPtr<ACharacterViewerCameraPawn> CameraPawn;

	UPROPERTY()
	TObjectPtr<UCharacterViewerWidget> ViewerWidget;

	// False when zero or more-than-one APortfolioCharacterActor is found in the level; input is safely disabled.
	bool bInputEnabled = true;

	bool bIsPressed = false;
	bool bIsDragging = false;
	FVector2D PressScreenPosition = FVector2D::ZeroVector;

	bool bCleanViewActive = false;
	ESlateVisibility PreCleanViewVisibility = ESlateVisibility::Visible;
	bool bPreCleanViewShowCursor = true;
};
