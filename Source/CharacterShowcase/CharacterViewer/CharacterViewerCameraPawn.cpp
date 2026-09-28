#include "CharacterViewer/CharacterViewerCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

ACharacterViewerCameraPawn::ACharacterViewerCameraPawn()
{
	// Tick is only needed while a preset/reset interpolation is running.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	OrbitRoot = CreateDefaultSubobject<USceneComponent>(TEXT("OrbitRoot"));
	SetRootComponent(OrbitRoot);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(OrbitRoot);
	Camera->bUsePawnControlRotation = false;

	// Spawned and possessed explicitly by ACharacterViewerGameMode's DefaultPawnClass; not auto-possessed.
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	ResetFraming.Distance = CurrentDistance;
	ResetFraming.MinDistance = MinDistance;
	ResetFraming.MaxDistance = MaxDistance;
	ResetFraming.MinPitch = MinPitch;
	ResetFraming.MaxPitch = MaxPitch;
}

void ACharacterViewerCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	UpdateCameraTransform();
}

void ACharacterViewerCameraPawn::SetOrbitCenter(FVector WorldLocation, bool bInstant)
{
	if (bInstant || !bIsInterpolating)
	{
		if (OrbitRoot)
		{
			OrbitRoot->SetWorldLocation(WorldLocation);
		}
		StartOrbitCenterLocation = WorldLocation;
		TargetOrbitCenterLocation = WorldLocation;
		UpdateCameraTransform();
	}
	else
	{
		// Defer the move to Tick(): it interpolates OrbitRoot from
		// StartOrbitCenterLocation (captured in StartInterpolationTo()) to
		// this new target instead of snapping immediately.
		TargetOrbitCenterLocation = WorldLocation;
	}
}

void ACharacterViewerCameraPawn::SetFraming(const FViewerCameraFraming& NewFraming, bool bInstant, bool bUpdateResetFraming)
{
	if (bUpdateResetFraming)
	{
		ResetFraming = NewFraming;
	}

	MinDistance = NewFraming.MinDistance;
	MaxDistance = NewFraming.MaxDistance;
	MinPitch = NewFraming.MinPitch;
	MaxPitch = NewFraming.MaxPitch;
	TargetOffset = NewFraming.TargetOffset;

	if (bInstant)
	{
		CancelInterpolation();
		CurrentYaw = 0.f;
		CurrentPitch = FMath::Clamp(0.f, MinPitch, MaxPitch);
		CurrentDistance = FMath::Clamp(NewFraming.Distance, MinDistance, MaxDistance);
		if (Camera)
		{
			Camera->SetFieldOfView(NewFraming.FOV);
		}
		UpdateCameraTransform();
	}
	else
	{
		StartInterpolationTo(NewFraming);
	}
}

void ACharacterViewerCameraPawn::StartInterpolationTo(const FViewerCameraFraming& TargetFraming)
{
	// Normalize accumulated yaw before interpolating so Reset/preset
	// transitions take the short way around instead of unwinding several
	// full turns after a long manual orbit.
	CurrentYaw = FMath::UnwindDegrees(CurrentYaw);

	InterpolationTargetFraming = TargetFraming;
	StartYaw = CurrentYaw;
	StartPitch = CurrentPitch;
	StartDistance = CurrentDistance;
	StartFOV = Camera ? Camera->FieldOfView : TargetFraming.FOV;
	InterpolationElapsed = 0.f;

	// Orbit center stays put unless a SetOrbitCenter(..., false) call right
	// after this one supplies a new TargetOrbitCenterLocation.
	StartOrbitCenterLocation = OrbitRoot ? OrbitRoot->GetComponentLocation() : FVector::ZeroVector;
	TargetOrbitCenterLocation = StartOrbitCenterLocation;

	bIsInterpolating = true;
	SetActorTickEnabled(true);
}

void ACharacterViewerCameraPawn::ResetToFraming()
{
	StartInterpolationTo(ResetFraming);
}

void ACharacterViewerCameraPawn::Orbit(FVector2D Delta)
{
	CancelInterpolation();
	CurrentYaw += Delta.X * OrbitYawSensitivity;
	CurrentPitch = FMath::Clamp(CurrentPitch + Delta.Y * OrbitPitchSensitivity, MinPitch, MaxPitch);
	UpdateCameraTransform();
}

void ACharacterViewerCameraPawn::Zoom(float Axis)
{
	CancelInterpolation();
	CurrentDistance = FMath::Clamp(CurrentDistance - Axis * ZoomStep, MinDistance, MaxDistance);
	UpdateCameraTransform();
}

void ACharacterViewerCameraPawn::CancelInterpolation()
{
	if (bIsInterpolating)
	{
		bIsInterpolating = false;
		SetActorTickEnabled(false);
	}
}

void ACharacterViewerCameraPawn::UpdateCameraTransform()
{
	if (!Camera || !OrbitRoot)
	{
		return;
	}

	const FRotator OrbitRotation(CurrentPitch, CurrentYaw, 0.f);
	const FVector Forward = OrbitRotation.Vector();
	const FVector LocalCameraLocation = -Forward * CurrentDistance;

	Camera->SetRelativeLocation(LocalCameraLocation);
	Camera->SetRelativeRotation(OrbitRotation);
}

void ACharacterViewerCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsInterpolating)
	{
		return;
	}

	InterpolationElapsed += DeltaSeconds;
	const float Alpha = InterpolationDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(InterpolationElapsed / InterpolationDuration, 0.f, 1.f)
		: 1.f;
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

	const float TargetPitch = FMath::Clamp(0.f, MinPitch, MaxPitch);
	const float TargetDistance = FMath::Clamp(InterpolationTargetFraming.Distance, MinDistance, MaxDistance);

	CurrentYaw = FMath::Lerp(StartYaw, 0.f, Eased);
	CurrentPitch = FMath::Lerp(StartPitch, TargetPitch, Eased);
	CurrentDistance = FMath::Lerp(StartDistance, TargetDistance, Eased);

	if (Camera)
	{
		Camera->SetFieldOfView(FMath::Lerp(StartFOV, InterpolationTargetFraming.FOV, Eased));
	}

	if (OrbitRoot)
	{
		OrbitRoot->SetWorldLocation(FMath::Lerp(StartOrbitCenterLocation, TargetOrbitCenterLocation, Eased));
	}

	UpdateCameraTransform();

	if (Alpha >= 1.f)
	{
		bIsInterpolating = false;
		SetActorTickEnabled(false);
	}
}
