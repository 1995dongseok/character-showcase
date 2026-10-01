#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UCharacterProfileData;

// Batch portfolio capture (Viewer.CaptureAll / Tools\CaptureAll.bat,
// Docs/CHARACTER_VIEWER_SETUP.md sections 1.8 and 6.19).
//
// Rendering-free on purpose, like ViewerCapture.h: the plan builder is a pure
// function of the profile data, and FViewerBatchCaptureRunner only talks to
// an IViewerBatchCaptureHost, so the whole state machine can be unit-tested in
// the Editor (NullRHI) with a fake host. The real host (selection, camera,
// single-shot capture through ACharacterViewerController) lives in
// CharacterViewerControllerBatch.cpp.

// What one batch run should contain.
struct CHARACTERSHOWCASE_API FViewerBatchOptions
{
	// Profile to shoot (its asset name, e.g. DA_Character_Manny, looked up in
	// the GameMode's ProfileLibrary). NAME_None = the profile currently shown.
	FName ProfileAssetName = NAME_None;

	// Every profile of the GameMode's ProfileLibrary, in library order
	// (ProfileAssetName is ignored).
	bool bAllProfiles = false;

	// Multiply the shots by the profile's Expressions when it has more than
	// one (the neutral one alone adds nothing). Off: the expression on screen is kept.
	bool bIncludeExpressions = false;

	// One extra shot per pose entry (Animations with bIsPose), taken with the
	// profile's default camera preset and first material variant.
	bool bIncludePoses = false;

	// FPlatformMisc::RequestExit(false) after the last file is written (or the
	// batch ends any other way), so Tools\CaptureAll.bat can run unattended.
	bool bQuitWhenDone = false;

	// Seconds to wait after showing a profile (also before the very first
	// shot) so the mesh, animation and texture streaming settle.
	float ProfileSettleSeconds = 1.0f;

	// Upper bound for waiting on the camera preset interpolation; the shot is
	// taken anyway afterwards (logged).
	float CameraSettleTimeoutSeconds = 5.0f;

	// Upper bound (after ProfileSettleSeconds) for waiting until the host
	// reports the scene ready (shaders compiled on an uncooked first run); the
	// shots are taken anyway afterwards (logged).
	float SceneReadyTimeoutSeconds = 300.0f;
};

// One planned image.
struct CHARACTERSHOWCASE_API FViewerBatchShot
{
	// Camera preset id. NAME_None = the profile has no presets (DefaultFraming).
	FName PresetId = NAME_None;

	// Material variant id. NAME_None = the profile has no variants (default materials).
	FName VariantId = NAME_None;

	// Expression to apply first. NAME_None = keep the expression on screen.
	FName ExpressionId = NAME_None;

	// Animation/pose to apply first. NAME_None = keep the animation on screen.
	FName AnimationId = NAME_None;

	// "<Profile>_<Preset>_<Variant>[_<Expression>|_<Pose>].png", no folder.
	FString FileName;

	bool operator==(const FViewerBatchShot& Other) const
	{
		return PresetId == Other.PresetId && VariantId == Other.VariantId && ExpressionId == Other.ExpressionId
			&& AnimationId == Other.AnimationId && FileName == Other.FileName;
	}
};

namespace ViewerBatchCapture
{
	// Deterministic plan for one profile, in this order:
	//   for each camera preset (profile order; one "Default" entry if there are none)
	//     for each material variant (profile order; one "Default" entry if there are none)
	//       for each expression (only with bIncludeExpressions and > 1 expression)
	//   then, with bIncludePoses, one shot per pose entry (Animations with bIsPose)
	//   using DefaultPresetId (else the first preset) and the first variant.
	// File names: "<Profile>_<Preset>_<Variant>.png", "+_<Expression>" when
	// expressions are multiplied in, "+_<PoseId>" for pose shots; every token
	// goes through ViewerCapture::SanitizeToken and a name that would repeat
	// gets "_2", "_3"... Null profile or a profile without SkeletalMesh -> 0
	// shots (nothing to photograph).
	CHARACTERSHOWCASE_API TArray<FViewerBatchShot> BuildBatchPlan(const UCharacterProfileData* Profile, const FViewerBatchOptions& Options);

	// "Batch_<Profile>_<yyyyMMdd-HHmmss>"
	CHARACTERSHOWCASE_API FString MakeBatchFolderName(const FString& ProfileName, const FDateTime& Time);

	// Parses the Viewer.CaptureAll arguments ("profile=<AssetName>|all",
	// "expressions=0|1", "poses=0|1", "quit=0|1"; case-insensitive, any
	// order). Unknown/invalid tokens are appended to OutErrors and ignored.
	CHARACTERSHOWCASE_API FViewerBatchOptions ParseConsoleArgs(const TArray<FString>& Args, TArray<FString>& OutErrors);
}

class FViewerBatchCaptureRunner;

// What the runner needs from the viewer. ACharacterViewerController's host
// (CharacterViewerControllerBatch.cpp) is the real one; tests use a fake.
class CHARACTERSHOWCASE_API IViewerBatchCaptureHost
{
public:
	virtual ~IViewerBatchCaptureHost() = default;

	// Shows Profile on the viewer actor (switching only if it is not already
	// shown). FViewerBatchCaptureRunner::IsSwitchingProfile() is true during
	// this call. false = could not be shown; that profile's shots are skipped as failed.
	virtual bool ShowBatchProfile(UCharacterProfileData* Profile) = 0;

	// true once the shown profile can be photographed (no shader compilation
	// pending). Polled after ProfileSettleSeconds, up to SceneReadyTimeoutSeconds.
	virtual bool IsBatchSceneReady() = 0;

	// Selects the shot's preset/variant (and expression/animation when set).
	virtual void ApplyBatchShot(const FViewerBatchShot& Shot) = 0;

	// true once the camera preset interpolation has finished.
	virtual bool IsBatchCameraSettled() const = 0;

	// Starts one UI-less single-shot capture into FilePath (absolute). false = not started.
	virtual bool StartBatchShot(const FString& FilePath) = 0;

	// true while the shot started by StartBatchShot() is still being written.
	virtual bool IsBatchShotRunning() const = 0;

	// After IsBatchShotRunning() turned false: whether FilePath was written.
	virtual bool WasBatchShotWritten(const FString& FilePath) const = 0;

	// Stops the running single shot (the batch was cancelled mid-shot).
	virtual void CancelBatchShot() = 0;

	// Panel status line. bFinal = the batch's last message (shown for a few seconds).
	virtual void SetBatchStatus(const FString& Status, bool bFinal) = 0;

	// Called exactly once when the batch ends (Finished, Cancelled or
	// Failed), after the runner is no longer active: restore the previous
	// selection, and quit when Runner.GetOptions().bQuitWhenDone.
	virtual void OnBatchFinished(const FViewerBatchCaptureRunner& Runner) = 0;
};

enum class EViewerBatchPhase : uint8
{
	Idle,
	// Next tick: show the current job's profile.
	ShowingProfile,
	// Waiting ProfileSettleSeconds after showing the profile, then until the host reports the scene ready.
	ProfileSettling,
	// Next tick: apply the current shot's selection.
	ApplyingShot,
	// Waiting SettleFrames ticks, then until the camera is settled.
	ShotSettling,
	// Waiting for the single shot to be written.
	Capturing,
	Finished,
	Cancelled,
	// Ended without any file written although shots were planned.
	Failed,
};

// Drives a batch one shot at a time from ACharacterViewerController::PlayerTick
// (Tick once per engine tick). All plans are built in Start(), so the total
// shot count ("Batch 3/12") is known up front.
class CHARACTERSHOWCASE_API FViewerBatchCaptureRunner
{
public:
	struct FJob
	{
		TWeakObjectPtr<UCharacterProfileData> Profile;
		FString ProfileName;
		FString Folder;	// absolute: <RootFolder>/Batch_<Profile>_<timestamp>
		TArray<FViewerBatchShot> Shots;
	};

	// Builds one job per profile (profiles with an empty plan, nulls and
	// duplicates are skipped) and starts at the first one. Returns false (and
	// stays idle, the host is not called) if a batch is already running, or
	// there is nothing to shoot. SettleFrames < 0 is treated as 0.
	bool Start(const TSharedRef<IViewerBatchCaptureHost>& InHost, const TArray<UCharacterProfileData*>& Profiles,
		const FViewerBatchOptions& InOptions, const FString& RootFolder, const FDateTime& Time, int32 InSettleFrames);

	void Tick(float DeltaSeconds);

	// Ends an active batch as Cancelled (no-op when idle): stops a running
	// shot (host CancelBatchShot), then OnBatchFinished. Files already written
	// are kept. bRestoreSelection=false tells the host not to restore the
	// previous selection (EndPlay: the world is going away).
	void Cancel(bool bRestoreSelection = true);

	// false only after Cancel(false); read by the host in OnBatchFinished().
	bool ShouldRestoreSelection() const { return bRestoreSelectionOnFinish; }

	bool IsActive() const;
	bool IsSwitchingProfile() const { return bSwitchingProfile; }
	EViewerBatchPhase GetPhase() const { return Phase; }
	const FViewerBatchOptions& GetOptions() const { return Options; }
	const TArray<FJob>& GetJobs() const { return Jobs; }
	int32 GetCurrentJobIndex() const { return JobIndex; }
	int32 GetCurrentShotIndex() const { return ShotIndex; }

	int32 GetTotalShots() const { return TotalShots; }
	// Shots that are done (written or failed).
	int32 GetProcessedShots() const { return WrittenShots + FailedShots; }
	int32 GetWrittenShots() const { return WrittenShots; }
	int32 GetFailedShots() const { return FailedShots; }
	const TArray<FString>& GetWrittenFiles() const { return WrittenFiles; }
	TArray<FString> GetOutputFolders() const;

	// "Batch 3/12" (1-based shot being worked on) while active, empty otherwise.
	FString GetProgressText() const;

	// Final one-line summary, e.g. "Batch saved 6/6 -> Saved/Screenshots/Portfolio/Batch_DA_Character_Manny_20261001-120000".
	FString GetSummaryText() const;

private:
	const FViewerBatchShot* GetCurrentShot() const;
	FString GetCurrentShotPath() const;
	void AdvanceShot(bool bWritten);
	void AdvanceJob();
	void Finish(EViewerBatchPhase EndPhase);

	TSharedPtr<IViewerBatchCaptureHost> Host;
	FViewerBatchOptions Options;
	TArray<FJob> Jobs;
	EViewerBatchPhase Phase = EViewerBatchPhase::Idle;
	int32 JobIndex = 0;
	int32 ShotIndex = 0;
	int32 SettleFrames = 0;
	int32 SettleRemaining = 0;
	float PhaseElapsedSeconds = 0.f;
	int32 TotalShots = 0;
	int32 WrittenShots = 0;
	int32 FailedShots = 0;
	TArray<FString> WrittenFiles;
	bool bSwitchingProfile = false;
	bool bRestoreSelectionOnFinish = true;
};
