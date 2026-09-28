#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleTurntable();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestToggleCleanView();

	UFUNCTION(BlueprintCallable, Category = "Viewer")
	void RequestResetCamera();

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

	bool bFallbackUIBuilt = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> FallbackPanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackDisplayNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FallbackDescriptionText;

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
