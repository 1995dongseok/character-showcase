#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterProfileData.h"
#include "CharacterViewerWidget.generated.h"

class ACharacterViewerController;
class APortfolioCharacterActor;
class ACharacterViewerCameraPawn;
class UCharacterViewerWidget;
class UCanvasPanel;
class UBorder;
class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UButton;

// One row for a VIEW/EXPRESSION/ANIMATION/APPEARANCE selection list in the WBP.
USTRUCT(BlueprintType)
struct FViewerListItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	FText DisplayName;

	// False for data that would be a no-op or unsafe to apply (null Sequence,
	// empty Id, a material variant with no valid slot). The WBP should
	// disable/hide such rows instead of calling the matching Request* function.
	UPROPERTY(BlueprintReadOnly, Category = "Viewer")
	bool bEnabled = true;
};

// Kind of viewer action a generated-UI button (UCharacterViewerButtonBinding)
// forwards to, since a dynamically bound UButton::OnClicked has no
// parameters to carry the target id/action itself.
UENUM()
enum class ECharacterViewerButtonKind : uint8
{
	CameraPreset,
	Animation,
	Expression,
	MaterialVariant,
	ToggleTurntable,
	ResetCamera,
	ToggleCleanView,
	ToggleInspection,
	ToggleWireframe,
	// P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): CHARACTER section, one button per
	// ACharacterViewerGameMode::ProfileLibrary entry. Id is the target UCharacterProfileData's own asset FName.
	CharacterProfile,
};

// Tiny helper object bound to one generated-panel UButton::OnClicked
// (a dynamic delegate that takes no parameters), so a single HandleClicked()
// can still carry which item/action this particular button represents. See
// UCharacterViewerWidget::AddButtonRow(). Declared here (same UI/ files)
// per Docs/CHARACTER_VIEWER_SETUP.md section 13.10.
UCLASS()
class CHARACTERSHOWCASE_API UCharacterViewerButtonBinding : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UCharacterViewerWidget> Widget;

	UPROPERTY(Transient)
	FName Id;

	UPROPERTY(Transient)
	ECharacterViewerButtonKind Kind = ECharacterViewerButtonKind::CameraPreset;

	// The generated UButton itself (Test-only lookup: GetGeneratedButtonText()).
	UPROPERTY(Transient)
	TWeakObjectPtr<UButton> ButtonWidget;

	UFUNCTION()
	void HandleClicked();
};

// C++ base for WBP_CharacterViewer (P0-5 / P1 / P2's designer-editable UMG
// pass, Docs/CHARACTER_VIEWER_SETUP.md section 13.10). Holds no character/
// camera state of its own; every getter reads live state from the bound
// Actor/Pawn/Controller so the widget and the gameplay objects can never
// disagree.
//
// Two layout sources, chosen automatically and never mixed:
//  - Designer layout: WBP_CharacterViewer (or any WBP subclassing this) has
//    its own widget tree (WidgetTree->RootWidget != nullptr) and uses
//    BindWidgetOptional to hand this class PanelRoot/NameText/ControlsBox/
//    DescriptionScroll/DescriptionText/ListsScroll/ListsBox by name. C++
//    never builds a tree in this case; it only populates the bound widgets.
//  - Fallback layout: an empty designer tree. RebuildWidget() builds a
//    minimal UMG tree in C++ (BuildFallbackUI()) using the exact same
//    PanelRoot/NameText/... member names, so RefreshUI()/AddListSection()/
//    AddButtonRow() are shared by both paths (one code path fills whichever
//    containers ended up bound).
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API UCharacterViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Designer-bindable widgets (section 13.10) ---
	// A WBP_CharacterViewer designer tree must use exactly these names (and
	// mark each "Is Variable") for C++ to find and populate them. If
	// PanelRoot resolves to a real designer widget, BuildFallbackUI() never
	// runs; otherwise these are filled in by BuildFallbackUI() instead.

	// Right-side panel background; hit-testable so a press/drag/wheel over it
	// never reaches Orbit/Zoom/Inspection (IsPointerOverPanel(), section 4/13.11.5).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UBorder> PanelRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UTextBlock> NameText;

	// Holds the CHARACTER / VIEW / DISPLAY sections (name + primary controls, always visible, never scrolled away).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UVerticalBox> ControlsBox;

	// Limited-height scroll area for the (possibly long) Description text only.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UScrollBox> DescriptionScroll;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UTextBlock> DescriptionText;

	// Scroll area for the ANIMATION / EXPRESSION / APPEARANCE / INSPECTION sections.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UScrollBox> ListsScroll;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UVerticalBox> ListsBox;

	// True once BuildFallbackUI()/RefreshUI() has run and PanelRoot resolved
	// from a real designer tree (not the C++ fallback). Test/diagnostic use.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsUsingDesignerLayout() const;

	// Called by ACharacterViewerController after possessing/finding the
	// viewer actor and pawn, and again whenever the profile is switched.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void BindToViewer(ACharacterViewerController* InController, APortfolioCharacterActor* InActor, ACharacterViewerCameraPawn* InCameraPawn);

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetDescription() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetCameraPresets() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetAnimations() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetExpressions() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetMaterialVariants() const;

	// P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): the
	// current ACharacterViewerGameMode's ProfileLibrary, one row per profile
	// (Id = that UCharacterProfileData's own asset FName). Empty (section
	// hidden) if there is no GameMode or ProfileLibrary is empty.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	TArray<FViewerListItem> GetCharacterLibrary() const;

	// The bound Actor's current Profile's own asset FName, or NAME_None. Used
	// to mark the active CHARACTER row (section 13.10 "current selection display").
	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentCharacterProfileId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsTurntableEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentAnimationId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentExpressionId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentMaterialVariantId() const;

	UFUNCTION(BlueprintPure, Category = "Viewer")
	FName GetCurrentCameraPresetId() const { return CurrentCameraPresetId; }

	// Sets the currently-highlighted VIEW/camera-preset row without going
	// through RequestCameraPreset() (which also forwards the selection to the
	// Controller -- not wanted here, since the Controller is what calls this).
	// Used by ACharacterViewerController::ResetCamera() so R / Reset Camera
	// puts the "▶" back on the profile's own DefaultPresetId instead of
	// leaving a stale, previously-selected preset marked as current (section
	// 13.10 "state display"). Does not itself call RefreshUI(); the caller
	// does that afterward (typically via NotifySelectionChanged()).
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void SetCurrentCameraPresetId(FName Id) { CurrentCameraPresetId = Id; }

	// --- Inspection / Wireframe (P2-2) ---

	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	bool IsInspectionEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Viewer|Wireframe")
	bool IsWireframeEnabled() const;

	// True (and fills OutInfo) when the bound Actor has a selected part that
	// exists in its Profile->Parts; it does not itself check the Inspection
	// toggle (the Controller clears the selection when Inspection is turned
	// off, and the INSPECTION section is only shown while it is on).
	// All fields come straight from authored Profile data
	// (Docs/CHARACTER_VIEWER_SETUP.md section 7: never inferred per frame).
	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	bool GetSelectedPartInfo(FViewerPartInfo& OutInfo) const;

	// Called by the Controller (InspectAtScreenPosition, SetInspectionEnabled
	// in both directions, ToggleWireframe) so the INSPECTION section and
	// Inspection/Wireframe button labels refresh without waiting for the next
	// full BindToViewer().
	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void NotifySelectionChanged();

	// Test-only accessors (Tests/CharacterViewerGameSmokeTest.cpp, P2-2
	// evidence): the generated INSPECTION section box's actual visibility and
	// its body text as rendered. Path-independent: reads InspectionSectionBox/
	// InspectionBodyText, which RefreshUI() fills on both the designer and
	// fallback layout paths. Collapsed / empty if neither layout is bound yet.
	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	ESlateVisibility GetFallbackInspectionSectionVisibility() const;

	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	FText GetFallbackInspectionBodyText() const;

	// Test-only accessor (Tests/CharacterViewerGameSmokeTest.cpp, P1 profile-switch
	// evidence): the actually-rendered NameText text, so a test can confirm the
	// panel was rebuilt (RefreshUI() ran) rather than only that the underlying
	// Profile data changed. Path-independent (see above). Empty if neither
	// layout is bound yet.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetFallbackDisplayNameText() const;

	// Test-only accessor: the rendered label text of the generated button for
	// (Kind, Id) -- e.g. (ToggleTurntable, NAME_None) or (CameraPreset, "Face")
	// -- so a test can confirm a button's current-selection/toggle-state text
	// (e.g. "Turntable: On (Space)", "▶ Face") without re-deriving it.
	// Empty if no such button exists in the last RefreshUI().
	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetGeneratedButtonText(ECharacterViewerButtonKind Kind, FName Id) const;

	// True while the pointer is over this panel; the Controller uses this to
	// skip Orbit/Zoom when a press/drag/wheel started over UMG instead of the viewport.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	bool IsPointerOverPanel() const;

	// Fired after BindToViewer() (initial bind and every profile switch) so the WBP graph can rebuild its lists.
	UFUNCTION(BlueprintImplementableEvent, Category = "Viewer")
	void OnViewerDataChanged();

	// Thin forwarding calls for WBP button/list click handlers; all real logic lives in the Controller/Actor/Pawn.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestCameraPreset(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestAnimation(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestExpression(FName Id);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestMaterialVariant(FName Id);

	// P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): forwards to
	// ACharacterViewerController::SelectCharacterProfile(), i.e. a CHARACTER section button click.
	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestCharacterProfile(FName ProfileAssetName);

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleTurntable();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleCleanView();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestResetCamera();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void RequestToggleInspection();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Wireframe")
	void RequestToggleWireframe();

protected:
	// Builds the fallback tree (if needed) BEFORE UUserWidget::RebuildWidget()
	// converts WidgetTree->RootWidget into Slate. NativeConstruct() runs only
	// after that conversion, so a tree built there never reached the screen.
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// --- Layout (section 13.10) ---

	// Builds the minimal C++ fallback panel described in
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.10 into this widget's
	// (currently empty) WidgetTree, assigning the SAME PanelRoot/NameText/
	// ControlsBox/DescriptionScroll/DescriptionText/ListsScroll/ListsBox
	// members a designer WBP would resolve via BindWidgetOptional. Only ever
	// called once, from RebuildWidget() (outside design time), when
	// WidgetTree->RootWidget is null.
	void BuildFallbackUI();

	// Repopulates NameText/DescriptionText and regenerates every section
	// (CHARACTER/VIEW/DISPLAY into ControlsBox, ANIMATION/EXPRESSION/
	// APPEARANCE/INSPECTION into ListsBox) from the currently bound
	// Actor/Pawn/Controller. Path-independent: operates purely on
	// PanelRoot/NameText/ControlsBox/... regardless of whether they came from
	// a designer WBP or BuildFallbackUI(). No-op if PanelRoot is unbound
	// (neither layout is ready yet).
	void RefreshUI();

	// Constructs a fresh header+button-row VerticalBox and adds it as a child
	// of Container. Hidden entirely (nothing added) when Items is empty. Used
	// for the four data-driven sections (CHARACTER/VIEW/ANIMATION/EXPRESSION/
	// APPEARANCE); DISPLAY and INSPECTION are built separately (fixed rows /
	// conditional visibility) by BuildDisplaySection()/BuildInspectionSection().
	void AddListSection(UVerticalBox* Container, const FText& HeaderLabel, const TArray<FViewerListItem>& Items, ECharacterViewerButtonKind Kind, FName CurrentSelectedId);

	// Fixed (non-data-driven) DISPLAY rows: Turntable/Reset/Clean View/
	// Inspection/Wireframe. Rebuilt every RefreshUI() call so their labels
	// always show the current toggle state.
	void BuildDisplaySection(UVerticalBox* Container);

	// INSPECTION section (P2-2): shown only while Inspection is on. Shows
	// "Click a part" until a part is selected, then its DisplayName/PartType/
	// Description/TriangleCount/MaterialName/TextureResolution. Rebuilt every
	// RefreshUI() call; updates InspectionSectionBox/InspectionBodyText.
	void BuildInspectionSection(UVerticalBox* Container);

	// Adds one disableable UButton+UTextBlock row to Container, wired to
	// Kind/Id via a UCharacterViewerButtonBinding (kept alive in
	// ButtonBindings). bSelected prefixes the label so the current selection/
	// toggle state is visible (section 13.10 "state display").
	UButton* AddButtonRow(UVerticalBox* Container, const FText& Label, bool bEnabled, bool bSelected, FName Id, ECharacterViewerButtonKind Kind, UTextBlock** OutTextBlock = nullptr);

	// Bound to PanelRoot->OnMouseButtonDownEvent (both layout paths, from
	// NativeConstruct()): a press on the panel background (padding / gaps
	// between buttons/scroll areas) is handled here so it never bubbles to
	// the game viewport. Without this, the unhandled press reached the
	// viewport, which captured the mouse (moving Slate's hover off the
	// panel) before Enhanced Input ran HandleOrbitPressStarted(), so its
	// IsPointerOverPanel() guard read false and the release became an
	// Inspection click (found by the -game smoke's synthesized-click step,
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.11.9). Buttons/scroll areas
	// handle their own input first.
	UFUNCTION()
	FEventReply HandleFallbackPanelMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	// True once BuildFallbackUI() has run (i.e. PanelRoot etc. came from the
	// C++ fallback, not a designer tree). Used by IsUsingDesignerLayout().
	bool bFallbackUIBuilt = false;

	// DISPLAY section (P2-2): Inspection (I) / Wireframe (W) buttons live here
	// alongside Turntable/Reset/Clean View. Wireframe is disabled (not hidden)
	// when the profile has no WireframeMaterial.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TurntableButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectionButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WireframeButton;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> InspectionSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InspectionBodyText;

	// Keeps every UCharacterViewerButtonBinding created by AddButtonRow()
	// alive (they are UObjects held only via TWeakObjectPtr by the buttons'
	// bound delegate target, which is not itself a strong reference).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCharacterViewerButtonBinding>> ButtonBindings;

	TWeakObjectPtr<ACharacterViewerController> WeakController;
	TWeakObjectPtr<APortfolioCharacterActor> WeakActor;
	TWeakObjectPtr<ACharacterViewerCameraPawn> WeakCameraPawn;

	// Tracked here (not on the Actor/Pawn) purely for UI highlight purposes; not gameplay state.
	FName CurrentCameraPresetId = NAME_None;
};
