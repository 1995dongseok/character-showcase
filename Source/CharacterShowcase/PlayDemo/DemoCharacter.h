#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DemoCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCharacterProfileData;

// D1 (Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md sections 3-6): the playable
// character. Uses ACharacter + UCharacterMovementComponent as-is (camera
// relative walk/run, orient-to-movement) and the profile's SkeletalMesh /
// DefaultAnimClass; it is a separate class from APortfolioCharacterActor
// (the exhibition actor is never used as a movement pawn).
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API ADemoCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADemoCharacter();

	// Ground speeds in cm/s currently in use. These class defaults are only the
	// fallback when no Profile is applied; the Profile's Play values
	// (UCharacterProfileData::WalkSpeed/RunSpeed) win, so edit the profile, not these.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Play|Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Play|Movement")
	float RunSpeed = 600.f;

	// Falling below (start transform Z + KillZOffset) returns the character to
	// its start transform. The level edge is walled; this is only a safety net.
	// Relative to the start so moving the PlayerStart/level keeps it valid.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Movement")
	float KillZOffset = -500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float MinPitch = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float MaxPitch = 30.f;

	// Pitch restored by ResetCamera() (negative = camera above, looking down).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float DefaultPitch = -15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float DefaultArmLength = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float MinArmLength = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float MaxArmLength = 600.f;

	// Arm length change (cm) per unit of mouse-wheel axis value.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play|Camera")
	float ZoomStep = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Play")
	TObjectPtr<UCharacterProfileData> Profile;

	// Applies Profile's mesh, animation class and speeds. Null profile, null
	// mesh or null anim class never crash: the character keeps working as an
	// invisible capsule / without an anim instance and a warning is logged.
	UFUNCTION(BlueprintCallable, Category = "Play")
	void ApplyProfile(UCharacterProfileData* InProfile);

	// --- Movement ---

	// Input.X = right, Input.Y = forward, relative to the controller (camera)
	// yaw. The vector is clamped to length 1 so diagonals are not faster.
	void AddMoveInput2D(FVector2D Input);

	UFUNCTION(BlueprintCallable, Category = "Play")
	void SetRunning(bool bInRunning);

	UFUNCTION(BlueprintPure, Category = "Play")
	bool IsRunning() const { return bRunning; }

	// Zero velocity and discard queued movement input (does not touch the run flag).
	UFUNCTION(BlueprintCallable, Category = "Play")
	void StopMoving();

	// Teleports to the transform captured in BeginPlay, stops, and resets the camera.
	UFUNCTION(BlueprintCallable, Category = "Play")
	void ResetToStart();

	UFUNCTION(BlueprintPure, Category = "Play")
	FTransform GetStartTransform() const { return StartTransform; }

	// --- Camera ---

	void AddCameraYaw(float Degrees);
	void AddCameraPitch(float Degrees);

	// Wheel axis: positive zooms in (shorter arm). Clamped to Min/MaxArmLength.
	UFUNCTION(BlueprintCallable, Category = "Play|Camera")
	void ZoomCamera(float Axis);

	// Arm length -> DefaultArmLength; control rotation -> behind the character (yaw of the actor) at DefaultPitch.
	UFUNCTION(BlueprintCallable, Category = "Play|Camera")
	void ResetCamera();

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float ClampCameraPitch(float Pitch) const;

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float GetCameraPitch() const;

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float GetCameraYaw() const;

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	float GetArmLength() const;

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "Play|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Play|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Play|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	void ApplySpeed();

	bool bRunning = false;
	FTransform StartTransform;
};
