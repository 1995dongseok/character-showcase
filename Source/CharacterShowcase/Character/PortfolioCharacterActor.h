#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortfolioCharacterActor.generated.h"

class UCharacterProfileData;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API APortfolioCharacterActor : public AActor
{
	GENERATED_BODY()

public:
	APortfolioCharacterActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<UCharacterProfileData> Profile = nullptr;

	// Clears runtime state (turntable rotation, expression morphs, material
	// overrides, animation) accumulated under the previous profile, then
	// applies NewProfile (or clears the mesh if null/invalid).
	UFUNCTION(BlueprintCallable, Category = "Character")
	void ApplyProfile(UCharacterProfileData* NewProfile);

	// --- Turntable (P1-2) ---

	UFUNCTION(BlueprintCallable, Category = "Character|Turntable")
	void SetTurntableEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Character|Turntable")
	bool IsTurntableEnabled() const { return bTurntableEnabled; }

	// The actual rotation step, also called from Tick. Exposed publicly so
	// automation tests can simulate turntable rotation without relying on the
	// engine's tick loop.
	UFUNCTION(BlueprintCallable, Category = "Character|Turntable")
	void AdvanceTurntable(float DeltaSeconds);

	// --- Animation (P1-3) ---

	// Selects an Animation Sequence/pose by id (from Profile->Animations), or
	// restores the profile default playback state for NAME_None. Returns
	// false (no crash) for a null Profile/Mesh, an unknown id, or a
	// skeleton-incompatible Sequence.
	UFUNCTION(BlueprintCallable, Category = "Character|Animation")
	bool SetAnimation(FName Id);

	UFUNCTION(BlueprintPure, Category = "Character|Animation")
	FName GetCurrentAnimationId() const { return CurrentAnimationId; }

	// --- Expression (P1-4) ---

	// Selects a Morph-Target-based expression by id (from
	// Profile->Expressions), or clears to Neutral for NAME_None.
	UFUNCTION(BlueprintCallable, Category = "Character|Expression")
	bool SetExpression(FName Id);

	UFUNCTION(BlueprintPure, Category = "Character|Expression")
	FName GetCurrentExpressionId() const { return CurrentExpressionId; }

	// --- Material Variant (P1-5) ---

	// Selects a slot-based material variant by id (from
	// Profile->MaterialVariants), or restores default materials for NAME_None.
	UFUNCTION(BlueprintCallable, Category = "Character|Appearance")
	bool SetMaterialVariant(FName Id);

	UFUNCTION(BlueprintPure, Category = "Character|Appearance")
	FName GetCurrentVariantId() const { return CurrentVariantId; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	// Captures InitialRotation as early as possible (before BeginPlay/
	// ApplyProfile can run), since ACharacterViewerGameMode::PostLogin() may
	// call ApplyProfile() before this actor's own BeginPlay has run (PostLogin
	// runs before World::BeginPlay). See PortfolioCharacterActor.cpp.
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	// Resets turntable rotation (not the enabled state, which persists across
	// a profile switch), expression morphs, material overrides and tracked
	// selection ids to a clean slate. Called before a new profile is applied.
	void ClearRuntimeState();

	// Restores the default playback state: Profile->DefaultAnimClass if set,
	// else Profile->DefaultAnimationId's Sequence if set and valid, else an
	// empty AnimationSingleNode state.
	void RestoreDefaultAnimationState();

private:
	// Rotation captured in PostInitializeComponents(), used to reset the
	// turntable rotation on ClearRuntimeState()/ApplyProfile(). Until
	// bInitialRotationCaptured is true, ClearRuntimeState() must not reset
	// rotation (it would otherwise snap a still-unknown placed rotation to
	// identity).
	FRotator InitialRotation = FRotator::ZeroRotator;
	bool bInitialRotationCaptured = false;

	bool bTurntableEnabled = false;

	FName CurrentAnimationId = NAME_None;
	FName CurrentExpressionId = NAME_None;
	FName CurrentVariantId = NAME_None;

	// Morph names applied by the current expression, so switching expression
	// only resets the morphs the previous expression actually touched.
	TArray<FName> AppliedMorphNames;
};
