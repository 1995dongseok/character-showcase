#include "CharacterViewer/CharacterViewerController.h"

#include "Character/CharacterProfileData.h"
#include "Character/CharacterProfileValidator.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UI/CharacterViewerWidget.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

ACharacterViewerController::ACharacterViewerController()
{
	bShowMouseCursor = true;
}

void ACharacterViewerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	bShowMouseCursor = true;

	FindViewerActorIfNeeded();
	EnsureWidgetCreated();

	if (ViewerWidget)
	{
		ViewerWidget->BindToViewer(this, ViewerActor, CameraPawn);
	}

	ApplyFramingForCurrentActor(true);

	if (FSlateApplication::IsInitialized())
	{
		ApplicationActivationStateChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ACharacterViewerController::HandleApplicationActivationStateChanged);
	}

	// Portfolio capture: completion signal for FScreenshotRequest/high-res
	// shots (fired by UGameViewportClient::ProcessScreenShots after it wrote,
	// or failed to write, the file).
	ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &ACharacterViewerController::HandleScreenshotRequestProcessed);
}

void ACharacterViewerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CameraPawn = Cast<ACharacterViewerCameraPawn>(InPawn);
	ApplyFramingForCurrentActor(true);

	if (ViewerWidget)
	{
		ViewerWidget->BindToViewer(this, ViewerActor, CameraPawn);
	}
}

void ACharacterViewerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	ReleaseDrag();

	// Restores the actor rotation/turntable state of an interrupted turntable capture.
	CancelCapture();
	if (ScreenshotProcessedHandle.IsValid())
	{
		FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
		ScreenshotProcessedHandle.Reset();
	}

	if (FSlateApplication::IsInitialized() && ApplicationActivationStateChangedHandle.IsValid())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationStateChangedHandle);
		ApplicationActivationStateChangedHandle.Reset();
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MappingContext)
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACharacterViewerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (bCreateFallbackInputAssets)
	{
		EnsureFallbackInputAssets();
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComp = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (OrbitPressAction)
		{
			EnhancedInputComp->BindAction(OrbitPressAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleOrbitPressStarted);
			EnhancedInputComp->BindAction(OrbitPressAction, ETriggerEvent::Completed, this, &ACharacterViewerController::HandleOrbitPressCompleted);
			EnhancedInputComp->BindAction(OrbitPressAction, ETriggerEvent::Canceled, this, &ACharacterViewerController::HandleOrbitPressCompleted);
		}
		if (OrbitAction)
		{
			EnhancedInputComp->BindAction(OrbitAction, ETriggerEvent::Triggered, this, &ACharacterViewerController::HandleOrbitAxis);
		}
		if (ZoomAction)
		{
			EnhancedInputComp->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ACharacterViewerController::HandleZoom);
		}
		if (ResetCameraAction)
		{
			EnhancedInputComp->BindAction(ResetCameraAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleResetCamera);
		}
		if (ToggleTurntableAction)
		{
			EnhancedInputComp->BindAction(ToggleTurntableAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleToggleTurntable);
		}
		if (ToggleCleanViewAction)
		{
			EnhancedInputComp->BindAction(ToggleCleanViewAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleToggleCleanView);
		}
		if (ToggleInspectionAction)
		{
			EnhancedInputComp->BindAction(ToggleInspectionAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleToggleInspection);
		}
		if (ToggleWireframeAction)
		{
			EnhancedInputComp->BindAction(ToggleWireframeAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleToggleWireframe);
		}
		if (ScreenshotAction)
		{
			EnhancedInputComp->BindAction(ScreenshotAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleScreenshot);
		}
		if (TurntableCaptureAction)
		{
			EnhancedInputComp->BindAction(TurntableCaptureAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleTurntableCapture);
		}
		if (CancelCaptureAction)
		{
			EnhancedInputComp->BindAction(CancelCaptureAction, ETriggerEvent::Started, this, &ACharacterViewerController::HandleCancelCapture);
		}
	}
}

void ACharacterViewerController::EnsureFallbackInputAssets()
{
	// See Docs/CHARACTER_VIEWER_SETUP.md section 13 for the equivalent Editor
	// asset spec (IA_Orbit/IA_OrbitPress/IA_Zoom/IA_ResetCamera/
	// IA_ToggleTurntable/IA_ToggleCleanView/IMC_CharacterViewer). An
	// Editor-authored asset assigned on the instance always wins.
	//
	// Fallback IAs are only created/mapped when this call also creates the
	// fallback IMC below. If MappingContext already points at an
	// Editor-authored asset, it is treated as fully user-managed and is never
	// mutated here — otherwise repeated PIE runs would keep calling MapKey()
	// on that shared asset object and accumulate duplicate mappings on it.
	const bool bCreatedMappingContext = (MappingContext == nullptr);
	if (bCreatedMappingContext)
	{
		MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CharacterViewer_Fallback"));
	}

	if (!bCreatedMappingContext)
	{
		return;
	}

	if (!OrbitPressAction)
	{
		OrbitPressAction = NewObject<UInputAction>(this, TEXT("IA_OrbitPress_Fallback"));
		OrbitPressAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!OrbitAction)
	{
		OrbitAction = NewObject<UInputAction>(this, TEXT("IA_Orbit_Fallback"));
		OrbitAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (!ZoomAction)
	{
		ZoomAction = NewObject<UInputAction>(this, TEXT("IA_Zoom_Fallback"));
		ZoomAction->ValueType = EInputActionValueType::Axis1D;
	}
	if (!ResetCameraAction)
	{
		ResetCameraAction = NewObject<UInputAction>(this, TEXT("IA_ResetCamera_Fallback"));
		ResetCameraAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ToggleTurntableAction)
	{
		ToggleTurntableAction = NewObject<UInputAction>(this, TEXT("IA_ToggleTurntable_Fallback"));
		ToggleTurntableAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ToggleCleanViewAction)
	{
		ToggleCleanViewAction = NewObject<UInputAction>(this, TEXT("IA_ToggleCleanView_Fallback"));
		ToggleCleanViewAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ToggleInspectionAction)
	{
		ToggleInspectionAction = NewObject<UInputAction>(this, TEXT("IA_ToggleInspection_Fallback"));
		ToggleInspectionAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ToggleWireframeAction)
	{
		ToggleWireframeAction = NewObject<UInputAction>(this, TEXT("IA_ToggleWireframe_Fallback"));
		ToggleWireframeAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!ScreenshotAction)
	{
		ScreenshotAction = NewObject<UInputAction>(this, TEXT("IA_ViewerScreenshot_Fallback"));
		ScreenshotAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!TurntableCaptureAction)
	{
		TurntableCaptureAction = NewObject<UInputAction>(this, TEXT("IA_ViewerTurntableCapture_Fallback"));
		TurntableCaptureAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CaptureShiftAction)
	{
		CaptureShiftAction = NewObject<UInputAction>(this, TEXT("IA_ViewerCaptureShift_Fallback"));
		CaptureShiftAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CancelCaptureAction)
	{
		CancelCaptureAction = NewObject<UInputAction>(this, TEXT("IA_ViewerCancelCapture_Fallback"));
		CancelCaptureAction->ValueType = EInputActionValueType::Boolean;
	}

	if (MappingContext)
	{
		MappingContext->MapKey(OrbitPressAction, EKeys::LeftMouseButton);
		MappingContext->MapKey(OrbitAction, EKeys::Mouse2D);
		MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
		MappingContext->MapKey(ResetCameraAction, EKeys::R);
		MappingContext->MapKey(ToggleTurntableAction, EKeys::SpaceBar);
		MappingContext->MapKey(ToggleCleanViewAction, EKeys::H);
		MappingContext->MapKey(ToggleInspectionAction, EKeys::I);
		MappingContext->MapKey(ToggleWireframeAction, EKeys::W);

		// Portfolio capture. Order matters: Enhanced Input injects a chord
		// blocker only into mappings AFTER the chorded one (and evaluates the
		// chord action best when it is mapped earlier), so Shift, then
		// Shift+F12, then plain F12.
		MappingContext->MapKey(CaptureShiftAction, EKeys::LeftShift);
		MappingContext->MapKey(CaptureShiftAction, EKeys::RightShift);
		{
			FEnhancedActionKeyMapping& TurntableCaptureMapping = MappingContext->MapKey(TurntableCaptureAction, EKeys::F12);
			UInputTriggerChordAction* ShiftChord = NewObject<UInputTriggerChordAction>(MappingContext);
			ShiftChord->ChordAction = CaptureShiftAction;
			TurntableCaptureMapping.Triggers.Add(ShiftChord);
		}
		MappingContext->MapKey(ScreenshotAction, EKeys::F12);
		MappingContext->MapKey(CancelCaptureAction, EKeys::Escape);
	}
}

void ACharacterViewerController::EnsureWidgetCreated()
{
	if (ViewerWidget || !WidgetClass)
	{
		return;
	}

	ViewerWidget = CreateWidget<UCharacterViewerWidget>(this, WidgetClass);
	if (ViewerWidget)
	{
		ViewerWidget->AddToViewport();
	}
}

void ACharacterViewerController::FindViewerActorIfNeeded()
{
	if (IsValid(ViewerActor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APortfolioCharacterActor* Found = nullptr;
	int32 Count = 0;
	for (TActorIterator<APortfolioCharacterActor> It(World); It; ++It)
	{
		Found = *It;
		++Count;
	}

	if (Count == 1)
	{
		ViewerActor = Found;
		bInputEnabled = true;
	}
	else
	{
		ViewerActor = nullptr;
		bInputEnabled = false;
		UE_LOG(LogTemp, Warning, TEXT("ACharacterViewerController: expected exactly one APortfolioCharacterActor in the level, found %d. Input is disabled."), Count);
	}
}

void ACharacterViewerController::SetViewerActor(APortfolioCharacterActor* InActor)
{
	ViewerActor = InActor;
	bInputEnabled = IsValid(ViewerActor);

	EnsureWidgetCreated();
	ApplyFramingForCurrentActor(true);

	if (ViewerWidget)
	{
		ViewerWidget->BindToViewer(this, ViewerActor, CameraPawn);
	}
}

void ACharacterViewerController::ApplyFramingForCurrentActor(bool bInstant)
{
	if (!CameraPawn || !ViewerActor)
	{
		return;
	}

	const UCharacterProfileData* ActorProfile = ViewerActor->Profile;
	const FViewerCameraFraming Framing = ActorProfile ? ActorProfile->GetResetFraming() : FViewerCameraFraming();

	CameraPawn->SetFraming(Framing, bInstant);
	CameraPawn->SetOrbitCenter(ViewerActor->GetActorLocation() + Framing.TargetOffset, bInstant);
}

void ACharacterViewerController::ReleaseDrag()
{
	bIsPressed = false;
	bIsDragging = false;
}

void ACharacterViewerController::HandleApplicationActivationStateChanged(bool bIsActive)
{
	if (bIsActive)
	{
		return;
	}

	// Losing OS focus (alt-tab, another window, etc.) never delivers a mouse-up
	// to this application, so any in-progress drag/press must be cleared here
	// instead of sticking until the next click (section 4/13.10).
	ReleaseDrag();
	FlushPressedKeys();
}

void ACharacterViewerController::HandleOrbitPressStarted(const FInputActionValue& Value)
{
	if (!bInputEnabled || IsTurntableCaptureRunning())
	{
		return;
	}

	// Input that starts over the UMG panel must not drive Orbit/Trace. Note:
	// this hover check alone is not sufficient for a press that bubbles
	// unhandled to the viewport (viewport mouse capture can clear the panel's
	// hover before Enhanced Input fires this), so the C++ fallback panel also
	// consumes presses on its background
	// (UCharacterViewerWidget::HandleFallbackPanelMouseButtonDown).
	if (ViewerWidget && ViewerWidget->IsPointerOverPanel())
	{
		return;
	}

	bIsPressed = true;
	bIsDragging = false;

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (GetMousePosition(MouseX, MouseY))
	{
		PressScreenPosition = FVector2D(MouseX, MouseY);
	}
}

void ACharacterViewerController::HandleOrbitPressCompleted(const FInputActionValue& Value)
{
	// P2-1: a release below the drag threshold, from a press that did NOT
	// start over the UMG panel (bIsPressed is only true in that case -- see
	// HandleOrbitPressStarted()'s IsPointerOverPanel() guard), is an
	// Inspection click. InspectAtScreenPosition() itself ignores the click
	// while Inspection is off or Clean View is on.
	if (bInputEnabled && bInspectionEnabled && bIsPressed && !bIsDragging)
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (GetMousePosition(MouseX, MouseY))
		{
			InspectAtScreenPosition(FVector2D(MouseX, MouseY));
		}
	}

	ReleaseDrag();
}

void ACharacterViewerController::HandleOrbitAxis(const FInputActionValue& Value)
{
	if (!bInputEnabled || !bIsPressed || !CameraPawn)
	{
		return;
	}

	const FVector2D Delta = Value.Get<FVector2D>();

	if (!bIsDragging)
	{
		FVector2D CurrentPos = PressScreenPosition;
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (GetMousePosition(MouseX, MouseY))
		{
			CurrentPos = FVector2D(MouseX, MouseY);
		}

		if (FVector2D::Distance(CurrentPos, PressScreenPosition) < DragThresholdPixels)
		{
			return;
		}

		bIsDragging = true;

		// Manual Orbit start stops the turntable; CameraPawn->Orbit() below
		// cancels any in-progress interpolation. Routed through this
		// Controller's own SetTurntableEnabled() (not ViewerActor's directly)
		// so the Turntable button label follows this path too (section 13.10
		// "state display") instead of silently going stale until some other
		// state change happens to refresh the widget.
		SetTurntableEnabled(false);
	}

	CameraPawn->Orbit(Delta);
}

void ACharacterViewerController::HandleZoom(const FInputActionValue& Value)
{
	if (!bInputEnabled || !CameraPawn || IsTurntableCaptureRunning())
	{
		return;
	}

	if (ViewerWidget && ViewerWidget->IsPointerOverPanel())
	{
		return;
	}

	const float Axis = Value.Get<float>();
	CameraPawn->Zoom(Axis);
}

void ACharacterViewerController::HandleResetCamera(const FInputActionValue& Value)
{
	ResetCamera();
}

void ACharacterViewerController::HandleToggleTurntable(const FInputActionValue& Value)
{
	ToggleTurntable();
}

void ACharacterViewerController::HandleToggleCleanView(const FInputActionValue& Value)
{
	ToggleCleanView();
}

void ACharacterViewerController::SelectCameraPreset(FName Id)
{
	if (!bInputEnabled || !ViewerActor || !CameraPawn || !ViewerActor->Profile || IsTurntableCaptureRunning())
	{
		return;
	}

	const UCharacterProfileData* ActorProfile = ViewerActor->Profile;
	const FViewerCameraPreset* Preset = ActorProfile->FindPreset(Id);
	const FViewerCameraFraming Framing = Preset ? Preset->Framing : ActorProfile->DefaultFraming;

	// bUpdateResetFraming=false: selecting a preset must not change what R / Reset returns to.
	CameraPawn->SetFraming(Framing, false, false);
	// bInstant=false: let Tick() interpolate the orbit center alongside yaw/pitch/distance/FOV instead of snapping.
	CameraPawn->SetOrbitCenter(ViewerActor->GetActorLocation() + Framing.TargetOffset, false);
}

void ACharacterViewerController::SelectAnimation(FName Id)
{
	if (ViewerActor)
	{
		ViewerActor->SetAnimation(Id);
	}
}

void ACharacterViewerController::SelectExpression(FName Id)
{
	if (ViewerActor)
	{
		ViewerActor->SetExpression(Id);
	}
}

void ACharacterViewerController::SelectMaterialVariant(FName Id)
{
	if (ViewerActor)
	{
		ViewerActor->SetMaterialVariant(Id);
	}
}

void ACharacterViewerController::SetTurntableEnabled(bool bEnabled)
{
	if (IsTurntableCaptureRunning())
	{
		return;
	}

	if (ViewerActor)
	{
		ViewerActor->SetTurntableEnabled(bEnabled);
	}

	// Section 13.10 "state display": the Turntable button label must follow
	// every path that can change this state, not only the UI's own
	// RequestToggleTurntable() (which already refreshes itself) -- in
	// particular a direct/keyboard toggle (Space -> HandleToggleTurntable() ->
	// ToggleTurntable(), below) must also keep the label current.
	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
}

void ACharacterViewerController::ToggleTurntable()
{
	if (IsTurntableCaptureRunning())
	{
		return;
	}

	if (ViewerActor)
	{
		ViewerActor->SetTurntableEnabled(!ViewerActor->IsTurntableEnabled());
	}

	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
}

void ACharacterViewerController::ToggleCleanView()
{
	bCleanViewActive = !bCleanViewActive;

	if (bCleanViewActive)
	{
		// Save the exact previous visibility (not just a visible/collapsed
		// bool): forcing ESlateVisibility::Visible back on restore can make
		// the panel fully hit-testable even if it started as e.g.
		// SelfHitTestInvisible, which would then make IsPointerOverPanel()
		// return true permanently and block Orbit/Zoom for good.
		PreCleanViewVisibility = ViewerWidget ? ViewerWidget->GetVisibility() : ESlateVisibility::Visible;
		bPreCleanViewShowCursor = bShowMouseCursor;

		if (ViewerWidget)
		{
			ViewerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		bShowMouseCursor = false;

		// P2-3 (section 4: Clean View hides "모든 Viewer UI/선택 강조/커서"): hide the
		// selection highlight (overlay tint + Custom Depth) without forgetting
		// which part is selected. The Actor keeps it hidden for any later
		// SetSelectedPart() until SetHighlightVisible(true) on leaving Clean View.
		if (ViewerActor)
		{
			ViewerActor->SetHighlightVisible(false);
		}
	}
	else
	{
		if (ViewerWidget)
		{
			ViewerWidget->SetVisibility(PreCleanViewVisibility);
		}
		bShowMouseCursor = bPreCleanViewShowCursor;

		if (ViewerActor)
		{
			ViewerActor->SetHighlightVisible(true);
		}
	}

	// Orbit, Zoom, Space (turntable) and H itself remain bound and active while clean view is on;
	// only UI/highlight visibility and cursor visibility change here. Inspection
	// clicks are ignored while Clean View is on (InspectAtScreenPosition()).
}

void ACharacterViewerController::SetInspectionEnabled(bool bEnabled)
{
	bInspectionEnabled = bEnabled;

	if (!bInspectionEnabled && ViewerActor)
	{
		// Turning Inspection off clears the current selection (section 4/7).
		ViewerActor->ClearSelectedPart();
	}

	// Both directions (I key and panel button): the INSPECTION section and the
	// Inspection button label must follow the new state immediately.
	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
}

void ACharacterViewerController::ToggleInspection()
{
	SetInspectionEnabled(!bInspectionEnabled);
}

bool ACharacterViewerController::InspectAtScreenPosition(FVector2D ScreenPos)
{
	// Clean View (section 4) hides UI/selection/cursor and only keeps
	// Orbit/Zoom/Space/H, so an inspection click is ignored (no selection
	// change) until Clean View is left.
	if (!bInputEnabled || !bInspectionEnabled || bCleanViewActive || !ViewerActor || !ViewerActor->Mesh || !ViewerActor->Profile)
	{
		return false;
	}

	FHitResult Hit;
	const bool bHit = GetHitResultAtScreenPosition(ScreenPos, ECC_Visibility, false, Hit);

	// Only accept hits on the viewer actor's own Mesh; a hit elsewhere (or no hit) clears the selection.
	if (!bHit || Hit.GetActor() != ViewerActor || Hit.Component.Get() != ViewerActor->Mesh)
	{
		ViewerActor->ClearSelectedPart();
		if (ViewerWidget)
		{
			ViewerWidget->NotifySelectionChanged();
		}
		return false;
	}

	const UCharacterProfileData* ActorProfile = ViewerActor->Profile;
	const FViewerPartInfo* Part = nullptr;
	FName BoneName = Hit.BoneName;
	// Walk up parent bones (e.g. a finger bone -> hand_l -> "Left Arm") since a
	// Bone hit does not necessarily match an artist-defined part exactly
	// (Docs/CHARACTER_VIEWER_SETUP.md section 7). Capped so a malformed/cyclic
	// skeleton cannot loop forever.
	for (int32 Level = 0; Level < 10 && BoneName != NAME_None; ++Level)
	{
		Part = ActorProfile->FindPartByBone(BoneName);
		if (Part)
		{
			break;
		}
		BoneName = ViewerActor->Mesh->GetParentBone(BoneName);
	}

	if (!Part)
	{
		ViewerActor->ClearSelectedPart();
		if (ViewerWidget)
		{
			ViewerWidget->NotifySelectionChanged();
		}
		return false;
	}

	ViewerActor->SetSelectedPart(Part->Id);
	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
	return true;
}

bool ACharacterViewerController::ToggleWireframe()
{
	if (!ViewerActor)
	{
		return false;
	}

	const bool bResult = ViewerActor->SetWireframeEnabled(!ViewerActor->IsWireframeEnabled());

	// W key path too (not only the panel button): keep the Wireframe label current.
	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
	return bResult;
}

void ACharacterViewerController::HandleToggleInspection(const FInputActionValue& Value)
{
	ToggleInspection();
}

void ACharacterViewerController::HandleToggleWireframe(const FInputActionValue& Value)
{
	ToggleWireframe();
}

void ACharacterViewerController::ResetCamera()
{
	if (!CameraPawn || IsTurntableCaptureRunning())
	{
		return;
	}

	CameraPawn->ResetToFraming();

	// The orbit center must also return to the Reset framing's TargetOffset,
	// not stay wherever a previously-selected preset left it.
	if (ViewerActor)
	{
		const FViewerCameraFraming ResetFramingValue = CameraPawn->GetResetFraming();
		CameraPawn->SetOrbitCenter(ViewerActor->GetActorLocation() + ResetFramingValue.TargetOffset, false);
	}

	// Section 13.10 "state display": Reset Camera returns the CAMERA to the
	// profile's own DefaultPresetId framing, so the VIEW section's "▶" must
	// follow it too -- otherwise a previously-selected preset (e.g. "Face")
	// stays marked current even though Reset just moved the camera away from it.
	if (ViewerWidget)
	{
		const FName DefaultPresetId = (ViewerActor && ViewerActor->Profile) ? ViewerActor->Profile->DefaultPresetId : NAME_None;
		ViewerWidget->SetCurrentCameraPresetId(DefaultPresetId);
		ViewerWidget->NotifySelectionChanged();
	}
}

void ACharacterViewerController::SwitchProfile(UCharacterProfileData* NewProfile)
{
	if (!ViewerActor)
	{
		return;
	}

	// A running capture belongs to the previous profile (file names, actor
	// rotation): stop it (restoring rotation/turntable) before switching.
	CancelCapture();

	ViewerActor->ApplyProfile(NewProfile);
	UCharacterProfileValidator::LogProfileReport(ViewerActor->Profile, TEXT("SwitchProfile"));
	ApplyFramingForCurrentActor(true);

	if (ViewerWidget)
	{
		ViewerWidget->BindToViewer(this, ViewerActor, CameraPawn);
	}
}

void ACharacterViewerController::SelectCharacterProfile(FName ProfileAssetName)
{
	if (ProfileAssetName == NAME_None)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ACharacterViewerGameMode* GameMode = World ? World->GetAuthGameMode<ACharacterViewerGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	for (UCharacterProfileData* LibraryProfile : GameMode->GetProfileLibrary())
	{
		if (LibraryProfile && LibraryProfile->GetFName() == ProfileAssetName)
		{
			SwitchProfile(LibraryProfile);
			return;
		}
	}
}

// --- Portfolio capture (F12 / Shift+F12, Docs/CHARACTER_VIEWER_SETUP.md section 1.7) ---

void ACharacterViewerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	TickCapture();

	if (CaptureStatusExpireSeconds > 0.0 && FPlatformTime::Seconds() >= CaptureStatusExpireSeconds)
	{
		SetCaptureStatus(FString(), false);
	}
}

void ACharacterViewerController::HandleScreenshot(const FInputActionValue& Value)
{
	// Defensive: with an Editor-authored mapping that lacks the Shift chord
	// blocker, Shift+F12 would otherwise also take a single screenshot.
	if (IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift))
	{
		return;
	}
	TakePortfolioScreenshot();
}

void ACharacterViewerController::HandleTurntableCapture(const FInputActionValue& Value)
{
	StartTurntableCapture();
}

void ACharacterViewerController::HandleCancelCapture(const FInputActionValue& Value)
{
	CancelCapture();
}

UGameViewportClient* ACharacterViewerController::GetCaptureViewportClient() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
}

bool ACharacterViewerController::IsCapturing() const
{
	return CaptureSequence.IsActive();
}

FString ACharacterViewerController::GetCaptureProfileName() const
{
	return (ViewerActor && ViewerActor->Profile) ? ViewerActor->Profile->GetName() : FString(TEXT("NoProfile"));
}

FString ACharacterViewerController::GetCapturePresetId() const
{
	FName PresetId = ViewerWidget ? ViewerWidget->GetCurrentCameraPresetId() : NAME_None;
	if (PresetId == NAME_None && ViewerActor && ViewerActor->Profile)
	{
		PresetId = ViewerActor->Profile->DefaultPresetId;
	}
	return PresetId == NAME_None ? FString(TEXT("Default")) : PresetId.ToString();
}

bool ACharacterViewerController::TakePortfolioScreenshot()
{
	if (IsCapturing())
	{
		return false;
	}

	const FString Directory = ViewerCapture::GetPortfolioDirectory();
	const FString FileName = ViewerCapture::MakeScreenshotFileName(GetCaptureProfileName(), GetCapturePresetId(), FDateTime::Now());
	FString FilePath = Directory / FileName;
	// Two shots within the same second must not overwrite each other.
	for (int32 Suffix = 2; IFileManager::Get().FileExists(*FilePath) && Suffix < 1000; ++Suffix)
	{
		FilePath = Directory / FString::Printf(TEXT("%s_%d.png"), *FPaths::GetBaseFilename(FileName), Suffix);
	}

	return BeginCapture(EViewerCaptureMode::Single, FilePath, 1);
}

bool ACharacterViewerController::StartTurntableCapture()
{
	if (IsCapturing() || !ViewerActor)
	{
		return false;
	}

	const FString Folder = ViewerCapture::GetPortfolioDirectory() / ViewerCapture::MakeTurntableFolderName(GetCaptureProfileName(), FDateTime::Now());

	// Captured before the turntable is paused so the sequence starts exactly
	// at the pose on screen and is restored to it afterwards.
	CaptureStartRotation = ViewerActor->GetActorRotation();
	bCaptureRestoreTurntable = ViewerActor->IsTurntableEnabled();
	ReleaseDrag();

	if (!BeginCapture(EViewerCaptureMode::Turntable, Folder, ViewerCapture::GetTurntableFrameCount(TurntableStepDegrees)))
	{
		bCaptureRestoreTurntable = false;
		return false;
	}

	if (bCaptureRestoreTurntable)
	{
		ViewerActor->SetTurntableEnabled(false);
	}
	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
	return true;
}

bool ACharacterViewerController::BeginCapture(EViewerCaptureMode Mode, const FString& OutputPath, int32 NumFrames)
{
	const UGameViewportClient* ViewportClient = GetCaptureViewportClient();
	if (!ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerCapture] No game viewport; capture not started."));
		return false;
	}

	const FString Directory = (Mode == EViewerCaptureMode::Turntable) ? OutputPath : FPaths::GetPath(OutputPath);
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerCapture] Could not create '%s'; capture not started."), *Directory);
		return false;
	}

	CaptureOutputPath = OutputPath;
	LastCaptureOutputPath = OutputPath;
	LastCaptureSavedFrames = 0;
	LastCaptureMethod.Reset();
	PendingCaptureFile.Reset();
	bPendingCaptureProcessed = false;

	// A single shot needs one settle tick (nothing moves); turntable frames
	// wait CaptureSettleFrames after each rotation.
	CaptureSequence.Begin(Mode, NumFrames, Mode == EViewerCaptureMode::Single ? 1 : CaptureSettleFrames);
	SetCaptureStatus(CaptureSequence.GetProgressText(), false);

	UE_LOG(LogTemp, Log, TEXT("[CharacterViewerCapture] Started %s capture: %d frame(s) -> %s"),
		Mode == EViewerCaptureMode::Turntable ? TEXT("turntable") : TEXT("single"), CaptureSequence.GetNumFrames(), *OutputPath);

	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
	return true;
}

FString ACharacterViewerController::GetCaptureFramePath(int32 FrameIndex) const
{
	return CaptureSequence.GetMode() == EViewerCaptureMode::Turntable
		? CaptureOutputPath / ViewerCapture::MakeTurntableFrameFileName(FrameIndex)
		: CaptureOutputPath;
}

void ACharacterViewerController::PrepareCaptureFrame(int32 FrameIndex)
{
	if (CaptureSequence.GetMode() == EViewerCaptureMode::Turntable && ViewerActor)
	{
		FRotator FrameRotation = CaptureStartRotation;
		FrameRotation.Yaw += ViewerCapture::GetTurntableYawOffset(FrameIndex, TurntableStepDegrees);
		ViewerActor->SetActorRotation(FrameRotation);
	}
	SetCaptureStatus(CaptureSequence.GetProgressText(), false);
}

void ACharacterViewerController::RequestCaptureFrame(int32 FrameIndex)
{
	PendingCaptureFile = GetCaptureFramePath(FrameIndex);
	bPendingCaptureProcessed = false;
	bPendingUsesHighRes = false;
	PendingCaptureStartSeconds = FPlatformTime::Seconds();
	PendingCaptureMultiplier = FMath::Clamp(CaptureSequence.GetMode() == EViewerCaptureMode::Turntable ? TurntableResolutionMultiplier : ScreenshotResolutionMultiplier, 1, 4);

	// A stale file at this path would make the "written" check meaningless.
	IFileManager::Get().Delete(*PendingCaptureFile, false, true, true);

	// Primary path: the engine screenshot request, read from the SCENE
	// viewport before Slate draws the panel (bShowUI=false), so the UI is
	// never in the image while the panel can keep showing "Capturing n/N".
	// For a multiplier > 1 the high-resolution path renders the scene at
	// viewport size x multiplier; FilenameOverride gives it the exact file name.
	const UGameViewportClient* ViewportClient = GetCaptureViewportClient();
	const FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (PendingCaptureMultiplier > 1 && Viewport)
	{
		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		GetHighResScreenshotConfig().SetFilename(PendingCaptureFile);
		bPendingUsesHighRes = ViewportSize.X > 0 && ViewportSize.Y > 0
			&& GetHighResScreenshotConfig().SetResolution(ViewportSize.X, ViewportSize.Y, static_cast<float>(PendingCaptureMultiplier));
		if (!bPendingUsesHighRes)
		{
			GetHighResScreenshotConfig().SetFilename(FString());
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerCapture] High-resolution x%d not available for %dx%d; using viewport size."), PendingCaptureMultiplier, ViewportSize.X, ViewportSize.Y);
		}
	}
	FScreenshotRequest::RequestScreenshot(PendingCaptureFile, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
}

void ACharacterViewerController::HandleScreenshotRequestProcessed()
{
	if (!PendingCaptureFile.IsEmpty())
	{
		bPendingCaptureProcessed = true;
	}
}

void ACharacterViewerController::ClearPendingEngineScreenshot()
{
	FScreenshotRequest::Reset();
	if (bPendingUsesHighRes)
	{
		GIsHighResScreenshot = false;
	}
	GetHighResScreenshotConfig().SetFilename(FString());
	bPendingUsesHighRes = false;
}

bool ACharacterViewerController::CaptureFrameWithSlate(const FString& FilePath)
{
	// Fallback (same API the -game smoke test uses for its window captures):
	// a synchronous Slate draw of the game window into a bitmap, with the
	// panel collapsed for exactly that draw so the UI is not in the image.
	UGameViewportClient* ViewportClient = GetCaptureViewportClient();
	const TSharedPtr<SWindow> Window = ViewportClient ? ViewportClient->GetWindow() : nullptr;
	if (!Window.IsValid() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	ESlateVisibility PreviousVisibility = ESlateVisibility::Visible;
	const bool bHidePanel = ViewerWidget && ViewerWidget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bHidePanel)
	{
		PreviousVisibility = ViewerWidget->GetVisibility();
		ViewerWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	TArray<FColor> Bitmap;
	FIntVector Size(0, 0, 0);
	const bool bTaken = FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Bitmap, Size);

	if (bHidePanel)
	{
		ViewerWidget->SetVisibility(PreviousVisibility);
	}

	if (!bTaken || Size.X <= 0 || Size.Y <= 0 || Bitmap.Num() < Size.X * Size.Y)
	{
		return false;
	}

	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}
	return FImageUtils::SaveImageByExtension(*FilePath, FImageView(Bitmap.GetData(), Size.X, Size.Y));
}

void ACharacterViewerController::TickCapture()
{
	if (!CaptureSequence.IsActive())
	{
		return;
	}

	switch (CaptureSequence.Tick())
	{
	case EViewerCaptureStep::PrepareFrame:
		PrepareCaptureFrame(CaptureSequence.GetCurrentFrame());
		return;
	case EViewerCaptureStep::RequestCapture:
		// The request is processed by this same engine frame's viewport draw.
		RequestCaptureFrame(CaptureSequence.GetCurrentFrame());
		return;
	default:
		break;
	}

	if (CaptureSequence.GetPhase() != EViewerCapturePhase::Capturing || PendingCaptureFile.IsEmpty())
	{
		return;
	}

	const bool bWritten = bPendingCaptureProcessed && IFileManager::Get().FileExists(*PendingCaptureFile);
	const bool bTimedOut = (FPlatformTime::Seconds() - PendingCaptureStartSeconds) > CaptureTimeoutSeconds;
	if (!bWritten && !bPendingCaptureProcessed && !bTimedOut)
	{
		return;
	}

	bool bSuccess = bWritten;
	if (bWritten)
	{
		LastCaptureMethod = bPendingUsesHighRes ? TEXT("HighResScreenshot") : TEXT("RequestScreenshot");
		GetHighResScreenshotConfig().SetFilename(FString());
		bPendingUsesHighRes = false;
	}
	else
	{
		// Processed without a file (e.g. an OnScreenshotCaptured delegate took
		// the bitmap) or never processed in time: cancel the engine request so
		// it cannot fire later with a stale name, and take this frame via Slate.
		UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerCapture] Engine screenshot %s for '%s'; falling back to FSlateApplication::TakeScreenshot."),
			bPendingCaptureProcessed ? TEXT("was processed but wrote no file") : TEXT("timed out"), *PendingCaptureFile);
		ClearPendingEngineScreenshot();
		bSuccess = CaptureFrameWithSlate(PendingCaptureFile) && IFileManager::Get().FileExists(*PendingCaptureFile);
		if (bSuccess)
		{
			LastCaptureMethod = TEXT("SlateTakeScreenshot");
		}
	}

	if (bSuccess)
	{
		++LastCaptureSavedFrames;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerCapture] Could not write '%s'."), *PendingCaptureFile);
	}

	PendingCaptureFile.Reset();
	bPendingCaptureProcessed = false;
	CaptureSequence.NotifyFrameCaptured(bSuccess);

	if (!CaptureSequence.IsActive())
	{
		FinishCapture();
	}
}

void ACharacterViewerController::CancelCapture()
{
	if (!CaptureSequence.IsActive())
	{
		return;
	}

	if (!PendingCaptureFile.IsEmpty())
	{
		ClearPendingEngineScreenshot();
		PendingCaptureFile.Reset();
		bPendingCaptureProcessed = false;
	}
	CaptureSequence.Cancel();
	FinishCapture();
}

void ACharacterViewerController::FinishCapture()
{
	const EViewerCaptureMode Mode = CaptureSequence.GetMode();
	const EViewerCapturePhase Phase = CaptureSequence.GetPhase();

	if (Mode == EViewerCaptureMode::Turntable && ViewerActor)
	{
		ViewerActor->SetActorRotation(CaptureStartRotation);
		if (bCaptureRestoreTurntable)
		{
			ViewerActor->SetTurntableEnabled(true);
		}
	}
	bCaptureRestoreTurntable = false;
	GetHighResScreenshotConfig().SetFilename(FString());

	const FString DisplayPath = ViewerCapture::MakeDisplayPath(CaptureOutputPath);
	FString Status;
	switch (Phase)
	{
	case EViewerCapturePhase::Finished:
		Status = (Mode == EViewerCaptureMode::Turntable)
			? FString::Printf(TEXT("Saved: %s (%d frames)"), *DisplayPath, LastCaptureSavedFrames)
			: FString::Printf(TEXT("Saved: %s"), *DisplayPath);
		break;
	case EViewerCapturePhase::Cancelled:
		Status = FString::Printf(TEXT("Capture cancelled (%d/%d saved)"), LastCaptureSavedFrames, CaptureSequence.GetNumFrames());
		break;
	default:
		Status = TEXT("Capture failed (see log)");
		break;
	}

	UE_LOG(LogTemp, Log, TEXT("[CharacterViewerCapture] %s [method=%s, path=%s]"), *Status, *LastCaptureMethod, *CaptureOutputPath);
	SetCaptureStatus(Status, true);

	if (ViewerWidget)
	{
		ViewerWidget->NotifySelectionChanged();
	}
}

void ACharacterViewerController::SetCaptureStatus(const FString& Status, bool bTimed)
{
	CaptureStatusExpireSeconds = (bTimed && !Status.IsEmpty()) ? FPlatformTime::Seconds() + CaptureStatusSeconds : 0.0;
	if (ViewerWidget)
	{
		ViewerWidget->SetCaptureStatus(FText::FromString(Status));
	}
}
