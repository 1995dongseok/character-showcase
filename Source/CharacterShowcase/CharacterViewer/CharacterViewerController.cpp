#include "CharacterViewer/CharacterViewerController.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "Components/SlateWrapperTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
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

	if (MappingContext)
	{
		MappingContext->MapKey(OrbitPressAction, EKeys::LeftMouseButton);
		MappingContext->MapKey(OrbitAction, EKeys::Mouse2D);
		MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
		MappingContext->MapKey(ResetCameraAction, EKeys::R);
		MappingContext->MapKey(ToggleTurntableAction, EKeys::SpaceBar);
		MappingContext->MapKey(ToggleCleanViewAction, EKeys::H);
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

void ACharacterViewerController::HandleOrbitPressStarted(const FInputActionValue& Value)
{
	if (!bInputEnabled)
	{
		return;
	}

	// Input that starts over the UMG panel must not drive Orbit/Trace.
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
	// P2 inspection click (release below the drag threshold) is not implemented yet; only drag state is cleared here.
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

		// Manual Orbit start stops the turntable; CameraPawn->Orbit() below cancels any in-progress interpolation.
		if (ViewerActor)
		{
			ViewerActor->SetTurntableEnabled(false);
		}
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
}

void ACharacterViewerController::ToggleTurntable()
{
	if (ViewerActor)
	{
		ViewerActor->SetTurntableEnabled(!ViewerActor->IsTurntableEnabled());
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
	}
	else
	{
		if (ViewerWidget)
		{
			ViewerWidget->SetVisibility(PreCleanViewVisibility);
		}
		bShowMouseCursor = bPreCleanViewShowCursor;
	}

	// Orbit, Zoom, Space (turntable) and H itself remain bound and active while clean view is on;
	// only UI visibility and cursor visibility change here.
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
