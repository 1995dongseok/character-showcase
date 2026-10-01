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
class USizeBox;
class UWidgetTree;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
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
	// Portfolio capture (Docs/CHARACTER_VIEWER_SETUP.md section 1.7): DISPLAY
	// buttons forwarding to ACharacterViewerController::TakePortfolioScreenshot()
	// / StartTurntableCapture() (same as F12 / Shift+F12).
	PortfolioScreenshot,
	TurntableCapture,
	// PLAYBACK section (under ANIMATION) and the DISPLAY LOD/Backdrop rows,
	// forwarding to ACharacterViewerController (same as P, [, ], -, 0, =, L, B).
	ToggleAnimationPause,
	AnimationStepBack,
	AnimationStepForward,
	AnimationRateDown,
	AnimationRateReset,
	AnimationRateUp,
	CycleLOD,
	CycleBackdrop,
	// DISPLAY rows "Ruler: Off (G)" / "Light: Studio (N)" (Docs/CHARACTER_VIEWER_SETUP.md 6.20).
	ToggleHeightRuler,
	CycleLighting,
};

// Raw pointers to the widgets UCharacterViewerWidget::BuildDefaultLayoutTree()
// constructs. One builder is shared by the C++ fallback (BuildFallbackUI())
// and the Editor tool that generates WBP_CharacterViewer's designer tree
// (UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout()), so both
// layouts are identical until a designer edits the WBP.
struct FCharacterViewerLayoutWidgets
{
	UCanvasPanel* RootCanvas = nullptr;
	UBorder* PanelRoot = nullptr;
	UTextBlock* NameText = nullptr;
	UVerticalBox* ControlsBox = nullptr;
	UTextBlock* StatusText = nullptr;
	USizeBox* DescriptionSizeBox = nullptr;
	UScrollBox* DescriptionScroll = nullptr;
	UTextBlock* DescriptionText = nullptr;
	UScrollBox* ListsScroll = nullptr;
	UVerticalBox* ListsBox = nullptr;
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
//    DescriptionScroll/DescriptionText/ListsScroll/ListsBox (7 required) and
//    StatusText (optional) by name. C++ never builds a tree in this case;
//    it only populates the bound widgets (and, with bAutoPanelWidth, sets
//    the right-anchored PanelRoot slot width).
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

	// 8th, OPTIONAL name (not part of the 7 required ones): one-line capture
	// status ("Capturing 12/36", "Saved: Saved/Screenshots/Portfolio/...").
	// Collapsed while there is no status. A designer tree without it still
	// works; the status is simply not shown.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Viewer|Designer")
	TObjectPtr<UTextBlock> StatusText;

	// --- Layout (Docs/CHARACTER_VIEWER_SETUP.md section 2, step 9) ---

	// When true (default), the panel width follows the viewport:
	// clamp(viewport width * PanelWidthFraction, PanelMinWidth, PanelMaxWidth),
	// in Slate units (= pixels at DPI scale 1.0; the project DPI curve in
	// Config/DefaultEngine.ini scales them, 720p = 0.8, 1080p = 1.0). Only
	// applied while PanelRoot sits in a CanvasPanelSlot anchored to the right
	// edge (anchor min/max X == 1); a designer who re-anchors the panel, or
	// turns this off in the WBP's Class Defaults, keeps their own width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout")
	bool bAutoPanelWidth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PanelWidthFraction = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "100.0"))
	float PanelMinWidth = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "100.0"))
	float PanelMaxWidth = 460.f;

	// Font sizes of the runtime-generated rows (buttons / section headers).
	// Headers are intentionally a little larger than button text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "6", ClampMax = "48"))
	int32 ButtonFontSize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "6", ClampMax = "48"))
	int32 HeaderFontSize = 15;

	// Defaults baked into the generated tree (NameText/DescriptionText/StatusText).
	static constexpr int32 DefaultNameFontSize = 20;
	static constexpr int32 DefaultDescriptionFontSize = 13;
	static constexpr int32 DefaultStatusFontSize = 12;
	static constexpr int32 DescriptionMinVisibleLines = 6;
	static constexpr float DefaultPanelWidth = 384.f;
	static constexpr float DefaultPanelPadding = 16.f;

	// clamp(ViewportWidth * Fraction, MinWidth, MaxWidth), never wider than
	// the viewport itself. Pure; unit-tested.
	static float ComputePanelWidth(float ViewportWidth, float Fraction, float MinWidth, float MaxWidth);

	// Conservative line height (Slate units) for a Roboto font of FontSize
	// points. Only the fallback of MeasureTextLinesHeight() when Slate's font
	// services are unavailable.
	static float EstimateLineHeight(int32 FontSize);

	// Height (Slate units) of exactly Lines lines of wrapped UTextBlock text in
	// Font, as Slate lays it out at LayoutScale (DPI scale): each line is a
	// whole number of pixels at that scale (font max character height +
	// |ShadowOffset.Y|, FSlateTextRun::GetMaxHeight), so a box this tall shows
	// Lines whole lines and none of the next one. Measured with a temporary
	// STextBlock (SlatePrepass) through the Slate font measure service; falls
	// back to Lines * EstimateLineHeight() when Slate is not initialized.
	// SampleText: characters the measured lines must contain besides Latin
	// ("Ag"); a line's height is the tallest font run in it, and e.g. Hangul
	// comes from a fallback font taller than Roboto (a Korean description
	// measured with Latin-only lines lost the bottom of its 6th line in the
	// 2026-10-01 -game capture). Pass GetLineHeightSample(the actual text).
	static float MeasureTextLinesHeight(const FSlateFontInfo& Font, int32 Lines, float LayoutScale = 1.f, FVector2D ShadowOffset = FVector2D(1.0, 1.0), const FString& SampleText = FString());

	// DescriptionSizeBox max height that shows exactly Lines whole lines
	// (= MeasureTextLinesHeight; the ScrollBox/TextBlock add no vertical padding).
	static float ComputeDescriptionMaxHeight(const FSlateFontInfo& Font, int32 Lines, float LayoutScale = 1.f, FVector2D ShadowOffset = FVector2D(1.0, 1.0), const FString& SampleText = FString());

	// Up to 16 distinct non-ASCII characters of Text, in order of appearance
	// (empty for pure ASCII text): enough to pull every fallback font the text
	// uses into a measured probe line.
	static FString GetLineHeightSample(const FString& Text);

	// Scrolls ListsScroll to its end on each of the next Ticks NativeTicks
	// (newly built rows -- wrapped text in particular -- only report their
	// final height after a layout pass or two, so a single ScrollToEnd() can
	// stop short). RefreshUI() requests this whenever a NEW part becomes
	// selected while Inspection is on, so the selected-part block (the last
	// rows of the INSPECTION section) comes into view.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Inspection")
	void RequestListsScrollToEnd(int32 Ticks = 5);

	// When true (default), DescriptionScroll's parent SizeBox (DescriptionSizeBox
	// in the generated tree) gets a max height of exactly DescriptionVisibleLines
	// whole lines of DescriptionText's font at the current DPI scale, so the
	// last visible line is never cut in half (2026-10-01 captures showed 6.5
	// lines at 720p/1080p). Re-applied from NativeTick() only when the DPI
	// scale or the font changes. Turn off in the WBP's Class Defaults to keep
	// a designer-set height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout")
	bool bFitDescriptionToWholeLines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viewer|Layout", meta = (ClampMin = "1", ClampMax = "40"))
	int32 DescriptionVisibleLines = DescriptionMinVisibleLines;

	// Applies the whole-line description height for LayoutScale (called every
	// NativeTick with this widget's geometry scale; public so Editor tests can
	// drive it). No-op unless bFitDescriptionToWholeLines and DescriptionScroll
	// sits in a USizeBox.
	void ApplyDescriptionLineFit(float LayoutScale);

	// Last max height applied by ApplyDescriptionLineFit (0 until applied).
	UFUNCTION(BlueprintPure, Category = "Viewer|Layout")
	float GetAppliedDescriptionMaxHeight() const { return AppliedDescriptionMaxHeight; }

	// Builds the default panel tree (RootCanvas > PanelRoot > PanelContent >
	// NameText / ControlsBox / StatusText / DescriptionSizeBox >
	// DescriptionScroll > DescriptionText / ListsScroll(Fill) > ListsBox) into
	// an EMPTY WidgetTree and marks the 8 bindable widgets bIsVariable. Returns
	// false (tree untouched) if Tree is null or already has a RootWidget.
	static bool BuildDefaultLayoutTree(UWidgetTree* Tree, FCharacterViewerLayoutWidgets& Out);

	// Applies the auto panel width for a viewport ViewportWidth Slate units
	// wide (called every NativeTick with this widget's own geometry; public so
	// Editor tests can drive it without a game viewport). No-op unless
	// bAutoPanelWidth and PanelRoot is right-anchored in a CanvasPanelSlot.
	void ApplyAutoPanelWidth(float ViewportWidth);

	// Last width applied by the auto panel width (0 until the first tick that applied it).
	UFUNCTION(BlueprintPure, Category = "Viewer|Layout")
	float GetAppliedPanelWidth() const { return AppliedPanelWidth; }

	// Capture status line (StatusText). Empty text collapses it. Uses the
	// status line's own colour (the green of the generated tree, or whatever
	// a designer WBP gave StatusText).
	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	void SetCaptureStatus(const FText& InStatus);

	// Same line in an explicit colour (profile check result: red for errors,
	// yellow for warnings). The next SetCaptureStatus() restores the default colour.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	void SetStatusLine(const FText& InStatus, FLinearColor InColor);

	// Colour the status line is drawn in now (tests).
	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	FLinearColor GetStatusColor() const;

	UFUNCTION(BlueprintPure, Category = "Viewer|Capture")
	FText GetCaptureStatus() const { return CaptureStatus; }

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

	// The INSPECTION section body (both layouts; BuildInspectionSection() puts
	// it into InspectionBodyText while Inspection is on), built from MEASURED
	// data of the bound Actor:
	//   mesh summary  "Triangles 92,178 · Verts 48,705 · Bones 89 · Slots 2 · LODs 3 · Morphs 0 · Height 182 cm"
	//                 "Skeleton SK_Mannequin · Physics PA_Mannequin" ("measured: n/a" if !bValid)
	//   one line per slot "M_Torso: 54,012 tris · MI_Manny_02_New · 4 tex, max 4096x4096"
	//   then "Click a part", or the selected part's authored DisplayName (PartType)
	//   / Description, "Measured: ..." (GetPartMeasuredStats) or the authored
	//   notes labelled "Authored: ...", and "Highlight: Material slots |
	//   Bone markers (N) | Whole mesh".
	// Just "Click a part" without a bound Actor/Profile. Does not check the
	// Inspection toggle.
	UFUNCTION(BlueprintPure, Category = "Viewer|Inspection")
	FText BuildInspectionText() const;

	// 92178 -> "92,178" (culture-independent, so the panel reads the same on every OS locale).
	static FString FormatThousands(int64 Value);

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

	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	void RequestPortfolioScreenshot();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Capture")
	void RequestTurntableCapture();

	// --- Animation playback (PLAYBACK section) / LOD / Backdrop (DISPLAY rows) ---

	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	void RequestToggleAnimationPause();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	void RequestStepAnimation(int32 Frames);

	// Direction > 0: one AnimationRateStep faster, < 0: slower, 0: reset to 1.0.
	UFUNCTION(BlueprintCallable, Category = "Viewer|Playback")
	void RequestChangeAnimationPlayRate(float Direction);

	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void RequestCycleLOD();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void RequestCycleBackdrop();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void RequestToggleHeightRuler();

	UFUNCTION(BlueprintCallable, Category = "Viewer|Display")
	void RequestCycleLighting();

	// "0.45 s / 1.20 s · frame 14 / 36" (unit-tested).
	static FString FormatPlaybackTime(float Time, float Length, int32 Frame, int32 NumFrames);

	// Test-only accessor: the PLAYBACK section's time line as rendered
	// (empty when the section is hidden or no layout is bound).
	UFUNCTION(BlueprintPure, Category = "Viewer|Playback")
	FText GetPlaybackTimeText() const;

	// Re-reads the bound actor's animation time and updates the PLAYBACK time
	// line only when the frame (or the sequence) changed. Called every
	// NativeTick(); public so Editor tests can drive it without ticking.
	void UpdatePlaybackTimeText();

protected:
	// Builds the fallback tree (if needed) BEFORE UUserWidget::RebuildWidget()
	// converts WidgetTree->RootWidget into Slate. NativeConstruct() runs only
	// after that conversion, so a tree built there never reached the screen.
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	// Applies the auto panel width (bAutoPanelWidth) from this widget's own
	// (viewport-filling) geometry; only touches the slot when the width changes.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// --- Layout (section 13.10) ---

	// Builds the C++ fallback panel (BuildDefaultLayoutTree(), the same tree
	// the Editor tool writes into WBP_CharacterViewer) into this widget's
	// (currently empty) WidgetTree, assigning the SAME PanelRoot/NameText/
	// ControlsBox/DescriptionScroll/DescriptionText/ListsScroll/ListsBox/
	// StatusText members a designer WBP would resolve via BindWidgetOptional.
	// Only ever called once, from RebuildWidget() (outside design time), when
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

	// INSPECTION section (P2-2): shown only while Inspection is on. Body =
	// BuildInspectionText() (measured mesh/slot numbers, then "Click a part"
	// or the selected part). Rebuilt every RefreshUI() call; updates
	// InspectionSectionBox/InspectionBodyText.
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
	// when APortfolioCharacterActor::IsWireframeAvailable() is false.
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

	// --- PLAYBACK section / DISPLAY LOD + Backdrop rows ---

	// Under ANIMATION: [Pause|Resume (P)] [◀ ([)] [▶ (])] / [Slower (-)] [Rate x.xx (0)]
	// [Faster (=)] and the time line. Hidden when the profile has no animation and
	// no AnimBP; buttons disabled while playback is not controllable (AnimBP
	// or nothing playing). Rebuilt every RefreshUI().
	void BuildPlaybackSection(UVerticalBox* Container);

	// "LOD: Auto (L)", "Backdrop: Studio (B)", "Ruler: Off (G)" and
	// "Light: Studio (N)" rows inside DISPLAY.
	void BuildViewOptionRows(UVerticalBox* SectionBox);

	// One equal-width button cell in a horizontal row (AddButtonRow() into a cell box).
	UButton* AddButtonCell(UHorizontalBox* Row, const FText& Label, bool bEnabled, ECharacterViewerButtonKind Kind);

	FText BuildPlaybackTimeLine() const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaybackTimeText;

	// UpdatePlaybackTimeText() cache: the text is only rewritten when one changes.
	int32 LastPlaybackFrame = INDEX_NONE;
	int32 LastPlaybackNumFrames = INDEX_NONE;
	float LastPlaybackLength = -1.f;
	bool bLastPlaybackControllable = false;

	void ApplyCaptureStatus();
	FSlateFontInfo MakeFont(const FSlateFontInfo& Base, int32 Size, FName Typeface) const;
	void AddSectionHeader(UVerticalBox* SectionBox, const FText& Label);

	float AppliedPanelWidth = 0.f;
	FText CaptureStatus;

	// SetStatusLine() colour; unset = StatusText's own colour (captured once
	// into DefaultStatusColor before the first override).
	TOptional<FLinearColor> StatusColorOverride;
	FSlateColor DefaultStatusColor;
	bool bDefaultStatusColorCaptured = false;

	// ApplyDescriptionLineFit() cache: re-measure only when one of these changes.
	float AppliedDescriptionMaxHeight = 0.f;
	float LastDescriptionFitScale = 0.f;
	int32 LastDescriptionFitLines = 0;
	FVector2D LastDescriptionFitShadow = FVector2D::ZeroVector;
	FSlateFontInfo LastDescriptionFitFont;
	FString LastDescriptionFitSample;

	// RequestListsScrollToEnd() countdown, and the part the lists were last
	// auto-scrolled for (so a plain RefreshUI() does not yank the scroll).
	int32 PendingListsScrollToEndTicks = 0;
	FName LastAutoScrolledPartId = NAME_None;
};
