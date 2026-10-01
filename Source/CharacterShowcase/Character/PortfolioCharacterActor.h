#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Character/ViewerMeshStats.h"
#include "PortfolioCharacterActor.generated.h"

class UCharacterProfileData;
class USkeletalMeshComponent;
class USkeletalMesh;
class USkeleton;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
struct FViewerPartInfo;

// How the currently selected part is shown (GetActiveHighlightMode()).
// Precedence per part: MaterialSlots (the part's MaterialSlotNames resolve on
// the mesh) > BoneMarkers (the part's BoneNames exist in the mesh skeleton) >
// WholeMesh (fallback overlay tint, so a selection is never invisible).
UENUM(BlueprintType)
enum class EViewerHighlightMode : uint8
{
	// No selection, or the highlight is hidden (Clean View).
	None,
	// PartHighlightMaterial replaces exactly the part's material slots.
	MaterialSlots,
	// Small spheres on the part's bones and their direct child bones.
	BoneMarkers,
	// Translucent overlay tint over the whole mesh (HighlightOverlayMaterial).
	// While the Wireframe overlay is on (EViewerWireframeMode::Overlay) the
	// mesh's single overlay slot shows the wireframe instead; this mode then
	// only keeps Custom Depth (logged once), see ApplyOverlayState().
	WholeMesh
};

// How Wireframe is drawn (GetActiveWireframeMode()).
UENUM(BlueprintType)
enum class EViewerWireframeMode : uint8
{
	// Wireframe off, or no wireframe material available.
	None,
	// Default: WireframeOverlayMaterial (M_WireframeOverlay) drawn as the
	// mesh's overlay pass over the normally shaded surface, so topology is
	// readable on dense meshes. Material slots are untouched.
	Overlay,
	// Legacy: Profile->WireframeMaterial replaces every material slot (lines
	// only, no shaded surface). Used when bWireframeReplacesSlots is on, or as
	// a fallback when WireframeOverlayMaterial is null.
	ReplaceSlots
};

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

	// P2-3: translucent tint applied via Mesh->SetOverlayMaterial() as the
	// WholeMesh fallback highlight (a selected part with no resolvable
	// MaterialSlotNames and no BoneNames found in the mesh). Defaults to
	// /Game/Portfolio/Materials/M_ViewerHighlight (created by
	// Scripts/CreatePortfolioAssets.py) if present at construction time; null
	// is safe (no visible tint, Custom Depth still set). Not part of
	// UCharacterProfileData: it is a Viewer implementation detail, not
	// authored character data.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection")
	TObjectPtr<UMaterialInterface> HighlightOverlayMaterial = nullptr;

	// Per-part highlight: opaque unlit emissive magenta material put on the
	// selected part's material slots (EViewerHighlightMode::MaterialSlots) and
	// on the bone marker spheres (BoneMarkers). Defaults to
	// /Game/Portfolio/Materials/M_ViewerPartHighlight (created by
	// Scripts/CreatePortfolioAssets.py) if present at construction time. Null
	// disables both per-part modes; a selection then falls back to WholeMesh.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection")
	TObjectPtr<UMaterialInterface> PartHighlightMaterial = nullptr;

	// Static mesh used for bone markers (default /Engine/BasicShapes/Sphere,
	// 100 cm diameter). Null disables BoneMarkers (falls back to WholeMesh).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection")
	TObjectPtr<UStaticMesh> BoneMarkerMesh = nullptr;

	// World-space diameter (cm) of one bone marker sphere.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection", meta = (ClampMin = "1"))
	float BoneMarkerDiameter = 10.f;

	// Opt-in: also keep the faint whole-mesh overlay tint while BoneMarkers
	// are shown. Off by default: the markers are the indication.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Inspection")
	bool bWholeMeshTintWithBoneMarkers = false;

	// Shaded wireframe: unlit cyan Wireframe=True material set with
	// Mesh->SetOverlayMaterial() while Wireframe is on, so the lines are drawn
	// on top of the normally shaded mesh (EViewerWireframeMode::Overlay).
	// Defaults to /Game/Portfolio/Materials/M_WireframeOverlay (created by
	// Scripts/CreatePortfolioAssets.py) if present at construction time. A
	// Viewer implementation detail like the highlight materials, not authored
	// character data. Null -> Wireframe falls back to the profile's legacy
	// slot-replacing WireframeMaterial (if any).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Wireframe")
	TObjectPtr<UMaterialInterface> WireframeOverlayMaterial = nullptr;

	// Legacy opt-in: when the profile has a WireframeMaterial, replace every
	// material slot with it (lines only, no shaded surface) instead of using
	// the overlay. Off by default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Wireframe")
	bool bWireframeReplacesSlots = false;

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
	// skeleton-incompatible Sequence (see IsAnimationSkeletonCompatible()).
	UFUNCTION(BlueprintCallable, Category = "Character|Animation")
	bool SetAnimation(FName Id);

	UFUNCTION(BlueprintPure, Category = "Character|Animation")
	FName GetCurrentAnimationId() const { return CurrentAnimationId; }

	// Runtime-safe (no editor-only API) check used by SetAnimation(): true if
	// SequenceSkeleton is the mesh's own Skeleton, is listed in either
	// skeleton's Compatible Skeletons, or its bone hierarchy matches the mesh
	// (USkeleton::IsCompatibleMesh). False for null inputs.
	static bool IsAnimationSkeletonCompatible(const USkeleton* SequenceSkeleton, const USkeletalMesh* MeshAsset);

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

	// Highlights PartId so the user can see WHICH part is selected, using the
	// first mode that applies (EViewerHighlightMode): the part's
	// MaterialSlotNames slots get PartHighlightMaterial; else marker spheres
	// are attached to the part's BoneNames and their direct child bones; else
	// the whole mesh gets the HighlightOverlayMaterial tint. Component-level
	// Custom Depth (stencil 1) is also set in every mode. NAME_None clears the
	// selection (markers destroyed). A PartId not found in Profile->Parts (or
	// a null Profile) is rejected: no-op, the current selection is kept. While
	// highlight visibility is off (SetHighlightVisible(false), Clean View) the
	// id is still recorded but nothing is shown.
	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void SetSelectedPart(FName PartId);

	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void ClearSelectedPart();

	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	FName GetSelectedPartId() const { return SelectedPartId; }

	// Clean View (section 4) hides the selection highlight without forgetting
	// the selection: false removes the highlight (slots restored, markers
	// hidden, tint/Custom Depth off) and keeps it off for any later
	// SetSelectedPart(); true re-applies it for the current selection. Not
	// reset by ApplyProfile() (it mirrors a Controller-level UI mode that
	// persists across a profile switch).
	UFUNCTION(BlueprintCallable, Category = "Character|Inspection")
	void SetHighlightVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	bool IsHighlightVisible() const { return bHighlightVisible; }

	// The highlight currently shown: None when nothing is selected or the
	// highlight is hidden (Clean View), otherwise the mode chosen for the
	// selected part (see EViewerHighlightMode for the precedence).
	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	EViewerHighlightMode GetActiveHighlightMode() const { return ActiveHighlightMode; }

	// Bone marker components currently alive (pooled; hidden ones included).
	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	int32 GetBoneMarkerCount() const { return BoneMarkers.Num(); }

	// Bone marker components currently visible.
	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	int32 GetVisibleBoneMarkerCount() const;

	// Bones the visible markers are attached to (the selected part's
	// BoneNames found in the mesh plus their direct children), in marker order.
	UFUNCTION(BlueprintPure, Category = "Character|Inspection")
	TArray<FName> GetBoneMarkerBoneNames() const;

	// --- Measured mesh info (replaces hand-authored guesses) ---

	// Measured stats for the current mesh (LOD0 render data, skeleton,
	// materials). bValid is false without a mesh or render data.
	UFUNCTION(BlueprintPure, Category = "Character|Stats")
	FViewerMeshStats GetMeshStats() const;

	// One entry per material slot of the current mesh (empty without a mesh).
	UFUNCTION(BlueprintPure, Category = "Character|Stats")
	TArray<FViewerSlotStats> GetSlotStats() const;

	// Measured stats summed over PartId's MaterialSlotNames (unique textures
	// across those slots). False (Out reset) if the part is unknown or none of
	// its MaterialSlotNames resolve on the current mesh -- the UI then shows
	// the part's authored notes (TriangleCount/MaterialName/TextureResolution).
	UFUNCTION(BlueprintPure, Category = "Character|Stats")
	bool GetPartMeasuredStats(FName PartId, FViewerSlotStats& Out) const;

	// --- Wireframe (P2-4) ---

	// true: shows the wireframe in the mode GetActiveWireframeMode() reports:
	// Overlay (default) sets WireframeOverlayMaterial as the mesh overlay and
	// leaves every material slot as it is (Variant and part highlight stay
	// visible under the lines); ReplaceSlots (bWireframeReplacesSlots, or no
	// overlay material) applies Profile->WireframeMaterial to every slot
	// except the selected part's highlighted slots. false: removes the overlay
	// / restores the current material variant to every slot -- never the
	// stale override array -- and re-applies the part highlight, so Variant ->
	// Wireframe -> Variant round-trips exactly (ApplyHighlightState()
	// recomputes slots, markers and the overlay). Turning it ON returns false
	// (no-op) if there is no Profile or neither WireframeOverlayMaterial nor
	// Profile->WireframeMaterial is available (IsWireframeAvailable());
	// turning it OFF always succeeds while a Mesh exists.
	UFUNCTION(BlueprintCallable, Category = "Character|Wireframe")
	bool SetWireframeEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Character|Wireframe")
	bool IsWireframeEnabled() const { return bWireframeEnabled; }

	// True if SetWireframeEnabled(true) would succeed for the current profile
	// (the Widget enables/disables the Wireframe button with this).
	UFUNCTION(BlueprintPure, Category = "Character|Wireframe")
	bool IsWireframeAvailable() const;

	// None while Wireframe is off, else how it is drawn (Overlay or ReplaceSlots).
	UFUNCTION(BlueprintPure, Category = "Character|Wireframe")
	EViewerWireframeMode GetActiveWireframeMode() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	// Captures InitialRotation as early as possible (before BeginPlay/
	// ApplyProfile can run), since ACharacterViewerGameMode::PostLogin() may
	// call ApplyProfile() before this actor's own BeginPlay has run (PostLogin
	// runs before World::BeginPlay). See PortfolioCharacterActor.cpp.
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	// Resets turntable rotation (not the enabled state, which persists across
	// a profile switch), expression morphs, material overrides, bone markers
	// and tracked selection ids to a clean slate. Called before a new profile
	// is applied.
	void ClearRuntimeState();

	// Restores the default playback state: Profile->DefaultAnimClass if set,
	// else Profile->DefaultAnimationId's Sequence if set and valid, else an
	// empty AnimationSingleNode state.
	void RestoreDefaultAnimationState();

	// Recomputes every mesh slot from scratch from (CurrentVariantId,
	// bWireframeEnabled, SelectedPartId, bHighlightVisible): defaults, then the
	// variant's overrides, then the wireframe material on every slot if on in
	// ReplaceSlots mode (the default Overlay mode never touches slots),
	// then PartHighlightMaterial on the selected part's slots (MaterialSlots
	// mode). It never layers on the previous override array (the
	// Docs/CHARACTER_VIEWER_SETUP.md "do not copy the override array blindly"
	// pitfall), so every round-trip (Variant/Wireframe/selection/Clean View)
	// is exact.
	void ApplyMaterialState();

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

	EViewerHighlightMode ActiveHighlightMode = EViewerHighlightMode::None;

	// Pooled marker spheres (BoneMarkers mode). Reused across part
	// selections, hidden while the highlight is hidden, destroyed when the
	// selection is cleared or the profile changes.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BoneMarkers;

	// Bone each pooled marker is currently attached to (parallel to BoneMarkers).
	TArray<FName> BoneMarkerBones;

	// Applies CurrentVariantId's slot overrides on top of the current slots.
	// Only called by ApplyMaterialState() after EmptyOverrideMaterials().
	void ApplyVariantOverrides();

	// Mode that would show Part on the current mesh, with the resolved slot
	// indices (MaterialSlots) or marker bones (BoneMarkers). Does not look at
	// bHighlightVisible.
	EViewerHighlightMode ResolveHighlightMode(const FViewerPartInfo* Part, TArray<int32>& OutSlotIndices, TArray<FName>& OutMarkerBones) const;

	// Part.MaterialSlotNames -> unique valid slot indices on the current mesh.
	void ResolvePartSlotIndices(const FViewerPartInfo& Part, TArray<int32>& OutSlotIndices) const;

	// Applies the whole selection highlight (slots via ApplyMaterialState(),
	// markers, overlay tint, Custom Depth) for SelectedPartId according to
	// bHighlightVisible, and updates ActiveHighlightMode.
	void ApplyHighlightState();

	// The mesh has ONE overlay material slot. Precedence: the Wireframe
	// overlay (GetActiveWireframeMode() == Overlay) wins; otherwise the
	// selection tint (WholeMesh mode, or BoneMarkers with
	// bWholeMeshTintWithBoneMarkers); otherwise none. A tint suppressed by the
	// wireframe overlay is logged once per occurrence (Custom Depth stays on).
	void ApplyOverlayState();

	// Mode SetWireframeEnabled(true) would use with the current profile/materials.
	EViewerWireframeMode ResolveWireframeMode() const;

	// True while a suppressed-tint log line has been written for the current
	// Wireframe + WholeMesh combination (reset when the combination ends).
	bool bLoggedTintSuppressedByWireframe = false;

	void UpdateBoneMarkers(const TArray<FName>& MarkerBones, bool bVisible);
	void DestroyBoneMarkers();

	FViewerSlotStats ComputeSlotStats(int32 SlotIndex) const;
};
