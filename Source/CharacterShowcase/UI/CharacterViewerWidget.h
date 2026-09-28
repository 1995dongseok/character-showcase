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

// Kind of viewer action a fallback-UI button (UCharacterViewerButtonBinding)
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

// Tiny helper object bound to one fallback-panel UButton::OnClicked
// (a dynamic delegate that takes no parameters), so a single HandleClicked()
// can still carry which item/action this particular button represents. See
// UCharacterViewerWidget::BuildFallbackUI(). Declared here (same UI/ files)
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

	UFUNCTION()
	void HandleClicked();
};

// C++ base for WBP_CharacterViewer (P0-5 / P1). Holds no character/camera
// state of its own; every getter reads live state from the bound Actor/Pawn/
// Controller so the widget and the gameplay objects can never disagree.
//
// If the bound WBP (or, when none is assigned, this C++ class itself) has an
// empty designer tree (WidgetTree->RootWidget == nullptr), RebuildWidget()
// builds a minimal fallback UMG panel in C++ (see BuildFallbackUI() /
// Docs/CHARACTER_VIEWER_SETUP.md section 13.10) so the viewer is usable
// without any manual WBP design work. A WBP that designs its own tree
// (RootWidget != nullptr) is left untouched and this fallback is skipped
// entirely.
UCLASS(Blueprintable)
class CHARACTERSHOWCASE_API UCharacterViewerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
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

	// --- Inspection / Wireframe (P2-2) ---

	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	bool IsInspectionEnabled() const;

	UFUNCTION(BlueprintPure, Category = "Viewer|Wireframe")
	bool IsWireframeEnabled() const;

	// True (and fills OutInfo) when the bound Actor has a selected part that
	// exists in its Profile->Parts; it does not itself check the Inspection
	// toggle (the Controller clears the selection when Inspection is turned
	// off, and the fallback INSPECTION section is only shown while it is on).
	// All fields come straight from authored Profile data
	// (Docs/CHARACTER_VIEWER_SETUP.md section 7: never inferred per frame).
	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	bool GetSelectedPartInfo(FViewerPartInfo& OutInfo) const;

	// Called by the Controller (InspectAtScreenPosition, SetInspectionEnabled
	// in both directions, ToggleWireframe) so the fallback panel's INSPECTION
	// section and Inspection/Wireframe button labels refresh without waiting
	// for the next full BindToViewer().
	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void NotifySelectionChanged();

	// Test-only accessors (Tests/CharacterViewerGameSmokeTest.cpp, P2-2
	// evidence): the fallback INSPECTION section box's actual visibility and
	// its body text as rendered. Collapsed / empty if the fallback UI was never built.
	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	ESlateVisibility GetFallbackInspectionSectionVisibility() const;

	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	FText GetFallbackInspectionBodyText() const;

	// Test-only accessor (Tests/CharacterViewerGameSmokeTest.cpp, P1 profile-switch
	// evidence): the fallback panel's actually-rendered DisplayName text, so a test
	// can confirm the panel was rebuilt (RefreshFallbackUI() ran) rather than only
	// that the underlying Profile data changed. Empty if the fallback UI was never built.
	UFUNCTION(BlueprintPure, Category = "Viewer")
	FText GetFallbackDisplayNameText() const;

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
	// --- Fallback UI (section 13.10) ---

	// Builds the minimal C++ fallback panel described in
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.10 into this widget's
	// (currently empty) WidgetTree. Only ever called once, from
	// RebuildWidget() (outside design time), when WidgetTree->RootWidget is null.
	void BuildFallbackUI();

	// Repopulates the fallback panel's dynamic content (name/description,
	// per-section item buttons, Turntable button label) from the currently
	// bound Actor/Pawn/Controller. Safe to call repeatedly; no-ops if the
	// fallback UI was never built (i.e. a designer WBP is in use).
	void RefreshFallbackUI();

	// Clears Container's children and FallbackButtonBindings entries that
	// belonged to it, then adds one disableable UButton+UTextBlock row per
	// Item, wired to Kind/Item.Id via a UCharacterViewerButtonBinding. The
	// section (its header included) is hidden entirely when Items is empty.
	void PopulateFallbackSection(UVerticalBox* SectionBox, UTextBlock* HeaderText, const FText& HeaderLabel, const TArray<FViewerListItem>& Items, ECharacterViewerButtonKind Kind);

	UButton* AddFallbackButtonRow(UVerticalBox* Container, const FText& Label, bool bEnabled, FName Id, ECharacterViewerButtonKind Kind, UTextBlock** OutTextBlock = nullptr);

	// Bound to FallbackPanelBorder->OnMouseButtonDownEvent: a press on the
	// panel background (padding / gaps between buttons) is handled here so it
	// never bubbles to the game viewport. Without this, the unhandled press
	// reached the viewport, which captured the mouse (moving Slate's hover off
	// the panel) before Enhanced Input ran HandleOrbitPressStarted(), so its
	// IsPointerOverPanel() guard read false and the release became an
	// Inspection click (found by the -game smoke's synthesized-click step,
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.11.9). Buttons handle their own clicks first.
	UFUNCTION()
	FEventReply HandleFallbackPanelMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	bool bFallbackUIBuilt = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FallbackPanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackDisplayNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackDescriptionText;

	// CHARACTER: top section, one button per ACharacterViewerGameMode::ProfileLibrary entry (section 6/13.6).
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackCharacterSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackCharacterSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackViewSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackViewSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackAnimationSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackAnimationSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackExpressionSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackExpressionSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackAppearanceSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackAppearanceSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackDisplaySectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackTurntableButtonText;

	// DISPLAY section (P2-2): Inspection (I) / Wireframe (W) buttons live here
	// alongside Turntable/Reset/Clean View. Wireframe is disabled (not hidden)
	// when the profile has no WireframeMaterial.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackInspectionButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FallbackWireframeButton;

	// INSPECTION section (P2-2): shown only while Inspection is on. Shows
	// "Click a part" until a part is selected, then its DisplayName/PartType/
	// Description/TriangleCount/MaterialName/TextureResolution.
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FallbackInspectionSectionBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackInspectionSectionHeader;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackInspectionBodyText;

	// Keeps every UCharacterViewerButtonBinding created by
	// PopulateFallbackSection() alive (they are UObjects held only via
	// TWeakObjectPtr by the buttons' bound delegate target, which is not
	// itself a strong reference).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCharacterViewerButtonBinding>> FallbackButtonBindings;

	TWeakObjectPtr<ACharacterViewerController> WeakController;
	TWeakObjectPtr<APortfolioCharacterActor> WeakActor;
	TWeakObjectPtr<ACharacterViewerCameraPawn> WeakCameraPawn;

	// Tracked here (not on the Actor/Pawn) purely for UI highlight purposes; not gameplay state.
	FName CurrentCameraPresetId = NAME_None;
};
