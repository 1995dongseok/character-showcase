#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "CharacterViewer/ViewerCapture.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UI/CharacterViewerWidget.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

// CharacterShowcase.Game.ViewerCapture (Docs/CHARACTER_VIEWER_SETUP.md
// sections 1.3/1.7/4.1): a short (~30-60 s) -game test for the panel layout
// and portfolio capture. ClientContext only (never runs under the NullRHI
// Editor automation). NO pointer/keyboard synthesis and no cursor movement:
//  1. window capture (with UI) at the current resolution, then Clean View
//     and a second capture -> Saved/Screenshots/ViewerCapture/
//     ViewerCapture_<UI|Clean>_<W>x<H>.png (run it at 1280x720 and 1920x1080);
//     then (pointer-free) Inspection on + first part selected ->
//     ViewerCapture_Inspect_<W>x<H>.png, + Wireframe (shaded overlay) ->
//     ViewerCapture_Wireframe_<W>x<H>.png, then everything off again;
//  2. ACharacterViewerController::TakePortfolioScreenshot() (= F12) must
//     write its file within a few seconds (size = viewport x multiplier on
//     the high-res path); the method used is logged;
//  3. StartTurntableCapture() (= Shift+F12) must write frame_000/001, then
//     CancelCapture() (= Esc) must restore the actor yaw and turntable state.
// The process exits through -TestExit="Automation Test Queue Empty".

namespace CharacterViewerCaptureGameTest
{
	struct FState
	{
		TWeakObjectPtr<ACharacterViewerController> Controller;
		TWeakObjectPtr<UCharacterViewerWidget> Widget;
		TWeakObjectPtr<APortfolioCharacterActor> Actor;
		FIntPoint ViewportSize = FIntPoint::ZeroValue;
		double StepStartSeconds = 0.0;
		FString ScreenshotPath;
		FString TurntableFolder;
		FRotator TurntableStartRotation = FRotator::ZeroRotator;
		bool bTurntableWasEnabled = false;
	};

	static UWorld* FindGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static FString GetOutputDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("ViewerCapture"));
	}

	// Same capture path as CharacterShowcase.Game.ViewerSmoke (FSlateApplication::
	// TakeScreenshot of the game window, UI included), file name with resolution.
	static bool CaptureWindow(FAutomationTestBase* Test, const FString& Label, const FIntPoint& ViewportSize)
	{
		const TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
		if (!Window.IsValid() || !FSlateApplication::IsInitialized())
		{
			Test->AddError(FString::Printf(TEXT("ViewerCapture '%s': no game window / Slate application."), *Label));
			return false;
		}

		TArray<FColor> Bitmap;
		FIntVector Size(0, 0, 0);
		if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Bitmap, Size))
		{
			Test->AddError(FString::Printf(TEXT("ViewerCapture '%s': FSlateApplication::TakeScreenshot failed (window visible=%d minimized=%d)."),
				*Label, Window->IsVisible() ? 1 : 0, Window->IsWindowMinimized() ? 1 : 0));
			return false;
		}
		for (FColor& Pixel : Bitmap)
		{
			Pixel.A = 255;
		}

		const FString Path = GetOutputDirectory() / FString::Printf(TEXT("ViewerCapture_%s_%dx%d.png"), *Label, ViewportSize.X, ViewportSize.Y);
		IFileManager::Get().MakeDirectory(*GetOutputDirectory(), true);
		const bool bSaved = FImageUtils::SaveImageByExtension(*Path, FImageView(Bitmap.GetData(), Size.X, Size.Y));
		if (Test->TestTrue(FString::Printf(TEXT("ViewerCapture '%s' saved"), *Label), bSaved))
		{
			Test->AddInfo(FString::Printf(TEXT("ViewerCapture '%s' (%dx%d, with UI) -> %s"), *Label, Size.X, Size.Y, *Path));
		}
		return bSaved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerCaptureGameTest,
	"CharacterShowcase.Game.ViewerCapture",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharacterViewerCaptureGameTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerCaptureGameTest;

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;

	// 0. Let BeginPlay/PostLogin, the first layout pass and shader warm-up settle.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 1. Scene / widget / layout facts.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		UWorld* World = FindGameWorld();
		if (!Test->TestNotNull(TEXT("A Game world exists"), World))
		{
			return true;
		}
		ACharacterViewerController* Controller = Cast<ACharacterViewerController>(World->GetFirstPlayerController());
		if (!Test->TestNotNull(TEXT("PlayerController is an ACharacterViewerController"), Controller))
		{
			return true;
		}
		State->Controller = Controller;
		State->Actor = Controller->GetViewerActor();
		State->Widget = Controller->GetViewerWidget();
		Test->TestNotNull(TEXT("Viewer actor exists"), State->Actor.Get());

		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			State->ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}
		Test->TestTrue(TEXT("Viewport has a size"), State->ViewportSize.X > 0 && State->ViewportSize.Y > 0);

		UCharacterViewerWidget* Widget = State->Widget.Get();
		if (Test->TestNotNull(TEXT("Viewer widget exists"), Widget))
		{
			Test->TestTrue(TEXT("WBP_CharacterViewer designer layout is in use"), Widget->IsUsingDesignerLayout());
			const float Applied = Widget->GetAppliedPanelWidth();
			Test->TestTrue(FString::Printf(TEXT("Auto panel width applied within [300, 460] Slate units (got %.1f)"), Applied), Applied >= 299.5f && Applied <= 460.5f);
			Test->TestNotNull(TEXT("StatusText is bound (8th, optional name present in the generated WBP)"), Widget->StatusText.Get());
			if (Widget->PanelRoot)
			{
				const FGeometry Geometry = Widget->PanelRoot->GetCachedGeometry();
				Test->AddInfo(FString::Printf(TEXT("Viewport %dx%d: PanelRoot %.0f Slate units wide = %.0f px on screen (absolute x %.0f..%.0f)."),
					State->ViewportSize.X, State->ViewportSize.Y, Geometry.GetLocalSize().X, Geometry.GetAbsoluteSize().X,
					Geometry.GetAbsolutePosition().X, Geometry.GetAbsolutePosition().X + Geometry.GetAbsoluteSize().X));
			}
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));

	// 2. Window captures with UI, then Clean View.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		CaptureWindow(Test, TEXT("UI"), State->ViewportSize);
		if (ACharacterViewerController* Controller = State->Controller.Get())
		{
			Controller->ToggleCleanView();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		CaptureWindow(Test, TEXT("Clean"), State->ViewportSize);
		if (ACharacterViewerController* Controller = State->Controller.Get())
		{
			Controller->ToggleCleanView();
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// 2b. Pointer-free visual checks (Docs/CHARACTER_VIEWER_SETUP.md 6.14):
	// Inspection on + the profile's FIRST part selected programmatically (no
	// click, so no foreground/cursor precondition), then the shaded Wireframe
	// overlay on top of that selection. The panel's lists are scrolled to the
	// end so the INSPECTION text (measured numbers) is in the capture.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		APortfolioCharacterActor* Actor = State->Actor.Get();
		if (!Controller || !Actor || !Actor->Profile || Actor->Profile->Parts.Num() == 0)
		{
			Test->AddError(TEXT("Inspect capture step: no controller/actor/profile parts."));
			return true;
		}
		Controller->SetInspectionEnabled(true);
		const FName PartId = Actor->Profile->Parts[0].Id;
		Actor->SetSelectedPart(PartId);
		Test->TestEqual(TEXT("Inspect capture: first part selected"), Actor->GetSelectedPartId(), PartId);
		Test->TestNotEqual(TEXT("Inspect capture: a highlight mode is active"), Actor->GetActiveHighlightMode(), EViewerHighlightMode::None);
		if (UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			// Selecting a new part with Inspection on already requests a
			// multi-tick scroll-to-end (RefreshUI); request it explicitly too.
			Widget->NotifySelectionChanged();
			Widget->RequestListsScrollToEnd(10);
		}
		Test->AddInfo(FString::Printf(TEXT("Inspect capture: part '%s', highlight %s, visible bone markers %d (%s)."),
			*PartId.ToString(), *UEnum::GetValueAsString(Actor->GetActiveHighlightMode()), Actor->GetVisibleBoneMarkerCount(),
			*FString::JoinBy(Actor->GetBoneMarkerBoneNames(), TEXT(", "), [](const FName& Name) { return Name.ToString(); })));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		if (const UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			if (Widget->ListsScroll)
			{
				// The selected-part block is the last rows of ListsScroll: it is
				// on screen only if the lists are scrolled to their end.
				const float Offset = Widget->ListsScroll->GetScrollOffset();
				const float EndOffset = Widget->ListsScroll->GetScrollOffsetOfEnd();
				const FGeometry ListsGeometry = Widget->ListsScroll->GetCachedGeometry();
				Test->AddInfo(FString::Printf(TEXT("Inspect capture: ListsScroll offset %.1f of end %.1f, visible height %.1f Slate units (%.0f px)."),
					Offset, EndOffset, ListsGeometry.GetLocalSize().Y, ListsGeometry.GetAbsoluteSize().Y));
				Test->TestTrue(FString::Printf(TEXT("Inspect capture: lists scrolled to the end (offset %.1f, end %.1f) so the selected-part block is visible"), Offset, EndOffset),
					EndOffset <= 0.f || FMath::Abs(EndOffset - Offset) <= 1.f);
			}
			Test->AddInfo(FString::Printf(TEXT("Inspect capture: INSPECTION body: %s"), *Widget->GetFallbackInspectionBodyText().ToString()));
		}
		CaptureWindow(Test, TEXT("Inspect"), State->ViewportSize);
		ACharacterViewerController* Controller = State->Controller.Get();
		APortfolioCharacterActor* Actor = State->Actor.Get();
		if (!Controller || !Actor)
		{
			return true;
		}
		Test->TestTrue(TEXT("Wireframe capture: ToggleWireframe() turns Wireframe on"), Controller->ToggleWireframe() && Actor->IsWireframeEnabled());
		Test->AddInfo(FString::Printf(TEXT("Wireframe capture: mode %s, overlay %s, highlight %s, visible bone markers %d."),
			*UEnum::GetValueAsString(Actor->GetActiveWireframeMode()),
			Actor->Mesh ? *GetNameSafe(Actor->Mesh->GetOverlayMaterial()) : TEXT("(no mesh)"),
			*UEnum::GetValueAsString(Actor->GetActiveHighlightMode()), Actor->GetVisibleBoneMarkerCount()));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		CaptureWindow(Test, TEXT("Wireframe"), State->ViewportSize);
		ACharacterViewerController* Controller = State->Controller.Get();
		APortfolioCharacterActor* Actor = State->Actor.Get();
		if (Actor)
		{
			Actor->ClearSelectedPart();
		}
		if (Controller && Actor && Actor->IsWireframeEnabled())
		{
			Controller->ToggleWireframe();
		}
		if (Controller)
		{
			Controller->SetInspectionEnabled(false);
		}
		if (UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			if (Widget->ListsScroll)
			{
				Widget->ListsScroll->ScrollToStart();
			}
		}
		Test->TestFalse(TEXT("Wireframe off again after the capture"), Actor && Actor->IsWireframeEnabled());
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// 3. F12 path: TakePortfolioScreenshot() writes its file.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		if (!Controller)
		{
			Test->AddError(TEXT("F12 step: no controller."));
			return true;
		}
		Test->TestTrue(TEXT("TakePortfolioScreenshot() starts a capture"), Controller->TakePortfolioScreenshot());
		Test->TestTrue(TEXT("Controller reports IsCapturing() after F12"), Controller->IsCapturing());
		Test->TestFalse(TEXT("A second request while capturing is refused"), Controller->TakePortfolioScreenshot());
		State->ScreenshotPath = Controller->GetLastCaptureOutputPath();
		Test->TestTrue(FString::Printf(TEXT("Screenshot path is under Saved/Screenshots/Portfolio (%s)"), *State->ScreenshotPath),
			State->ScreenshotPath.Contains(TEXT("Saved/Screenshots/Portfolio/")) && State->ScreenshotPath.EndsWith(TEXT(".png")));
		State->StepStartSeconds = FPlatformTime::Seconds();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		const double Elapsed = FPlatformTime::Seconds() - State->StepStartSeconds;
		if (Controller && Controller->IsCapturing() && Elapsed < 20.0)
		{
			return false; // keep waiting
		}
		if (!Controller)
		{
			return true;
		}
		Test->TestFalse(FString::Printf(TEXT("F12 capture finished within 20 s (%.1f s)"), Elapsed), Controller->IsCapturing());
		Test->TestEqual(TEXT("F12 capture wrote exactly one file"), Controller->GetLastCaptureSavedFrameCount(), 1);
		const bool bExists = IFileManager::Get().FileExists(*State->ScreenshotPath);
		Test->TestTrue(FString::Printf(TEXT("F12 file exists: %s"), *State->ScreenshotPath), bExists);
		const FString Method = Controller->GetLastCaptureMethod();
		Test->AddInfo(FString::Printf(TEXT("F12 capture method: %s, %.1f s."), *Method, Elapsed));

		FImage Image;
		if (bExists && FImageUtils::LoadImage(*State->ScreenshotPath, Image))
		{
			const int32 Multiplier = (Method == TEXT("HighResScreenshot")) ? Controller->ScreenshotResolutionMultiplier : 1;
			Test->AddInfo(FString::Printf(TEXT("F12 image %dx%d (viewport %dx%d, multiplier applied %d)."), Image.SizeX, Image.SizeY, State->ViewportSize.X, State->ViewportSize.Y, Multiplier));
			if (Method != TEXT("SlateTakeScreenshot"))
			{
				Test->TestEqual(TEXT("F12 image width = viewport width x multiplier"), Image.SizeX, State->ViewportSize.X * Multiplier);
				Test->TestEqual(TEXT("F12 image height = viewport height x multiplier"), Image.SizeY, State->ViewportSize.Y * Multiplier);
			}
		}
		else
		{
			Test->AddError(TEXT("F12 image could not be loaded."));
		}

		if (const UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			Test->TestTrue(FString::Printf(TEXT("Status line shows 'Saved: ...' (got '%s')"), *Widget->GetCaptureStatus().ToString()),
				Widget->GetCaptureStatus().ToString().StartsWith(TEXT("Saved: Saved/Screenshots/Portfolio/")));
		}
		return true;
	}));

	// 4. Shift+F12 path: two frames, then Esc (CancelCapture) restores the actor.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		APortfolioCharacterActor* Actor = State->Actor.Get();
		if (!Controller || !Actor)
		{
			Test->AddError(TEXT("Turntable step: no controller/actor."));
			return true;
		}
		Actor->SetTurntableEnabled(true); // the capture must pause it and restore it afterwards
		State->bTurntableWasEnabled = true;
		State->TurntableStartRotation = Actor->GetActorRotation();
		Test->TestTrue(TEXT("StartTurntableCapture() starts a capture"), Controller->StartTurntableCapture());
		Test->TestFalse(TEXT("Turntable is paused during the capture"), Actor->IsTurntableEnabled());
		State->TurntableStartRotation = Actor->GetActorRotation();
		State->TurntableFolder = Controller->GetLastCaptureOutputPath();
		Test->TestTrue(FString::Printf(TEXT("Turntable folder is Turntable_<Profile>_<timestamp> (%s)"), *State->TurntableFolder),
			FPaths::GetCleanFilename(State->TurntableFolder).StartsWith(TEXT("Turntable_")));
		State->StepStartSeconds = FPlatformTime::Seconds();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		const double Elapsed = FPlatformTime::Seconds() - State->StepStartSeconds;
		if (Controller && Controller->IsCapturing() && Controller->GetLastCaptureSavedFrameCount() < 2 && Elapsed < 40.0)
		{
			if (const UCharacterViewerWidget* Widget = State->Widget.Get())
			{
				// Progress is visible in the panel while frames are written.
				if (!Widget->GetCaptureStatus().ToString().StartsWith(TEXT("Capturing ")))
				{
					Test->AddError(FString::Printf(TEXT("Status line during turntable capture should be 'Capturing n/36', got '%s'."), *Widget->GetCaptureStatus().ToString()));
				}
			}
			return false;
		}
		if (!Controller)
		{
			return true;
		}
		Test->TestTrue(FString::Printf(TEXT("Turntable wrote >= 2 frames within 40 s (%d in %.1f s, method %s)"),
			Controller->GetLastCaptureSavedFrameCount(), Elapsed, *Controller->GetLastCaptureMethod()), Controller->GetLastCaptureSavedFrameCount() >= 2);

		Controller->CancelCapture(); // same as Esc
		Test->TestFalse(TEXT("CancelCapture() stops the sequence"), Controller->IsCapturing());
		Test->TestTrue(TEXT("frame_000.png exists"), IFileManager::Get().FileExists(*(State->TurntableFolder / TEXT("frame_000.png"))));
		Test->TestTrue(TEXT("frame_001.png exists"), IFileManager::Get().FileExists(*(State->TurntableFolder / TEXT("frame_001.png"))));
		if (APortfolioCharacterActor* Actor = State->Actor.Get())
		{
			Test->TestEqual(TEXT("Actor yaw restored after cancel"), (float)FMath::UnwindDegrees(Actor->GetActorRotation().Yaw), (float)FMath::UnwindDegrees(State->TurntableStartRotation.Yaw), 0.1f);
			Test->TestEqual(TEXT("Turntable state restored after cancel"), Actor->IsTurntableEnabled(), State->bTurntableWasEnabled);
			Actor->SetTurntableEnabled(false);
		}
		if (const UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			Test->TestTrue(FString::Printf(TEXT("Status line reports the cancel (got '%s')"), *Widget->GetCaptureStatus().ToString()),
				Widget->GetCaptureStatus().ToString().StartsWith(TEXT("Capture cancelled")));
		}
		return true;
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
