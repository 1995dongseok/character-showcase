#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DemoPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class ADemoCharacter;
struct FInputActionValue;

// D1: Enhanced Input -> ADemoCharacter. Same runtime-fallback pattern as
// ACharacterViewerController::EnsureFallbackInputAssets(): when an
// Editor-authored asset reference is null, a transient IA/IMC is created.
// Keys: WASD move, LeftShift run, mouse look, wheel zoom, R reset camera,
// Backspace respawn, Esc toggle cursor (cursor mode ignores move/look).
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ADemoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADemoPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	// Axis2D: X = right, Y = forward.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// Bool: LeftShift, held.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RunAction;

	// Axis2D: Mouse2D. X = right, Y = up (positive Y raises the camera pitch).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// Axis1D: mouse wheel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	// Bool: R.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ResetCameraAction;

	// Bool: Backspace.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> RespawnAction;

	// Bool: Escape.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleCursorAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	bool bCreateFallbackInputAssets = true;

	// Degrees of camera rotation per unit of Look input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float LookSensitivity = 0.3f;

	// Optional short control hint. The fallback UI is a D2 item: when unset nothing is drawn (logged once).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> HintWidgetClass;

	// Cursor mode: cursor shown, GameAndUI input, move/look/zoom ignored and the character stopped.
	UFUNCTION(BlueprintCallable, Category = "Play")
	void SetCursorMode(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Play")
	void ToggleCursorMode();

	UFUNCTION(BlueprintPure, Category = "Play")
	bool IsCursorMode() const { return bCursorMode; }

	// Stops movement/run and releases all held keys (focus loss, cursor mode).
	// Afterwards OS key-repeat events are ignored for every key until that key
	// is pressed anew (see InputKey), so a key still held is not resumed.
	UFUNCTION(BlueprintCallable, Category = "Play")
	void ReleaseAllInput();

	// True after ReleaseAllInput() (focus loss / cursor mode): IE_Repeat of a
	// key that has not had a fresh IE_Pressed since then is dropped.
	UFUNCTION(BlueprintPure, Category = "Play")
	bool IsIgnoringRepeatUntilPress() const { return bIgnoreRepeatUntilPress; }

	// Camera state right after this controller's BeginPlay (the composition the
	// player first sees). -1 arm when there was no demo character.
	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float GetBeginPlayCameraPitch() const { return BeginPlayCameraPitch; }

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float GetBeginPlayArmLength() const { return BeginPlayArmLength; }

	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	UFUNCTION(BlueprintPure, Category = "Play")
	ADemoCharacter* GetDemoCharacter() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

private:
	void EnsureFallbackInputAssets();
	void HandleApplicationActivationStateChanged(bool bIsActive);

	void HandleMove(const FInputActionValue& Value);
	void HandleRunStarted(const FInputActionValue& Value);
	void HandleRunCompleted(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleResetCamera(const FInputActionValue& Value);
	void HandleRespawn(const FInputActionValue& Value);
	void HandleToggleCursor(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UUserWidget> HintWidget;

	bool bCursorMode = false;

	// Enhanced Input treats IE_Repeat as "key down" (and UPlayerInput re-creates
	// a pressed state from the first repeat after FlushPressedKeys), so a key
	// held through a focus loss would resume moving on focus regain without this.
	bool bIgnoreRepeatUntilPress = false;
	TSet<FKey> KeysPressedSinceRelease;

	float BeginPlayCameraPitch = 0.f;
	float BeginPlayArmLength = -1.f;

	FDelegateHandle ApplicationActivationStateChangedHandle;
};
