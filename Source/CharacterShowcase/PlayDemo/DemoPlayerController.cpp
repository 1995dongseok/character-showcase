#include "PlayDemo/DemoPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PlayDemo/DemoCharacter.h"

ADemoPlayerController::ADemoPlayerController()
{
	bShowMouseCursor = false;
}

ADemoCharacter* ADemoPlayerController::GetDemoCharacter() const
{
	return Cast<ADemoCharacter>(GetPawn());
}

void ADemoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetCursorMode(false);

	// Possession resets the control rotation to the pawn's rotation (pitch 0) after
	// ADemoCharacter::PossessedBy(); start with the same composition as the R reset.
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->ResetCamera();
		BeginPlayCameraPitch = DemoCharacter->GetCameraPitch();
		BeginPlayArmLength = DemoCharacter->GetArmLength();
	}

	if (HintWidgetClass)
	{
		HintWidget = CreateWidget<UUserWidget>(this, HintWidgetClass);
		if (HintWidget)
		{
			HintWidget->AddToViewport();
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("ADemoPlayerController: HintWidgetClass is not set; no control hint is shown (D2 item)."));
	}

	if (FSlateApplication::IsInitialized())
	{
		ApplicationActivationStateChangedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(this, &ADemoPlayerController::HandleApplicationActivationStateChanged);
	}
}

void ADemoPlayerController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
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

void ADemoPlayerController::SetupInputComponent()
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

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveAction)
		{
			Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADemoPlayerController::HandleMove);
		}
		if (RunAction)
		{
			Input->BindAction(RunAction, ETriggerEvent::Started, this, &ADemoPlayerController::HandleRunStarted);
			Input->BindAction(RunAction, ETriggerEvent::Completed, this, &ADemoPlayerController::HandleRunCompleted);
			Input->BindAction(RunAction, ETriggerEvent::Canceled, this, &ADemoPlayerController::HandleRunCompleted);
		}
		if (LookAction)
		{
			Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADemoPlayerController::HandleLook);
		}
		if (ZoomAction)
		{
			Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ADemoPlayerController::HandleZoom);
		}
		if (ResetCameraAction)
		{
			Input->BindAction(ResetCameraAction, ETriggerEvent::Started, this, &ADemoPlayerController::HandleResetCamera);
		}
		if (RespawnAction)
		{
			Input->BindAction(RespawnAction, ETriggerEvent::Started, this, &ADemoPlayerController::HandleRespawn);
		}
		if (ToggleCursorAction)
		{
			Input->BindAction(ToggleCursorAction, ETriggerEvent::Started, this, &ADemoPlayerController::HandleToggleCursor);
		}
	}
}

void ADemoPlayerController::EnsureFallbackInputAssets()
{
	// As in ACharacterViewerController: an Editor-authored IMC is treated as
	// fully user-managed and never mutated here; fallback IAs are only
	// created together with the fallback IMC.
	if (MappingContext)
	{
		return;
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Demo_Fallback"));

	const auto MakeAction = [this](TObjectPtr<UInputAction>& Slot, const TCHAR* Name, EInputActionValueType Type)
	{
		if (!Slot)
		{
			Slot = NewObject<UInputAction>(this, Name);
			Slot->ValueType = Type;
		}
	};

	MakeAction(MoveAction, TEXT("IA_DemoMove_Fallback"), EInputActionValueType::Axis2D);
	MakeAction(RunAction, TEXT("IA_DemoRun_Fallback"), EInputActionValueType::Boolean);
	MakeAction(LookAction, TEXT("IA_DemoLook_Fallback"), EInputActionValueType::Axis2D);
	MakeAction(ZoomAction, TEXT("IA_DemoZoom_Fallback"), EInputActionValueType::Axis1D);
	MakeAction(ResetCameraAction, TEXT("IA_DemoResetCamera_Fallback"), EInputActionValueType::Boolean);
	MakeAction(RespawnAction, TEXT("IA_DemoRespawn_Fallback"), EInputActionValueType::Boolean);
	MakeAction(ToggleCursorAction, TEXT("IA_DemoToggleCursor_Fallback"), EInputActionValueType::Boolean);

	// Move: X = right (D +, A -), Y = forward (W +, S -), like the third-person template IMC.
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));

		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));

		MappingContext->MapKey(MoveAction, EKeys::D);

		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	}

	MappingContext->MapKey(RunAction, EKeys::LeftShift);
	// Mouse2D is +Y when the mouse moves up, which is also "look up" (+pitch): no negate needed for this handler.
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MappingContext->MapKey(ResetCameraAction, EKeys::R);
	MappingContext->MapKey(RespawnAction, EKeys::BackSpace);
	MappingContext->MapKey(ToggleCursorAction, EKeys::Escape);
}

void ADemoPlayerController::SetCursorMode(bool bEnabled)
{
	bCursorMode = bEnabled;

	if (bCursorMode)
	{
		ReleaseAllInput();

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void ADemoPlayerController::ToggleCursorMode()
{
	SetCursorMode(!bCursorMode);
}

bool ADemoPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (bIgnoreRepeatUntilPress)
	{
		if (Params.Event == IE_Pressed)
		{
			KeysPressedSinceRelease.Add(Params.Key);
		}
		else if (Params.Event == IE_Repeat && !KeysPressedSinceRelease.Contains(Params.Key))
		{
			// Held through a focus loss / cursor toggle: consume, never forward.
			return true;
		}
	}

	return Super::InputKey(Params);
}

void ADemoPlayerController::ReleaseAllInput()
{
	FlushPressedKeys();
	bIgnoreRepeatUntilPress = true;
	KeysPressedSinceRelease.Reset();

	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->SetRunning(false);
		DemoCharacter->StopMoving();
	}
}

void ADemoPlayerController::HandleApplicationActivationStateChanged(bool bIsActive)
{
	if (bIsActive)
	{
		return;
	}

	// Losing OS focus never delivers key-up events: release everything here so
	// nothing keeps moving; after returning, keys must be pressed anew.
	ReleaseAllInput();
}

void ADemoPlayerController::HandleMove(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->AddMoveInput2D(Value.Get<FVector2D>());
	}
}

void ADemoPlayerController::HandleRunStarted(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->SetRunning(true);
	}
}

void ADemoPlayerController::HandleRunCompleted(const FInputActionValue& Value)
{
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->SetRunning(false);
	}
}

void ADemoPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		const FVector2D Delta = Value.Get<FVector2D>();
		DemoCharacter->AddCameraYaw(Delta.X * LookSensitivity);
		DemoCharacter->AddCameraPitch(Delta.Y * LookSensitivity);
	}
}

void ADemoPlayerController::HandleZoom(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->ZoomCamera(Value.Get<float>());
	}
}

void ADemoPlayerController::HandleResetCamera(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->ResetCamera();
	}
}

void ADemoPlayerController::HandleRespawn(const FInputActionValue& Value)
{
	if (bCursorMode)
	{
		return;
	}
	if (ADemoCharacter* DemoCharacter = GetDemoCharacter())
	{
		DemoCharacter->ResetToStart();
	}
}

void ADemoPlayerController::HandleToggleCursor(const FInputActionValue& Value)
{
	ToggleCursorMode();
}
