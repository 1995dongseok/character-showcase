// Batch portfolio capture: the ACharacterViewerController side
// (Docs/CHARACTER_VIEWER_SETUP.md sections 1.8 and 6.19).
//
// Kept out of CharacterViewerController.cpp on purpose: the controller only
// owns an FViewerBatchCaptureRunner, ticks it from PlayerTick() and stops it
// from CancelCapture()/EndPlay(). Everything else -- the host that applies a
// shot's selection and reuses the existing UI-less single-shot capture
// (BeginCapture), restoring the previous selection, quitting, and the
// Viewer.CaptureAll console command -- is here.

#include "CharacterViewer/CharacterViewerController.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerGameMode.h"
#include "ContentStreaming.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/DateTime.h"
#include "ShaderCompiler.h"
#include "UI/CharacterViewerWidget.h"
#include "UnrealClient.h"

// Friend of ACharacterViewerController (declared in its header).
class FViewerBatchControllerHost : public IViewerBatchCaptureHost
{
public:
	explicit FViewerBatchControllerHost(ACharacterViewerController* InController)
		: Controller(InController)
	{
		SaveSelection();
	}

	virtual bool ShowBatchProfile(UCharacterProfileData* Profile) override
	{
		ACharacterViewerController* C = Controller.Get();
		if (!C || !IsValid(C->ViewerActor) || !Profile)
		{
			return false;
		}
		if (C->ViewerActor->Profile != Profile)
		{
			// The runner reports IsSwitchingProfile() during this call, so the
			// CancelCapture() inside SwitchProfile() does not stop the batch.
			C->SwitchProfile(Profile);
		}
		// Every shot must see the actor at the same yaw.
		if (C->ViewerActor->IsTurntableEnabled())
		{
			C->ViewerActor->SetTurntableEnabled(false);
			if (C->ViewerWidget)
			{
				C->ViewerWidget->NotifySelectionChanged();
			}
		}
		return C->ViewerActor->Profile == Profile;
	}

	virtual bool IsBatchSceneReady() override
	{
		// An uncooked -game run compiles materials on first use; until then they
		// render with the default (grey checker) material.
		const int32 Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
		if (Remaining <= 0)
		{
			return true;
		}
		if (ACharacterViewerController* C = Controller.Get())
		{
			C->SetCaptureStatus(FString::Printf(TEXT("Batch: waiting for shaders (%d left)"), Remaining), false);
		}
		return false;
	}

	virtual void ApplyBatchShot(const FViewerBatchShot& Shot) override
	{
		ACharacterViewerController* C = Controller.Get();
		if (!C)
		{
			return;
		}
		if (Shot.AnimationId != NAME_None)
		{
			C->SelectAnimation(Shot.AnimationId);
		}
		if (Shot.ExpressionId != NAME_None)
		{
			C->SelectExpression(Shot.ExpressionId);
		}
		C->SelectMaterialVariant(Shot.VariantId);
		C->SelectCameraPreset(Shot.PresetId);
		if (C->ViewerWidget)
		{
			C->ViewerWidget->SetCurrentCameraPresetId(Shot.PresetId);
			C->ViewerWidget->NotifySelectionChanged();
		}
	}

	virtual bool IsBatchCameraSettled() const override
	{
		const ACharacterViewerController* C = Controller.Get();
		return !C || !C->CameraPawn || !C->CameraPawn->IsInterpolating();
	}

	virtual bool StartBatchShot(const FString& FilePath) override
	{
		ACharacterViewerController* C = Controller.Get();
		if (!C || C->IsCapturing())
		{
			return false;
		}
		// Same idea as the engine's screenshot automation: make the textures
		// of what is on screen resident before the shot (returns at once when
		// nothing is pending).
		IStreamingManager::Get().StreamAllResources(2.0f);
		return C->BeginCapture(EViewerCaptureMode::Single, FilePath, 1);
	}

	virtual bool IsBatchShotRunning() const override
	{
		const ACharacterViewerController* C = Controller.Get();
		return C && C->IsCapturing();
	}

	virtual bool WasBatchShotWritten(const FString& FilePath) const override
	{
		const ACharacterViewerController* C = Controller.Get();
		return C && C->GetLastCaptureOutputPath() == FilePath && C->GetLastCaptureSavedFrameCount() >= 1
			&& IFileManager::Get().FileExists(*FilePath);
	}

	virtual void CancelBatchShot() override
	{
		if (ACharacterViewerController* C = Controller.Get())
		{
			C->CancelCapture();
		}
	}

	virtual void SetBatchStatus(const FString& Status, bool bFinal) override
	{
		if (ACharacterViewerController* C = Controller.Get())
		{
			C->SetCaptureStatus(Status, bFinal);
		}
	}

	virtual void OnBatchFinished(const FViewerBatchCaptureRunner& Runner) override
	{
		if (Runner.ShouldRestoreSelection())
		{
			RestoreSelection();
		}
		if (Runner.GetOptions().bQuitWhenDone)
		{
			UE_LOG(LogTemp, Log, TEXT("[CharacterViewerBatch] quit=1: requesting exit (%d file(s) written)."), Runner.GetWrittenShots());
			FPlatformMisc::RequestExit(false, TEXT("Viewer.CaptureAll"));
		}
	}

private:
	void SaveSelection()
	{
		const ACharacterViewerController* C = Controller.Get();
		const APortfolioCharacterActor* Actor = C ? C->ViewerActor.Get() : nullptr;
		if (!Actor)
		{
			return;
		}
		SavedProfile = Actor->Profile;
		SavedPresetId = C->ViewerWidget ? C->ViewerWidget->GetCurrentCameraPresetId() : NAME_None;
		SavedVariantId = Actor->GetCurrentVariantId();
		SavedExpressionId = Actor->GetCurrentExpressionId();
		SavedAnimationId = Actor->GetCurrentAnimationId();
		SavedRotation = Actor->GetActorRotation();
		bSavedTurntable = Actor->IsTurntableEnabled();
	}

	void RestoreSelection()
	{
		ACharacterViewerController* C = Controller.Get();
		if (!C || !IsValid(C->ViewerActor))
		{
			return;
		}

		UCharacterProfileData* Profile = SavedProfile.Get();
		if (Profile && C->ViewerActor->Profile != Profile)
		{
			C->SwitchProfile(Profile);
		}
		APortfolioCharacterActor* Actor = C->ViewerActor;
		if (!IsValid(Actor))
		{
			return;
		}

		if (Actor->GetCurrentAnimationId() != SavedAnimationId)
		{
			C->SelectAnimation(SavedAnimationId);
		}
		if (Actor->GetCurrentExpressionId() != SavedExpressionId)
		{
			C->SelectExpression(SavedExpressionId);
		}
		if (Actor->GetCurrentVariantId() != SavedVariantId)
		{
			C->SelectMaterialVariant(SavedVariantId);
		}

		// The camera returns to the preset that was selected (a manual orbit
		// before the batch is not kept: the pawn only interpolates to framings).
		if (SavedPresetId != NAME_None)
		{
			C->SelectCameraPreset(SavedPresetId);
			if (C->ViewerWidget)
			{
				C->ViewerWidget->SetCurrentCameraPresetId(SavedPresetId);
			}
		}
		else
		{
			C->ResetCamera();
		}

		Actor->SetActorRotation(SavedRotation);
		if (bSavedTurntable)
		{
			C->SetTurntableEnabled(true);
		}
		if (C->ViewerWidget)
		{
			C->ViewerWidget->NotifySelectionChanged();
		}
	}

	TWeakObjectPtr<ACharacterViewerController> Controller;
	TWeakObjectPtr<UCharacterProfileData> SavedProfile;
	FName SavedPresetId = NAME_None;
	FName SavedVariantId = NAME_None;
	FName SavedExpressionId = NAME_None;
	FName SavedAnimationId = NAME_None;
	FRotator SavedRotation = FRotator::ZeroRotator;
	bool bSavedTurntable = false;
};

bool ACharacterViewerController::StartBatchCapture(const FViewerBatchOptions& Options)
{
	if (BatchRunner.IsActive() || IsCapturing())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] A capture is already running; batch not started."));
		return false;
	}
	const UGameViewportClient* ViewportClient = GetCaptureViewportClient();
	if (!IsValid(ViewerActor) || !ViewportClient || !ViewportClient->Viewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] No viewer actor or game viewport; batch not started."));
		return false;
	}

	const UWorld* World = GetWorld();
	const ACharacterViewerGameMode* GameMode = World ? World->GetAuthGameMode<ACharacterViewerGameMode>() : nullptr;
	const TArray<UCharacterProfileData*> Library = GameMode ? GameMode->GetProfileLibrary() : TArray<UCharacterProfileData*>();

	TArray<UCharacterProfileData*> Profiles;
	if (Options.bAllProfiles)
	{
		Profiles = Library;
		if (Profiles.Num() == 0 && ViewerActor->Profile)
		{
			Profiles.Add(ViewerActor->Profile);
		}
	}
	else if (Options.ProfileAssetName != NAME_None)
	{
		if (ViewerActor->Profile && ViewerActor->Profile->GetFName() == Options.ProfileAssetName)
		{
			Profiles.Add(ViewerActor->Profile);
		}
		for (UCharacterProfileData* LibraryProfile : Library)
		{
			if (Profiles.Num() == 0 && LibraryProfile && LibraryProfile->GetFName() == Options.ProfileAssetName)
			{
				Profiles.Add(LibraryProfile);
			}
		}
		if (Profiles.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] profile=%s is neither shown nor in the GameMode's ProfileLibrary; batch not started."),
				*Options.ProfileAssetName.ToString());
			return false;
		}
	}
	else if (ViewerActor->Profile)
	{
		Profiles.Add(ViewerActor->Profile);
	}

	ReleaseDrag();
	const TSharedRef<IViewerBatchCaptureHost> Host = MakeShared<FViewerBatchControllerHost>(this);
	if (!BatchRunner.Start(Host, Profiles, Options, ViewerCapture::GetPortfolioDirectory(), FDateTime::Now(), CaptureSettleFrames))
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] Nothing to shoot (%d profile(s), none with a Skeletal Mesh); batch not started."), Profiles.Num());
		return false;
	}
	return true;
}

void ACharacterViewerController::StopBatchCapture(bool bRestoreSelection)
{
	BatchRunner.Cancel(bRestoreSelection);
}

// --- Viewer.CaptureAll console command ---

namespace CharacterViewerBatchConsole
{
	static ACharacterViewerController* FindViewerController(UWorld* World)
	{
		if (World)
		{
			if (ACharacterViewerController* Found = Cast<ACharacterViewerController>(World->GetFirstPlayerController()))
			{
				return Found;
			}
		}
		if (GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* ContextWorld = Context.World();
				if (ContextWorld && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
				{
					if (ACharacterViewerController* Found = Cast<ACharacterViewerController>(ContextWorld->GetFirstPlayerController()))
					{
						return Found;
					}
				}
			}
		}
		return nullptr;
	}

	static void HandleCaptureAll(const TArray<FString>& Args, UWorld* World)
	{
		TArray<FString> Errors;
		const FViewerBatchOptions Options = ViewerBatchCapture::ParseConsoleArgs(Args, Errors);
		for (const FString& Error : Errors)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] Viewer.CaptureAll: ignored argument %s (use profile=<AssetName>|all expressions=0|1 poses=0|1 quit=0|1)."), *Error);
		}

		ACharacterViewerController* Controller = FindViewerController(World);
		const bool bStarted = Controller && Controller->StartBatchCapture(Options);
		if (bStarted)
		{
			UE_LOG(LogTemp, Display, TEXT("[CharacterViewerBatch] Viewer.CaptureAll started: %d shot(s)."), Controller->GetBatchRunner().GetTotalShots());
			return;
		}

		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] Viewer.CaptureAll did not start (%s)."),
			Controller ? TEXT("see the lines above") : TEXT("no ACharacterViewerController: run it in LV_Portfolio with -game or PIE"));
		if (Options.bQuitWhenDone)
		{
			// Unattended run (Tools\CaptureAll.bat): never leave the window open waiting.
			FPlatformMisc::RequestExit(false, TEXT("Viewer.CaptureAll"));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CaptureAllCommand(
		TEXT("Viewer.CaptureAll"),
		TEXT("Portfolio batch capture: every camera preset x material variant as UI-less screenshots in ")
		TEXT("Saved/Screenshots/Portfolio/Batch_<Profile>_<timestamp>/. ")
		TEXT("Args: profile=<AssetName>|all (default: the profile on screen), expressions=0|1, poses=0|1, quit=0|1. Esc cancels."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleCaptureAll));
}
