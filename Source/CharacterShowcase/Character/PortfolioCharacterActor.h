#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PortfolioCharacterActor.generated.h"

class UCharacterProfileData;
class USkeletalMeshComponent;
class UMaterialInterface;

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

	// P2-3: translucent tint applied via Mesh->SetOverlayMaterial() while a
	// part is selected (see SetSelectedPart()). Defaults to
	// /Game/Portfolio/Materials/M_ViewerHighlight (created by
	// Scripts/CreatePortfolioAssets.py) if present at construction time; null
	// is safe (no visible tint, Custom Depth still set). Not part of
	// UCharacterProfileData: it is a Viewer implementation detail, not
	// authored character data.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection")
	TObjectPtr<UMaterialInterface> HighlightOverlayMaterial = nullptr;

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

	// --- Inspection / part selection (P2-1/P2-3) ---

	// Highlights PartId: component-level Custom Depth (stencil 1, see
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.11 for the single-component
	// limitation) plus an OverlayMaterial tint (M_ViewerHighlight) so the
	// selection is actually visible without a project-supplied post-process
	// material. Both the tint and Custom Depth cover the whole mesh, not just
	// PartId's region (single SkeletalMeshComponent). NAME_None clears the
	// selection. A PartId not found in Profile->Parts (or a null Profile) is
	// rejected: no-op, the current selection is kept. While highlight
	// visibility is off (SetHighlightVisible(false), Clean View) the id is
	// still recorded but no tint/Custom Depth is shown.
	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void SetSelectedPart(FName PartId);

	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void ClearSelectedPart();

	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	FName GetSelectedPartId() const { return SelectedPartId; }

	// Clean View (section 4) hides the selection highlight without forgetting
	// the selection: false removes the tint/Custom Depth and keeps them off
	// for any later SetSelectedPart(); true re-applies them for the current
	// selection. Not reset by ApplyProfile() (it mirrors a Controller-level
	// UI mode that persists across a profile switch).
	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void SetHighlightVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	bool IsHighlightVisible() const { return bHighlightVisible; }

	// --- Wireframe (P2-4) ---

	// true: applies Profile->WireframeMaterial to every material slot (keeping
	// the current OverlayMaterial highlight, if any). false: restores the
	// current material variant (CurrentVariantId) to every slot -- never the
	// stale override array -- so Variant -> Wireframe -> Variant round-trips
	// exactly. Returns false (no-op) if Profile or Profile->WireframeMaterial is null.
	UFUNCTION(BlueprintCallable, Category = "Character|Wireframe")
	bool SetWireframeEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Character|Wireframe")
	bool IsWireframeEnabled() const { return bWireframeEnabled; }

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

	// Re-applies CurrentVariantId's slot overrides (or plain defaults for
	// NAME_None) to every mesh slot. Shared by SetMaterialVariant() and by
	// SetWireframeEnabled(false)'s restore path, per
	// Docs/CHARACTER_VIEWER_SETUP.md section 7's "do not copy the override
	// array blindly" pitfall: re-selecting the variant by id is what actually
	// restores it correctly instead of snapshotting/restoring the override array.
	void ApplyMaterialsForCurrentVariant();

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

	FName SelectedPartId = NAME_None;
	bool bWireframeEnabled = false;
	bool bHighlightVisible = true;

	// Applies (or removes) the overlay tint + Custom Depth for SelectedPartId
	// according to bHighlightVisible.
	void ApplyHighlightState();
};
