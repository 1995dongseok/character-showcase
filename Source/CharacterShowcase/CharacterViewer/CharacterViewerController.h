#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/PlayerController.h"
#include "CharacterViewer/ViewerCapture.h"
#include "CharacterViewer/ViewerBatchCapture.h"
#include "CharacterViewerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UCharacterViewerWidget;
class APortfolioCharacterActor;
class ACharacterViewerCameraPawn;
class UCharacterProfileData;
class UGameViewportClient;
struct FInputActionValue;
class UMaterialInstanceDynamic;
class UMeshComponent;

// Backdrop/floor colour presets for silhouette checks (B key, DISPLAY row
// "Backdrop: <name> (B)"). Studio = the level as authored (the MI parameter
// values captured the first time a preset is applied).
UENUM(BlueprintType)
enum class EViewerBackdropPreset : uint8
{
	Studio,
	Black,
	White,
	MidGrey
};

// One material slot recoloured by ACharacterViewerController::SetBackdropPreset()
// (backdrop sphere or floor). Runtime-only bookkeeping, not reflected.
struct FViewerBackdropTarget
{
	TWeakObjectPtr<UMeshComponent> Component;
	int32 SlotIndex = 0;
	TWeakObjectPtr<UMaterialInstanceDynamic> Material;
	bool bIsFloor = false;
	// Parameter values as authored (restored by the Studio preset).
	FLinearColor OriginalA = FLinearColor::Black;	// TopColor / BaseColor
	FLinearColor OriginalB = FLinearColor::Black;	// BottomColor / EdgeColor
};

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

	// Bool: I (P2-1 Inspection toggle).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleInspectionAction;

	// Bool: W (P2-4 Wireframe toggle).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ToggleWireframeAction;

	// --- Portfolio capture input (Docs/CHARACTER_VIEWER_SETUP.md section 1.7) ---

	// Bool: F12 -> TakePortfolioScreenshot().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Capture")
	TObjectPtr<UInputAction> ScreenshotAction;

	// Bool: Shift+F12 -> StartTurntableCapture(). The fallback maps F12 with a
	// UInputTriggerChordAction on CaptureShiftAction, so Enhanced Input's
	// automatic chord blocker keeps the plain F12 mapping from also firing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Capture")
	TObjectPtr<UInputAction> TurntableCaptureAction;

	// Bool: Left/Right Shift; only used as the chord for TurntableCaptureAction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Capture")
	TObjectPtr<UInputAction> CaptureShiftAction;

	// Bool: Escape -> CancelCapture() (no-op when nothing is being captured).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Capture")
	TObjectPtr<UInputAction> CancelCaptureAction;

	// --- Animation playback / LOD / backdrop input (section 1.7) ---

	// Bool: P -> ToggleAnimationPaused().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> ToggleAnimationPauseAction;

	// Bool: [ -> StepAnimationFrames(-1) (pauses).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> StepAnimationBackAction;

	// Bool: ] -> StepAnimationFrames(+1) (pauses).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> StepAnimationForwardAction;

	// Bool: - (and numpad -) -> ChangeAnimationPlayRate(-AnimationRateStep).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> AnimationRateDownAction;

	// Bool: = (and numpad +) -> ChangeAnimationPlayRate(+AnimationRateStep).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> AnimationRateUpAction;

	// Bool: 0 -> ResetAnimationPlayRate() (1.0).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Playback")
	TObjectPtr<UInputAction> AnimationRateResetAction;

	// Bool: L -> CycleForcedLOD().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Display")
	TObjectPtr<UInputAction> CycleLODAction;

	// Bool: B -> CycleBackdropPreset().
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Display")
	TObjectPtr<UInputAction> CycleBackdropAction;

	// Play rate change per -/= press (and per panel button).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float AnimationRateStep = 0.25f;

	// --- Portfolio capture settings ---

	// F12 output = viewport size x this (2 at 1920x1080 -> 3840x2160), via
	// the engine's high-resolution screenshot path. 1 = plain viewport-size shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "1", ClampMax = "4"))
	int32 ScreenshotResolutionMultiplier = 2;

	// Shift+F12 frame size multiplier (1 = viewport size; higher values make a
	// 36-frame sequence very slow on a low-end GPU).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "1", ClampMax = "4"))
	int32 TurntableResolutionMultiplier = 1;

	// Yaw step per turntable frame; 360 / step frames are captured (10 -> 36).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "1.0", ClampMax = "90.0"))
	float TurntableStepDegrees = 10.f;

	// Engine ticks to wait after rotating the actor before each frame is
	// requested, so the renderer (temporal AA/motion blur) settles at the new pose.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "0", ClampMax = "30"))
	int32 CaptureSettleFrames = 4;

	// If a requested shot has not been written after this many seconds (real
	// time), the frame is taken with FSlateApplication::TakeScreenshot instead
	// (panel hidden for that frame) and the method is recorded (GetLastCaptureMethod()).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "0.5"))
	float CaptureTimeoutSeconds = 6.f;

	// How long "Saved: <path>" stays in the panel status line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture", meta = (ClampMin = "0.0"))
	float CaptureStatusSeconds = 3.f;

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

	// Section 4 / P1-6 / P2-3: ON collapses the widget (exact previous
	// visibility is restored on OFF), hides the cursor and hides the selection
	// highlight via Actor->SetHighlightVisible(false) (selection id kept);
	// OFF restores all three. Orbit/Zoom/Space/H stay active; inspection
	// clicks are ignored while it is on (see InspectAtScreenPosition()).
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void ToggleCleanView();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void ResetCamera();

	// --- Inspection / Wireframe (P2-1/P2-4) ---

	// false also clears the current part selection (section 4/7). Both
	// directions notify the widget (INSPECTION section / button label).
	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void SetInspectionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void ToggleInspection();

	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	bool IsInspectionEnabled() const { return bInspectionEnabled; }

	// World-space line trace from ScreenPos against the viewer actor's Mesh
	// (Visibility channel). Only accepts hits on the viewer actor's Mesh. Maps
	// Hit.BoneName to a part via Profile->FindPartByBone(), walking up parent
	// bones (Mesh->GetParentBone(), up to 10 levels) if the exact bone is not
	// itself mapped (e.g. a finger bone maps to "LeftArm"). A hit with no
	// mapped part, or no hit at all, clears the selection. Returns true if a
	// part was selected. No-op returning false (selection unchanged) while
	// input is disabled, Inspection is off, or Clean View is on. Notifies the
	// widget after any selection change. Used by both the click path
	// (HandleOrbitPressCompleted) and tests, so there is a single inspection
	// code path.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	bool InspectAtScreenPosition(FVector2D ScreenPos);

	// Toggles Actor->SetWireframeEnabled() and notifies the widget (W key and
	// panel button). Returns false (no-op) if there is no viewer actor or
	// Wireframe is unavailable (APortfolioCharacterActor::IsWireframeAvailable():
	// neither the overlay material nor the profile's WireframeMaterial exists).
	UFUNCTION(BlueprintCallable, Category = "Viewer|Wireframe")
	bool ToggleWireframe();

	// Applies NewProfile to the viewer actor, reframes the camera to the new
	// profile's reset framing, and rebinds/notifies the widget.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SwitchProfile(UCharacterProfileData* NewProfile);

	// P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): looks
	// up ProfileAssetName (a UCharacterProfileData's own asset FName, e.g.
	// "DA_Character_Cube") in the current ACharacterViewerGameMode's
	// ProfileLibrary and, if found, calls SwitchProfile() with it. No-op
	// (no crash) if there is no GameMode or no matching entry. This is what
	// the widget's CHARACTER section buttons call, so switching to a new
	// character in ProfileLibrary never requires a C++/Blueprint code change.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SelectCharacterProfile(FName ProfileAssetName);

	// Preferred way to give the controller its viewer actor (called by
	// ACharacterViewerGameMode); falls back to TActorIterator in BeginPlay if never called.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SetViewerActor(APortfolioCharacterActor* InActor);

	// --- Portfolio capture (F12 / Shift+F12, section 1.7) ---

	// Saves Saved/Screenshots/Portfolio/<ProfileAssetName>_<PresetId>_<yyyyMMdd-HHmmss>.png
	// at viewport size x ScreenshotResolutionMultiplier, without the UI (the
	// scene viewport is read before Slate draws the panel) and with the
	// selection highlight exactly as it is on screen. Asynchronous: the file
	// is written a few frames later (IsCapturing() until then). Returns false
	// if a capture is already running or there is no game viewport.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	bool TakePortfolioScreenshot();

	// 360 / TurntableStepDegrees frames (36 at 10 degrees): the viewer actor's
	// yaw is stepped from its current rotation, one frame per capture, saved as
	// Saved/Screenshots/Portfolio/Turntable_<Profile>_<timestamp>/frame_000.png...
	// The turntable is paused during the sequence; rotation and turntable
	// state are restored afterwards (also on cancel/failure). Camera Orbit/Zoom/
	// Reset and turntable toggles are ignored while it runs. Returns false if a
	// capture is already running or there is no viewer actor/viewport.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	bool StartTurntableCapture();

	// Stops a running capture (Esc). Frames already written are kept.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	void CancelCapture();

	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	bool IsCapturing() const;

	// Full path of the last screenshot file / turntable folder (empty before the first capture).
	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	FString GetLastCaptureOutputPath() const { return LastCaptureOutputPath; }

	// Frames written by the last (or running) capture.
	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	int32 GetLastCaptureSavedFrameCount() const { return LastCaptureSavedFrames; }

	// "HighResScreenshot", "RequestScreenshot" or "SlateTakeScreenshot" (fallback) for the last written frame.
	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	FString GetLastCaptureMethod() const { return LastCaptureMethod; }

	// --- Animation playback / LOD / backdrop (P, [, ], -, =, 0, L, B) ---
	// Each forwards to the viewer actor and refreshes the widget; false when
	// there is no viewer actor or the actor rejected it (e.g. an Animation
	// Blueprint drives the mesh -- APortfolioCharacterActor::IsAnimationPlaybackControllable()).
	// All of them (backdrop included) are ignored while a turntable capture
	// runs, so every frame of a sequence shares one look.

	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	bool ToggleAnimationPaused();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	bool StepAnimationFrames(int32 Frames);

	// Current rate + Delta (clamped 0.1..2.0 by the actor).
	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	bool ChangeAnimationPlayRate(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	bool ResetAnimationPlayRate();

	// Auto -> LOD0 -> LOD1 -> ... -> last LOD -> Auto (actor's SetForcedLOD 0, 1, 2, ...).
	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	bool CycleForcedLOD();

	// Studio -> Black -> White -> MidGrey -> Studio.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void CycleBackdropPreset();

	// Recolours the studio backdrop sphere (MI_StudioBackdrop TopColor/
	// BottomColor) and floor (MI_StudioFloor BaseColor/EdgeColor) through
	// dynamic material instances created once (lights/post process untouched;
	// no level asset is modified). Targets are found on first use: static mesh
	// components whose material (or its parent chain) is MI_StudioBackdrop/
	// M_StudioBackdrop or MI_StudioFloor/M_StudioFloor (works in cooked builds
	// where Outliner labels are gone), or, in the editor, actors labelled
	// StudioBackdrop/PlatformCylinder. Studio restores the values captured
	// then. Missing actors: the preset is still recorded, nothing is
	// recoloured, one log line. Kept across Clean View and profile switches.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void SetBackdropPreset(EViewerBackdropPreset Preset);

	UFUNCTION(BlueprintPure, Category = "Viewer|Display")
	EViewerBackdropPreset GetBackdropPreset() const { return BackdropPreset; }

	// Backdrop + floor material slots being recoloured (0 until the first
	// SetBackdropPreset(), or when the level has no studio actors).
	UFUNCTION(BlueprintPure, Category = "Viewer|Display")
	int32 GetBackdropTargetCount() const { return BackdropTargets.Num(); }

	// "Studio", "Black", "White", "Mid Grey".
	static FString GetBackdropPresetDisplayName(EViewerBackdropPreset Preset);

	// Linear colours a preset applies (backdrop top/bottom, floor base/edge).
	// False for Studio (the captured original values are restored instead).
	static bool GetBackdropPresetColors(EViewerBackdropPreset Preset, FLinearColor& OutBackdropTop, FLinearColor& OutBackdropBottom, FLinearColor& OutFloorBase, FLinearColor& OutFloorEdge);

	// --- Batch portfolio capture (console Viewer.CaptureAll, Tools\CaptureAll.bat, section 1.8) ---
	// Implemented in CharacterViewer/CharacterViewerControllerBatch.cpp; plan
	// and state machine in CharacterViewer/ViewerBatchCapture.h.

	// Every camera preset x material variant (x expressions / + poses, see
	// FViewerBatchOptions) of one profile or of the whole ProfileLibrary, one
	// UI-less F12-style shot each, into
	// Saved/Screenshots/Portfolio/Batch_<Profile>_<timestamp>/<Profile>_<Preset>_<Variant>[_<Expression>].png.
	// The previous selection is restored afterwards; Esc (CancelCapture())
	// stops the batch. Returns false if a capture/batch is running, there is
	// no viewer actor/viewport, the profile is unknown or nothing is shootable.
	bool StartBatchCapture(const FViewerBatchOptions& Options);

	bool IsBatchCapturing() const { return BatchRunner.IsActive(); }

	// State of the running or last batch (counts, written files, folders).
	const FViewerBatchCaptureRunner& GetBatchRunner() const { return BatchRunner; }

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
	virtual void PlayerTick(float DeltaTime) override;

private:
	// --- Portfolio capture internals ---

	// While a turntable sequence runs, camera/turntable/profile changes are
	// ignored so every frame shares one camera and only the actor yaw changes.
	bool IsTurntableCaptureRunning() const { return CaptureSequence.IsActive() && CaptureSequence.GetMode() == EViewerCaptureMode::Turntable; }
	UGameViewportClient* GetCaptureViewportClient() const;
	bool BeginCapture(EViewerCaptureMode Mode, const FString& OutputPath, int32 NumFrames);
	void TickCapture();
	void PrepareCaptureFrame(int32 FrameIndex);
	void RequestCaptureFrame(int32 FrameIndex);
	FString GetCaptureFramePath(int32 FrameIndex) const;
	bool CaptureFrameWithSlate(const FString& FilePath);
	void ClearPendingEngineScreenshot();
	void FinishCapture();
	void HandleScreenshotRequestProcessed();
	void SetCaptureStatus(const FString& Status, bool bTimed);
	FString GetCaptureProfileName() const;
	FString GetCapturePresetId() const;

	void HandleScreenshot(const FInputActionValue& Value);
	void HandleTurntableCapture(const FInputActionValue& Value);
	void HandleCancelCapture(const FInputActionValue& Value);

	FViewerCaptureSequence CaptureSequence;
	FString CaptureOutputPath;	// file (Single) or folder (Turntable)
	FString PendingCaptureFile;
	int32 PendingCaptureMultiplier = 1;
	double PendingCaptureStartSeconds = 0.0;
	bool bPendingCaptureProcessed = false;
	bool bPendingUsesHighRes = false;
	FRotator CaptureStartRotation = FRotator::ZeroRotator;
	bool bCaptureRestoreTurntable = false;
	double CaptureStatusExpireSeconds = 0.0;
	FDelegateHandle ScreenshotProcessedHandle;

	FString LastCaptureOutputPath;
	FString LastCaptureMethod;
	int32 LastCaptureSavedFrames = 0;

	// --- Animation playback / LOD / backdrop internals ---

	void HandleToggleAnimationPause(const FInputActionValue& Value);
	void HandleStepAnimationBack(const FInputActionValue& Value);
	void HandleStepAnimationForward(const FInputActionValue& Value);
	void HandleAnimationRateDown(const FInputActionValue& Value);
	void HandleAnimationRateUp(const FInputActionValue& Value);
	void HandleAnimationRateReset(const FInputActionValue& Value);
	void HandleCycleLOD(const FInputActionValue& Value);
	void HandleCycleBackdrop(const FInputActionValue& Value);
	void NotifyViewerWidget();

	// Finds the targets once (bBackdropTargetsResolved), creating the dynamic MIs.
	void ResolveBackdropTargets();

	EViewerBackdropPreset BackdropPreset = EViewerBackdropPreset::Studio;
	TArray<FViewerBackdropTarget> BackdropTargets;
	bool bBackdropTargetsResolved = false;

	// Batch capture (CharacterViewerControllerBatch.cpp). The host adapter
	// drives the existing single-shot path (BeginCapture/SetCaptureStatus).
	friend class FViewerBatchControllerHost;
	FViewerBatchCaptureRunner BatchRunner;
	// bRestoreSelection=false: EndPlay (the world is going away).
	void StopBatchCapture(bool bRestoreSelection);

	void EnsureFallbackInputAssets();
	void EnsureWidgetCreated();
	void FindViewerActorIfNeeded();
	void ApplyFramingForCurrentActor(bool bInstant);
	void ReleaseDrag();

	// Section 4 ("드래그 종료·포커스 상실 시 캡처/버튼 상태를 해제한다") /
	// 13.10 input boundary: when the application (window) loses OS focus
	// mid-drag, the platform never delivers the matching mouse-up, so
	// bIsPressed/bIsDragging would otherwise stick. Bound to
	// FSlateApplication::OnApplicationActivationStateChanged() in BeginPlay,
	// unbound in EndPlay.
	void HandleApplicationActivationStateChanged(bool bIsActive);

	void HandleOrbitPressStarted(const FInputActionValue& Value);
	void HandleOrbitPressCompleted(const FInputActionValue& Value);
	void HandleOrbitAxis(const FInputActionValue& Value);
	void HandleZoom(const FInputActionValue& Value);
	void HandleResetCamera(const FInputActionValue& Value);
	void HandleToggleTurntable(const FInputActionValue& Value);
	void HandleToggleCleanView(const FInputActionValue& Value);
	void HandleToggleInspection(const FInputActionValue& Value);
	void HandleToggleWireframe(const FInputActionValue& Value);

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

	bool bInspectionEnabled = false;

	FDelegateHandle ApplicationActivationStateChangedHandle;
};
