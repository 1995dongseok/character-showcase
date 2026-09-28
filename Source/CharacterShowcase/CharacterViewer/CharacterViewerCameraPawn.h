#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Character/CharacterProfileData.h"
#include "CharacterViewerCameraPawn.generated.h"

class USceneComponent;
class UCameraComponent;

// P0-2 / P1-1: character-centered orbit camera. Owns only camera/orbit state
// (yaw/pitch/distance/FOV and preset interpolation); it has no knowledge of
// input devices (that is ACharacterViewerController's job) or of the
// character's profile data beyond the FViewerCameraFraming it is given.
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ACharacterViewerCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ACharacterViewerCameraPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USceneComponent> OrbitRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Sensitivity")
	float OrbitYawSensitivity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Sensitivity")
	float OrbitPitchSensitivity = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Sensitivity")
	float ZoomStep = 50.f;

	// Duration (seconds) of the preset/reset interpolation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interpolation")
	float InterpolationDuration = 0.35f;

	// Applies distance/FOV/zoom-and-pitch limits from NewFraming. bInstant
	// snaps immediately (used on level start / profile switch); otherwise a
	// short interpolation runs (used for preset selection and Reset), which
	// always interpolates yaw/pitch back to 0 as well as distance/FOV, i.e. a
	// full return to that framing's canonical front view.
	// bUpdateResetFraming controls whether NewFraming also becomes the R /
	// Reset target (see ResetToFraming()): true for profile application
	// (the Reset target should be the profile's full-body framing), false for
	// preset selection (a preset must not silently change what R returns to).
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetFraming(const FViewerCameraFraming& NewFraming, bool bInstant, bool bUpdateResetFraming = true);

	// Manual orbit input; cancels any in-progress interpolation.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Orbit(FVector2D Delta);

	// Manual zoom input; cancels any in-progress interpolation.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void Zoom(float Axis);

	// Interpolates back to the framing last passed to SetFraming (the R / Reset target).
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ResetToFraming();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void StartInterpolationTo(const FViewerCameraFraming& TargetFraming);

	// World-space orbit center (character actor location + framing TargetOffset).
	// bInstant snaps OrbitRoot immediately; otherwise (used together with a
	// non-instant SetFraming(), e.g. preset selection) the move to
	// WorldLocation is interpolated alongside the in-progress Tick
	// interpolation instead of jumping immediately.
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetOrbitCenter(FVector WorldLocation, bool bInstant = true);

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetYaw() const { return CurrentYaw; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetPitch() const { return CurrentPitch; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetDistance() const { return CurrentDistance; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	FVector GetTargetOffset() const { return TargetOffset; }

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsInterpolating() const { return bIsInterpolating; }

	// The framing last passed to SetFraming() with bUpdateResetFraming=true; the R / Reset target.
	UFUNCTION(BlueprintPure, Category = "Camera")
	FViewerCameraFraming GetResetFraming() const { return ResetFraming; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateCameraTransform();
	void CancelInterpolation();

	float CurrentYaw = 0.f;
	float CurrentPitch = 0.f;
	float CurrentDistance = 300.f;
	FVector TargetOffset = FVector::ZeroVector;

	float MinDistance = 50.f;
	float MaxDistance = 1000.f;
	float MinPitch = -80.f;
	float MaxPitch = 80.f;

	// The last framing passed to SetFraming(); the R / Reset target.
	FViewerCameraFraming ResetFraming;

	bool bIsInterpolating = false;
	float InterpolationElapsed = 0.f;
	float StartYaw = 0.f;
	float StartPitch = 0.f;
	float StartDistance = 0.f;
	float StartFOV = 60.f;
	FViewerCameraFraming InterpolationTargetFraming;

	// Orbit center (OrbitRoot world location) interpolation, driven alongside
	// yaw/pitch/distance/FOV in Tick() so a preset/Reset framing change with a
	// different TargetOffset does not snap the view.
	FVector StartOrbitCenterLocation = FVector::ZeroVector;
	FVector TargetOrbitCenterLocation = FVector::ZeroVector;
};
