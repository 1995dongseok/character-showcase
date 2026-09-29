#include "CharacterViewer/CharacterViewerController.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerGameMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UI/CharacterViewerWidget.h"

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
	if (!bInputEnabled)
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
	if (!bInputEnabled || !CameraPawn)
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
	if (!bInputEnabled || !ViewerActor || !CameraPawn || !ViewerActor->Profile)
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
	if (!CameraPawn)
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

	ViewerActor->ApplyProfile(NewProfile);
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
