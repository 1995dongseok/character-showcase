#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterViewer/ViewerCapture.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/DateTime.h"
#include "Rendering/SlateRenderer.h"
#include "UI/CharacterViewerWidget.h"
#include "UObject/Package.h"

// Editor (NullRHI) unit tests for the 2026-10-01 panel layout / portfolio
// capture pass (Docs/CHARACTER_VIEWER_SETUP.md sections 1.7, 2 step 9, 4).
// No rendering: layout math, the shared default tree, the generated WBP's
// tree, file naming and the capture state machine.

namespace CharacterViewerLayoutCaptureTests
{
	struct FNamedWidget
	{
		const TCHAR* Name;
		UClass* Class;
	};

	static TArray<FNamedWidget> GetBindableWidgets()
	{
		return {
			{ TEXT("PanelRoot"), UBorder::StaticClass() },
			{ TEXT("NameText"), UTextBlock::StaticClass() },
			{ TEXT("ControlsBox"), UVerticalBox::StaticClass() },
			{ TEXT("DescriptionScroll"), UScrollBox::StaticClass() },
			{ TEXT("DescriptionText"), UTextBlock::StaticClass() },
			{ TEXT("ListsScroll"), UScrollBox::StaticClass() },
			{ TEXT("ListsBox"), UVerticalBox::StaticClass() },
			{ TEXT("StatusText"), UTextBlock::StaticClass() },
		};
	}

	// Whole-line rule (2026-10-01): a description box of Height Slate units at
	// LayoutScale shows exactly DescriptionMinVisibleLines (>= 6) WHOLE lines
	// of Font and nothing of the next line (the old 134-unit box showed ~6.5).
	static void CheckWholeLineHeight(FAutomationTestBase& Test, const FString& Label, float Height, const FSlateFontInfo& Font, FVector2D Shadow, float LayoutScale, const FString& Sample = FString())
	{
		const int32 Lines = UCharacterViewerWidget::DescriptionMinVisibleLines;
		const float OneLine = UCharacterViewerWidget::MeasureTextLinesHeight(Font, 1, LayoutScale, Shadow, Sample);
		const float NLines = UCharacterViewerWidget::MeasureTextLinesHeight(Font, Lines, LayoutScale, Shadow, Sample);
		const float NextLines = UCharacterViewerWidget::MeasureTextLinesHeight(Font, Lines + 1, LayoutScale, Shadow, Sample);
		const bool bMeasured = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr;
		uint16 FontMaxHeightPx = 0;
		if (bMeasured)
		{
			FontMaxHeightPx = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(Font, LayoutScale);
		}
		Test.AddInfo(FString::Printf(TEXT("%s: font %.1f pt %s, scale %.2f, measured via Slate=%d: line %.3f units (%.2f px, font max char height %d px), %d lines %.3f, %d lines %.3f, box %.3f units = %.3f lines (old 134-unit box = %.2f lines)"),
			*Label, static_cast<float>(Font.Size), *Font.TypefaceFontName.ToString(), LayoutScale, bMeasured ? 1 : 0, OneLine, OneLine * LayoutScale, static_cast<int32>(FontMaxHeightPx),
			Lines, NLines, Lines + 1, NextLines, Height, OneLine > 0.f ? Height / OneLine : 0.f, OneLine > 0.f ? 134.f / OneLine : 0.f));

		Test.TestTrue(*FString::Printf(TEXT("%s: at least 6 description lines"), *Label), Lines >= 6);
		if (!Test.TestTrue(*FString::Printf(TEXT("%s: line height > 0"), *Label), OneLine > 0.f))
		{
			return;
		}
		Test.TestEqual(*FString::Printf(TEXT("%s: box height is exactly %d measured lines"), *Label, Lines), Height, NLines, 0.01f);
		Test.TestEqual(*FString::Printf(TEXT("%s: %d lines = %d x one line (uniform line height)"), *Label, Lines, Lines), NLines, OneLine * Lines, 0.01f);
		Test.TestEqual(*FString::Printf(TEXT("%s: the next line starts exactly at the box bottom (nothing of it shows)"), *Label), NextLines - Height, OneLine, 0.01f);
		Test.TestEqual(*FString::Printf(TEXT("%s: box / line height is a whole number (%d)"), *Label, Lines), Height / OneLine, static_cast<float>(Lines), 1e-3f);
		if (bMeasured)
		{
			Test.TestTrue(*FString::Printf(TEXT("%s: measured line height (%.2f px) is at least the font max character height (%d px)"), *Label, OneLine * LayoutScale, static_cast<int32>(FontMaxHeightPx)),
				OneLine * LayoutScale + 0.01f >= static_cast<float>(FontMaxHeightPx));
		}
	}

	// Shared shape assertions for any tree built by BuildDefaultLayoutTree()
	// (a fresh transient tree, or the generated WBP_CharacterViewer archetype).
	static void CheckDefaultTreeShape(FAutomationTestBase& Test, const FString& Label, const UWidgetTree* Tree)
	{
		for (const FNamedWidget& Expected : GetBindableWidgets())
		{
			const UWidget* Found = Tree ? Tree->FindWidget(FName(Expected.Name)) : nullptr;
			if (Test.TestNotNull(*FString::Printf(TEXT("%s: '%s' exists"), *Label, Expected.Name), Found))
			{
				Test.TestTrue(*FString::Printf(TEXT("%s: '%s' is a %s"), *Label, Expected.Name, *Expected.Class->GetName()), Found->IsA(Expected.Class));
				Test.TestTrue(*FString::Printf(TEXT("%s: '%s' is marked Is Variable"), *Label, Expected.Name), (bool)Found->bIsVariable);
			}
		}

		const UBorder* PanelRoot = Tree ? Cast<UBorder>(Tree->FindWidget(TEXT("PanelRoot"))) : nullptr;
		const UCanvasPanelSlot* PanelSlot = PanelRoot ? Cast<UCanvasPanelSlot>(PanelRoot->Slot) : nullptr;
		if (Test.TestNotNull(*FString::Printf(TEXT("%s: PanelRoot sits in a CanvasPanelSlot"), *Label), PanelSlot))
		{
			const FAnchors Anchors = PanelSlot->GetAnchors();
			Test.TestEqual(*FString::Printf(TEXT("%s: PanelRoot anchor min X = 1 (right edge)"), *Label), Anchors.Minimum.X, 1.0, 1e-4);
			Test.TestEqual(*FString::Printf(TEXT("%s: PanelRoot anchor max X = 1 (right edge)"), *Label), Anchors.Maximum.X, 1.0, 1e-4);
			Test.TestEqual(*FString::Printf(TEXT("%s: PanelRoot anchor min Y = 0 (full height)"), *Label), Anchors.Minimum.Y, 0.0, 1e-4);
			Test.TestEqual(*FString::Printf(TEXT("%s: PanelRoot anchor max Y = 1 (full height)"), *Label), Anchors.Maximum.Y, 1.0, 1e-4);
			Test.TestEqual(*FString::Printf(TEXT("%s: PanelRoot alignment X = 1"), *Label), PanelSlot->GetAlignment().X, 1.0, 1e-4);
			Test.TestTrue(*FString::Printf(TEXT("%s: PanelRoot default width >= 300"), *Label), PanelSlot->GetOffsets().Right >= 300.f);
		}

		const UScrollBox* ListsScroll = Tree ? Cast<UScrollBox>(Tree->FindWidget(TEXT("ListsScroll"))) : nullptr;
		const UVerticalBoxSlot* ListsSlot = ListsScroll ? Cast<UVerticalBoxSlot>(ListsScroll->Slot) : nullptr;
		if (Test.TestNotNull(*FString::Printf(TEXT("%s: ListsScroll sits in a VerticalBoxSlot"), *Label), ListsSlot))
		{
			Test.TestEqual(*FString::Printf(TEXT("%s: ListsScroll slot size rule is Fill"), *Label), (int32)ListsSlot->GetSize().SizeRule, (int32)ESlateSizeRule::Fill);
		}

		const USizeBox* DescriptionSize = Tree ? Cast<USizeBox>(Tree->FindWidget(TEXT("DescriptionSizeBox"))) : nullptr;
		const UTextBlock* DescriptionText = Tree ? Cast<UTextBlock>(Tree->FindWidget(TEXT("DescriptionText"))) : nullptr;
		if (Test.TestNotNull(*FString::Printf(TEXT("%s: DescriptionSizeBox exists"), *Label), DescriptionSize)
			&& Test.TestNotNull(*FString::Printf(TEXT("%s: DescriptionText exists"), *Label), DescriptionText))
		{
			Test.TestTrue(*FString::Printf(TEXT("%s: DescriptionText is DescriptionSizeBox > DescriptionScroll > DescriptionText"), *Label),
				DescriptionText->GetParent() && DescriptionText->GetParent()->GetParent() == DescriptionSize);
			CheckWholeLineHeight(Test, Label + TEXT(" (stored, scale 1.0)"), DescriptionSize->GetMaxDesiredHeight(), DescriptionText->GetFont(), DescriptionText->GetShadowOffset(), 1.f);
		}

		const UTextBlock* StatusText = Tree ? Cast<UTextBlock>(Tree->FindWidget(TEXT("StatusText"))) : nullptr;
		if (StatusText)
		{
			Test.TestEqual(*FString::Printf(TEXT("%s: StatusText starts Collapsed"), *Label), StatusText->GetVisibility(), ESlateVisibility::Collapsed);
		}
	}
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerPanelLayoutTest,
	"CharacterShowcase.Viewer.PanelLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerPanelLayoutTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerLayoutCaptureTests;

	// 1. Width rule: clamp(24% of viewport, 300, 460) Slate units.
	TestEqual(TEXT("Panel width: 1600 Slate units wide (1280x720 at 0.8) -> 384"), UCharacterViewerWidget::ComputePanelWidth(1600.f, 0.24f, 300.f, 460.f), 384.f, 0.01f);
	TestEqual(TEXT("Panel width: 1920 (1080p at 1.0) -> clamped to 460"), UCharacterViewerWidget::ComputePanelWidth(1920.f, 0.24f, 300.f, 460.f), 460.f, 0.01f);
	TestEqual(TEXT("Panel width: 1000 -> clamped up to 300"), UCharacterViewerWidget::ComputePanelWidth(1000.f, 0.24f, 300.f, 460.f), 300.f, 0.01f);
	TestEqual(TEXT("Panel width: never wider than the viewport (250)"), UCharacterViewerWidget::ComputePanelWidth(250.f, 0.24f, 300.f, 460.f), 250.f, 0.01f);

	// 2. Project DPI curve (Config/DefaultEngine.ini, ShortestSide).
	const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>();
	if (TestNotNull(TEXT("UUserInterfaceSettings exists"), UISettings))
	{
		const float Scale720 = UISettings->GetDPIScaleBasedOnSize(FIntPoint(1280, 720));
		const float Scale1080 = UISettings->GetDPIScaleBasedOnSize(FIntPoint(1920, 1080));
		const float Scale1440 = UISettings->GetDPIScaleBasedOnSize(FIntPoint(2560, 1440));
		TestEqual(TEXT("DPI scale at 1280x720 is 0.8"), Scale720, 0.8f, 0.01f);
		TestEqual(TEXT("DPI scale at 1920x1080 is 1.0"), Scale1080, 1.0f, 0.01f);
		TestEqual(TEXT("DPI scale at 2560x1440 is 1.25"), Scale1440, 1.25f, 0.01f);

		if (Scale720 > 0.f && Scale1080 > 0.f)
		{
			const float Width720 = UCharacterViewerWidget::ComputePanelWidth(1280.f / Scale720, 0.24f, 300.f, 460.f);
			const float Width1080 = UCharacterViewerWidget::ComputePanelWidth(1920.f / Scale1080, 0.24f, 300.f, 460.f);
			AddInfo(FString::Printf(TEXT("Panel at 1280x720: %.0f Slate units = %.0f px; at 1920x1080: %.0f Slate units = %.0f px (was 320 units = ~213 px at 720p)."),
				Width720, Width720 * Scale720, Width1080, Width1080 * Scale1080));
			TestTrue(TEXT("Panel at 1280x720 is >= 300 Slate units"), Width720 >= 300.f);
			TestTrue(TEXT("Panel at 1280x720 is wider on screen than the old ~213 px"), Width720 * Scale720 > 280.f);
		}
	}

	// 3. The shared default tree (C++ fallback == Editor-tool WBP tree).
	UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage(), NAME_None, RF_Transient);
	FCharacterViewerLayoutWidgets Built;
	TestTrue(TEXT("BuildDefaultLayoutTree builds into an empty tree"), UCharacterViewerWidget::BuildDefaultLayoutTree(Tree, Built));
	CheckDefaultTreeShape(*this, TEXT("Transient default tree"), Tree);
	FCharacterViewerLayoutWidgets Again;
	TestFalse(TEXT("BuildDefaultLayoutTree refuses a tree that already has a root (never overwrites)"), UCharacterViewerWidget::BuildDefaultLayoutTree(Tree, Again));
	TestNull(TEXT("Refused call returns no widgets"), Again.PanelRoot);

	// 4. Runtime fallback widget (no designer tree): builds, wraps text, applies the auto width.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (TestNotNull(TEXT("Transient world exists"), World))
	{
		UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
		if (TestNotNull(TEXT("Fallback UCharacterViewerWidget created"), Widget))
		{
			Widget->TakeWidget(); // RebuildWidget() -> BuildFallbackUI()
			TestFalse(TEXT("Native class with empty tree uses the C++ fallback"), Widget->IsUsingDesignerLayout());
			CheckDefaultTreeShape(*this, TEXT("Fallback widget tree"), Widget->WidgetTree);
			TestNotNull(TEXT("Fallback binds StatusText"), Widget->StatusText.Get());

			Widget->NotifySelectionChanged(); // RefreshUI() without a controller: DISPLAY rows still exist
			const FString ScreenshotLabel = Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::PortfolioScreenshot, NAME_None).ToString();
			const FString TurntableLabel = Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::TurntableCapture, NAME_None).ToString();
			TestEqual(TEXT("DISPLAY has the 'Screenshot (F12)' button"), ScreenshotLabel, FString(TEXT("Screenshot (F12)")));
			TestEqual(TEXT("DISPLAY has the 'Turntable Shots (Shift+F12)' button"), TurntableLabel, FString(TEXT("Turntable Shots (Shift+F12)")));

			int32 ButtonLabels = 0;
			int32 WrappedSmallLabels = 0;
			Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
			{
				if (const UTextBlock* Label = Cast<UTextBlock>(Child))
				{
					if (Label->GetParent() && Label->GetParent()->IsA(UButton::StaticClass()))
					{
						++ButtonLabels;
						if (Label->GetFont().Size == Widget->ButtonFontSize && Label->GetAutoWrapText())
						{
							++WrappedSmallLabels;
						}
					}
				}
			});
			TestTrue(TEXT("RefreshUI generated button labels"), ButtonLabels > 0);
			TestEqual(TEXT("Every button label uses ButtonFontSize and auto-wraps (no clipping)"), WrappedSmallLabels, ButtonLabels);
			TestTrue(TEXT("Section headers are larger than button text"), Widget->HeaderFontSize > Widget->ButtonFontSize);

			Widget->ApplyAutoPanelWidth(1600.f);
			const UCanvasPanelSlot* PanelSlot = Widget->PanelRoot ? Cast<UCanvasPanelSlot>(Widget->PanelRoot->Slot) : nullptr;
			if (TestNotNull(TEXT("Fallback PanelRoot slot"), PanelSlot))
			{
				TestEqual(TEXT("Auto width at a 1600-unit viewport sets the slot width to 384"), PanelSlot->GetOffsets().Right, 384.f, 0.5f);
			}
			Widget->ApplyAutoPanelWidth(2400.f);
			TestEqual(TEXT("Auto width at a 2400-unit viewport clamps to 460"), Widget->GetAppliedPanelWidth(), 460.f, 0.5f);

			// Whole-line description height re-fitted per DPI scale (720p = 0.8, 1080p = 1.0, 1440p = 1.25).
			USizeBox* FallbackDescriptionBox = Cast<USizeBox>(Widget->WidgetTree->FindWidget(TEXT("DescriptionSizeBox")));
			if (TestNotNull(TEXT("Fallback DescriptionSizeBox"), FallbackDescriptionBox) && TestNotNull(TEXT("Fallback DescriptionText"), Widget->DescriptionText.Get()))
			{
				TestTrue(TEXT("bFitDescriptionToWholeLines is on by default"), Widget->bFitDescriptionToWholeLines);
				TestEqual(TEXT("DescriptionVisibleLines defaults to 6"), Widget->DescriptionVisibleLines, 6);
				const FSlateFontInfo Font = Widget->DescriptionText->GetFont();
				const FVector2D Shadow = Widget->DescriptionText->GetShadowOffset();
				for (const float Scale : { 0.8f, 1.0f, 1.25f })
				{
					Widget->ApplyDescriptionLineFit(Scale);
					const float Applied = FallbackDescriptionBox->GetMaxDesiredHeight();
					TestEqual(*FString::Printf(TEXT("Line fit at scale %.2f: getter matches the SizeBox"), Scale), Widget->GetAppliedDescriptionMaxHeight(), Applied, 0.01f);
					CheckWholeLineHeight(*this, FString::Printf(TEXT("Fallback runtime fit @%.2f"), Scale), Applied, Font, Shadow, Scale);
				}

				// Korean description (Hangul comes from a fallback font taller than
				// Roboto): the fit must measure with the text's own characters.
				const FString Korean = TEXT("\uC5B8\uB9AC\uC5BC \uC5D4\uC9C4 3\uC778\uCE6D \uD15C\uD50C\uB9BF\uC758 \uAE30\uBCF8 \uB9C8\uB124\uD0B9 (SKM_Manny_Simple)");
				const FString KoreanSample = UCharacterViewerWidget::GetLineHeightSample(Korean);
				TestTrue(FString::Printf(TEXT("Korean text yields a non-empty line-height sample (%d chars)"), KoreanSample.Len()), KoreanSample.Len() > 0 && KoreanSample.Len() <= 16);
				TestTrue(TEXT("ASCII text yields an empty sample"), UCharacterViewerWidget::GetLineHeightSample(TEXT("Plain ASCII text")).IsEmpty());
				Widget->DescriptionText->SetText(FText::FromString(Korean));
				for (const float Scale : { 0.8f, 1.0f })
				{
					Widget->ApplyDescriptionLineFit(Scale);
					const float Applied = FallbackDescriptionBox->GetMaxDesiredHeight();
					const float LatinHeight = UCharacterViewerWidget::MeasureTextLinesHeight(Font, Widget->DescriptionVisibleLines, Scale, Shadow);
					AddInfo(FString::Printf(TEXT("Korean description fit @%.2f: %.2f units (Latin-only lines: %.2f)"), Scale, Applied, LatinHeight));
					TestTrue(FString::Printf(TEXT("Korean fit @%.2f is at least the Latin-only height"), Scale), Applied + 0.01f >= LatinHeight);
					CheckWholeLineHeight(*this, FString::Printf(TEXT("Korean runtime fit @%.2f"), Scale), Applied, Font, Shadow, Scale, KoreanSample);
				}
				Widget->DescriptionText->SetText(FText::GetEmpty());

				Widget->bFitDescriptionToWholeLines = false;
				const float Before = FallbackDescriptionBox->GetMaxDesiredHeight();
				Widget->ApplyDescriptionLineFit(0.8f);
				TestEqual(TEXT("bFitDescriptionToWholeLines=false leaves the height alone"), FallbackDescriptionBox->GetMaxDesiredHeight(), Before, 0.001f);
				Widget->bFitDescriptionToWholeLines = true;
			}

			Widget->SetCaptureStatus(FText::FromString(TEXT("Capturing 12/36")));
			TestEqual(TEXT("StatusText shows the capture status"), Widget->StatusText->GetText().ToString(), FString(TEXT("Capturing 12/36")));
			TestNotEqual(TEXT("StatusText is visible while it has text"), Widget->StatusText->GetVisibility(), ESlateVisibility::Collapsed);
			Widget->SetCaptureStatus(FText::GetEmpty());
			TestEqual(TEXT("Empty status collapses StatusText"), Widget->StatusText->GetVisibility(), ESlateVisibility::Collapsed);

			Widget->RemoveFromParent();
		}
		World->DestroyWorld(false);
	}

	// 5. The generated WBP_CharacterViewer (Scripts/CreateViewerWidgetLayout.py) carries the same tree.
	UWidgetBlueprintGeneratedClass* WbpClass = LoadObject<UWidgetBlueprintGeneratedClass>(nullptr, TEXT("/Game/Portfolio/UI/WBP_CharacterViewer.WBP_CharacterViewer_C"));
	if (TestNotNull(TEXT("WBP_CharacterViewer_C loads"), WbpClass))
	{
		TestTrue(TEXT("WBP_CharacterViewer derives from UCharacterViewerWidget"), WbpClass->IsChildOf(UCharacterViewerWidget::StaticClass()));
		CheckDefaultTreeShape(*this, TEXT("WBP_CharacterViewer"), WbpClass->GetWidgetTreeArchetype());
	}

	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerCaptureNamingTest,
	"CharacterShowcase.Viewer.CaptureNaming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerCaptureNamingTest::RunTest(const FString& Parameters)
{
	const FDateTime Time(2026, 10, 1, 14, 5, 9);

	TestEqual(TEXT("Timestamp is yyyyMMdd-HHmmss"), ViewerCapture::FormatTimestamp(Time), FString(TEXT("20261001-140509")));
	TestEqual(TEXT("Screenshot name: <Profile>_<Preset>_<timestamp>.png"),
		ViewerCapture::MakeScreenshotFileName(TEXT("DA_Character"), TEXT("Full"), Time), FString(TEXT("DA_Character_Full_20261001-140509.png")));
	TestEqual(TEXT("Unsafe characters are replaced"),
		ViewerCapture::MakeScreenshotFileName(TEXT("DA Hero/1"), TEXT("Upper:Body"), Time), FString(TEXT("DA_Hero_1_Upper_Body_20261001-140509.png")));
	TestEqual(TEXT("Empty profile/preset fall back to NoProfile/Default"),
		ViewerCapture::MakeScreenshotFileName(TEXT(""), TEXT(" "), Time), FString(TEXT("NoProfile_Default_20261001-140509.png")));
	TestEqual(TEXT("Turntable folder: Turntable_<Profile>_<timestamp>"),
		ViewerCapture::MakeTurntableFolderName(TEXT("DA_Character"), Time), FString(TEXT("Turntable_DA_Character_20261001-140509")));
	TestEqual(TEXT("Frame 0 file"), ViewerCapture::MakeTurntableFrameFileName(0), FString(TEXT("frame_000.png")));
	TestEqual(TEXT("Frame 35 file"), ViewerCapture::MakeTurntableFrameFileName(35), FString(TEXT("frame_035.png")));

	TestEqual(TEXT("10 degree step -> 36 frames"), ViewerCapture::GetTurntableFrameCount(10.f), 36);
	TestEqual(TEXT("15 degree step -> 24 frames"), ViewerCapture::GetTurntableFrameCount(15.f), 24);
	TestEqual(TEXT("Invalid step falls back to 10 degrees (36 frames)"), ViewerCapture::GetTurntableFrameCount(0.f), 36);
	TestEqual(TEXT("Frame 0 yaw offset is 0"), ViewerCapture::GetTurntableYawOffset(0, 10.f), 0.f, 1e-4f);
	TestEqual(TEXT("Frame 12 yaw offset is 120"), ViewerCapture::GetTurntableYawOffset(12, 10.f), 120.f, 1e-4f);
	TestEqual(TEXT("Last frame (35) yaw offset is 350, so the loop does not repeat frame 0"), ViewerCapture::GetTurntableYawOffset(35, 10.f), 350.f, 1e-4f);

	const FString PortfolioDir = ViewerCapture::GetPortfolioDirectory();
	TestTrue(TEXT("Portfolio directory ends with Saved/Screenshots/Portfolio"), PortfolioDir.EndsWith(TEXT("Saved/Screenshots/Portfolio")));
	TestEqual(TEXT("Display path is relative to the project"),
		ViewerCapture::MakeDisplayPath(PortfolioDir / TEXT("DA_Character_Full_20261001-140509.png")), FString(TEXT("Saved/Screenshots/Portfolio/DA_Character_Full_20261001-140509.png")));
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerCaptureSequenceTest,
	"CharacterShowcase.Viewer.CaptureSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerCaptureSequenceTest::RunTest(const FString& Parameters)
{
	// Full turntable: 36 frames, 4 settle ticks each -> every frame is
	// prepared exactly once, then requested exactly once, in order.
	{
		FViewerCaptureSequence Seq;
		TestFalse(TEXT("Idle sequence is not active"), Seq.IsActive());
		Seq.Begin(EViewerCaptureMode::Turntable, 36, 4);
		TestTrue(TEXT("Begin makes the sequence active"), Seq.IsActive());
		TestEqual(TEXT("Progress text before frame 1"), Seq.GetProgressText(), FString(TEXT("Capturing 1/36")));

		int32 Prepared = 0;
		int32 Requested = 0;
		int32 Ticks = 0;
		bool bOrderOk = true;
		while (Seq.IsActive() && Ticks < 10000)
		{
			++Ticks;
			const EViewerCaptureStep Step = Seq.Tick();
			if (Step == EViewerCaptureStep::PrepareFrame)
			{
				bOrderOk &= (Seq.GetCurrentFrame() == Prepared);
				++Prepared;
			}
			else if (Step == EViewerCaptureStep::RequestCapture)
			{
				bOrderOk &= (Seq.GetCurrentFrame() == Requested) && (Prepared == Requested + 1);
				++Requested;
				if (Requested == 12)
				{
					TestEqual(TEXT("Progress text while capturing frame 12"), Seq.GetProgressText(), FString(TEXT("Capturing 12/36")));
				}
				Seq.NotifyFrameCaptured(true);
			}
		}
		TestEqual(TEXT("36 frames prepared"), Prepared, 36);
		TestEqual(TEXT("36 frames requested"), Requested, 36);
		TestTrue(TEXT("Frames prepared/requested in order, one request per prepared frame"), bOrderOk);
		TestEqual(TEXT("One prepare + 4 settle + 1 request tick per frame"), Ticks, 36 * 6);
		TestEqual(TEXT("Sequence ends Finished"), (int32)Seq.GetPhase(), (int32)EViewerCapturePhase::Finished);
		TestEqual(TEXT("36 frames completed"), Seq.GetCompletedFrames(), 36);
		TestTrue(TEXT("Progress text is empty when not active"), Seq.GetProgressText().IsEmpty());
	}

	// No request is issued while waiting for the previous file.
	{
		FViewerCaptureSequence Seq;
		Seq.Begin(EViewerCaptureMode::Single, 1, 1);
		TestEqual(TEXT("Single: tick 1 prepares"), (int32)Seq.Tick(), (int32)EViewerCaptureStep::PrepareFrame);
		TestEqual(TEXT("Single: tick 2 settles"), (int32)Seq.Tick(), (int32)EViewerCaptureStep::None);
		TestEqual(TEXT("Single: tick 3 requests"), (int32)Seq.Tick(), (int32)EViewerCaptureStep::RequestCapture);
		TestEqual(TEXT("Single: waiting ticks do nothing"), (int32)Seq.Tick(), (int32)EViewerCaptureStep::None);
		TestEqual(TEXT("Single: still Capturing until notified"), (int32)Seq.GetPhase(), (int32)EViewerCapturePhase::Capturing);
		TestEqual(TEXT("Single progress text"), Seq.GetProgressText(), FString(TEXT("Capturing screenshot...")));
		Seq.NotifyFrameCaptured(true);
		TestEqual(TEXT("Single: Finished after one frame"), (int32)Seq.GetPhase(), (int32)EViewerCapturePhase::Finished);
		Seq.NotifyFrameCaptured(true);
		TestEqual(TEXT("Notify outside Capturing is ignored"), Seq.GetCompletedFrames(), 1);
	}

	// Cancel mid-sequence (Esc) and failure.
	{
		FViewerCaptureSequence Seq;
		Seq.Begin(EViewerCaptureMode::Turntable, 36, 0);
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			Seq.Tick(); // prepare
			Seq.Tick(); // request (0 settle frames)
			Seq.NotifyFrameCaptured(true);
		}
		TestEqual(TEXT("3 frames done before cancel"), Seq.GetCompletedFrames(), 3);
		Seq.Cancel();
		TestEqual(TEXT("Cancel -> Cancelled"), (int32)Seq.GetPhase(), (int32)EViewerCapturePhase::Cancelled);
		TestFalse(TEXT("Cancelled sequence is not active"), Seq.IsActive());
		TestEqual(TEXT("Cancelled sequence asks for nothing"), (int32)Seq.Tick(), (int32)EViewerCaptureStep::None);

		Seq.Begin(EViewerCaptureMode::Turntable, 36, 0);
		Seq.Tick();
		Seq.Tick();
		Seq.NotifyFrameCaptured(false);
		TestEqual(TEXT("A failed frame ends the sequence as Failed"), (int32)Seq.GetPhase(), (int32)EViewerCapturePhase::Failed);
		TestFalse(TEXT("Failed sequence is not active"), Seq.IsActive());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
