#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "CharacterViewer/ViewerBatchCapture.h"
#include "CharacterViewer/ViewerCapture.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "UI/CharacterViewerWidget.h"
#include "UnrealClient.h"

// CharacterShowcase.Game.ViewerBatchCapture (Docs/CHARACTER_VIEWER_SETUP.md
// sections 1.8 / 6.19): a short -game test of the batch capture
// (Viewer.CaptureAll) for the profile on screen only (DA_Character_Manny: 3
// presets x 2 variants = 6 shots). ClientContext only (never runs under the
// NullRHI Editor automation). No pointer/keyboard input. Checks: the batch
// starts and refuses a second start, finishes within 90 s, writes exactly the
// planned files into one Batch_<Profile>_<timestamp> folder, and restores the
// previous profile/variant/camera preset. The process exits through
// -TestExit="Automation Test Queue Empty".

namespace ViewerBatchCaptureGameTest
{
	struct FState
	{
		TWeakObjectPtr<ACharacterViewerController> Controller;
		TWeakObjectPtr<APortfolioCharacterActor> Actor;
		TWeakObjectPtr<UCharacterProfileData> Profile;
		TArray<FViewerBatchShot> Plan;
		FName PresetBefore = NAME_None;
		FName VariantBefore = NAME_None;
		FName AnimationBefore = NAME_None;
		FIntPoint ViewportSize = FIntPoint::ZeroValue;
		double StartSeconds = 0.0;
		TSet<FString> StatusesSeen;
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewerBatchCaptureGameTest,
	"CharacterShowcase.Game.ViewerBatchCapture",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FViewerBatchCaptureGameTest::RunTest(const FString& Parameters)
{
	using namespace ViewerBatchCaptureGameTest;

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;

	// 0. Let BeginPlay/PostLogin and the first frames settle.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 1. Start the batch for the profile on screen.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		UWorld* World = FindGameWorld();
		ACharacterViewerController* Controller = World ? Cast<ACharacterViewerController>(World->GetFirstPlayerController()) : nullptr;
		if (!Test->TestNotNull(TEXT("PlayerController is an ACharacterViewerController"), Controller))
		{
			return true;
		}
		APortfolioCharacterActor* Actor = Controller->GetViewerActor();
		if (!Test->TestNotNull(TEXT("Viewer actor with a profile"), Actor ? Actor->Profile.Get() : nullptr))
		{
			return true;
		}
		State->Controller = Controller;
		State->Actor = Actor;
		State->Profile = Actor->Profile;
		State->VariantBefore = Actor->GetCurrentVariantId();
		State->AnimationBefore = Actor->GetCurrentAnimationId();
		State->PresetBefore = Controller->GetViewerWidget() ? Controller->GetViewerWidget()->GetCurrentCameraPresetId() : NAME_None;
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
		{
			State->ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
		}

		FViewerBatchOptions Options;	// current profile, no expressions/poses, no quit
		Options.ProfileSettleSeconds = 0.5f;
		State->Plan = ViewerBatchCapture::BuildBatchPlan(Actor->Profile, Options);
		Test->AddInfo(FString::Printf(TEXT("Batch plan for %s: %d shot(s), viewport %dx%d."),
			*Actor->Profile->GetName(), State->Plan.Num(), State->ViewportSize.X, State->ViewportSize.Y));
		Test->TestTrue(TEXT("The profile on screen has a non-empty plan"), State->Plan.Num() > 0);

		Test->TestTrue(TEXT("StartBatchCapture() starts"), Controller->StartBatchCapture(Options));
		Test->TestTrue(TEXT("IsBatchCapturing() after the start"), Controller->IsBatchCapturing());
		Test->TestFalse(TEXT("A second batch while one runs is refused"), Controller->StartBatchCapture(Options));
		Test->TestFalse(TEXT("No single shot runs right after the start (the first one waits for the profile to settle)"), Controller->IsCapturing());
		State->StartSeconds = FPlatformTime::Seconds();
		return true;
	}));

	// 2. Wait (<= 90 s) for the batch to finish.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Test, State]()
	{
		ACharacterViewerController* Controller = State->Controller.Get();
		const double Elapsed = FPlatformTime::Seconds() - State->StartSeconds;
		if (Controller && Controller->IsBatchCapturing() && Elapsed < 90.0)
		{
			if (const UCharacterViewerWidget* Widget = Controller->GetViewerWidget())
			{
				const FString Status = Widget->GetCaptureStatus().ToString();
				if (Status.StartsWith(TEXT("Batch ")))
				{
					State->StatusesSeen.Add(Status);
				}
			}
			return false; // keep waiting
		}
		if (!Controller)
		{
			Test->AddError(TEXT("Controller went away during the batch."));
			return true;
		}

		const FViewerBatchCaptureRunner& Runner = Controller->GetBatchRunner();
		Test->TestFalse(FString::Printf(TEXT("Batch finished within 90 s (%.1f s)"), Elapsed), Controller->IsBatchCapturing());
		if (Controller->IsBatchCapturing())
		{
			Controller->CancelCapture();
		}
		Test->TestEqual(TEXT("Batch ended as Finished"), Runner.GetPhase(), EViewerBatchPhase::Finished);
		Test->TestEqual(TEXT("Every planned shot was written"), Runner.GetWrittenShots(), State->Plan.Num());
		Test->TestEqual(TEXT("No shot failed"), Runner.GetFailedShots(), 0);
		Test->AddInfo(FString::Printf(TEXT("Batch: %d/%d written in %.1f s (%.1f s per shot), last method %s, progress lines seen: %s."),
			Runner.GetWrittenShots(), State->Plan.Num(), Elapsed, State->Plan.Num() > 0 ? Elapsed / State->Plan.Num() : 0.0,
			*Controller->GetLastCaptureMethod(), *FString::Join(State->StatusesSeen.Array(), TEXT(", "))));

		const TArray<FString> Folders = Runner.GetOutputFolders();
		Test->TestEqual(TEXT("One output folder"), Folders.Num(), 1);
		if (Folders.Num() == 1)
		{
			const FString Folder = Folders[0];
			const FString ProfileName = State->Profile.IsValid() ? State->Profile->GetName() : FString();
			Test->TestTrue(FString::Printf(TEXT("Folder is Saved/Screenshots/Portfolio/Batch_%s_<timestamp> (%s)"), *ProfileName, *Folder),
				Folder.Contains(TEXT("Saved/Screenshots/Portfolio/")) && FPaths::GetCleanFilename(Folder).StartsWith(TEXT("Batch_") + ProfileName + TEXT("_")));

			TArray<FString> Pngs;
			IFileManager::Get().FindFiles(Pngs, *(Folder / TEXT("*.png")), true, false);
			Test->TestEqual(TEXT("Folder holds exactly the planned number of .png files"), Pngs.Num(), State->Plan.Num());
			for (const FViewerBatchShot& Shot : State->Plan)
			{
				Test->TestTrue(FString::Printf(TEXT("%s exists"), *Shot.FileName), IFileManager::Get().FileExists(*(Folder / Shot.FileName)));
			}

			FImage Image;
			if (State->Plan.Num() > 0 && FImageUtils::LoadImage(*(Folder / State->Plan[0].FileName), Image))
			{
				Test->AddInfo(FString::Printf(TEXT("First image %s: %dx%d (viewport %dx%d, multiplier %d)."), *State->Plan[0].FileName,
					Image.SizeX, Image.SizeY, State->ViewportSize.X, State->ViewportSize.Y, Controller->ScreenshotResolutionMultiplier));
				Test->TestTrue(TEXT("First image is at least viewport-sized"), Image.SizeX >= State->ViewportSize.X && Image.SizeY >= State->ViewportSize.Y);
			}
			else
			{
				Test->AddError(TEXT("First batch image could not be loaded."));
			}
		}

		// The previous selection is back.
		if (APortfolioCharacterActor* Actor = State->Actor.Get())
		{
			Test->TestTrue(TEXT("Profile restored"), Actor->Profile == State->Profile.Get());
			Test->TestEqual(TEXT("Material variant restored"), Actor->GetCurrentVariantId(), State->VariantBefore);
			Test->TestEqual(TEXT("Animation restored"), Actor->GetCurrentAnimationId(), State->AnimationBefore);
		}
		if (const UCharacterViewerWidget* Widget = Controller->GetViewerWidget())
		{
			const FName ExpectedPreset = State->PresetBefore != NAME_None ? State->PresetBefore
				: (State->Profile.IsValid() ? State->Profile->DefaultPresetId : NAME_None);
			Test->TestEqual(TEXT("Camera preset restored"), Widget->GetCurrentCameraPresetId(), ExpectedPreset);
			Test->TestTrue(FString::Printf(TEXT("Status line shows the summary (got '%s')"), *Widget->GetCaptureStatus().ToString()),
				Widget->GetCaptureStatus().ToString().StartsWith(TEXT("Batch saved ")));
		}
		return true;
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
