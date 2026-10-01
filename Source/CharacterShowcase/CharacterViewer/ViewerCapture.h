#pragma once

#include "CoreMinimal.h"

// Portfolio capture (F12 / Shift+F12, Docs/CHARACTER_VIEWER_SETUP.md section 1.7):
// pure, rendering-free helpers so file naming, turntable frame math and the
// capture state machine can be unit-tested in the Editor (NullRHI) without a
// game window. ACharacterViewerController owns one FViewerCaptureSequence and
// performs the actual rotation / screenshot requests the sequence asks for.

namespace ViewerCapture
{
	// "yyyyMMdd-HHmmss", e.g. 20261001-142530.
	CHARACTERSHOWCASE_API FString FormatTimestamp(const FDateTime& Time);

	// Makes In safe as one file-name token (no path separators/reserved
	// characters, spaces -> '_'). Returns Fallback if the result is empty.
	CHARACTERSHOWCASE_API FString SanitizeToken(const FString& In, const TCHAR* Fallback);

	// "<Profile>_<PresetId>_<yyyyMMdd-HHmmss>.png"
	CHARACTERSHOWCASE_API FString MakeScreenshotFileName(const FString& ProfileName, const FString& PresetId, const FDateTime& Time);

	// "Turntable_<Profile>_<yyyyMMdd-HHmmss>"
	CHARACTERSHOWCASE_API FString MakeTurntableFolderName(const FString& ProfileName, const FDateTime& Time);

	// "frame_000.png" ... (3-digit, zero-padded; ffmpeg pattern frame_%03d.png).
	CHARACTERSHOWCASE_API FString MakeTurntableFrameFileName(int32 FrameIndex);

	// Number of frames for one full 360 degree turn at StepDegrees per frame
	// (10 -> 36). Non-positive/NaN steps fall back to 10 degrees. At least 1.
	CHARACTERSHOWCASE_API int32 GetTurntableFrameCount(float StepDegrees);

	// Yaw offset (degrees, relative to the starting rotation) of FrameIndex.
	CHARACTERSHOWCASE_API float GetTurntableYawOffset(int32 FrameIndex, float StepDegrees);

	// <ProjectSavedDir>/Screenshots/Portfolio, absolute.
	CHARACTERSHOWCASE_API FString GetPortfolioDirectory();

	// FullPath relative to the project directory when it is inside it
	// ("Saved/Screenshots/Portfolio/..."), else FullPath unchanged.
	CHARACTERSHOWCASE_API FString MakeDisplayPath(const FString& FullPath);
}

enum class EViewerCaptureMode : uint8
{
	None,
	Single,
	Turntable,
};

enum class EViewerCapturePhase : uint8
{
	Idle,
	// The current frame was prepared (rotation applied) and the sequence is
	// waiting SettleFrames engine ticks so the renderer (TAA/motion vectors)
	// has a clean frame at the new pose before the capture is requested.
	Settling,
	// A capture was requested; waiting for NotifyFrameCaptured().
	Capturing,
	Finished,
	Cancelled,
	Failed,
};

// What the owner must do this tick, returned by FViewerCaptureSequence::Tick().
enum class EViewerCaptureStep : uint8
{
	None,
	// Apply the pose for GetCurrentFrame() (e.g. rotate the actor).
	PrepareFrame,
	// Request the screenshot for GetCurrentFrame().
	RequestCapture,
};

// One-frame-at-a-time capture state machine. No engine/rendering
// dependencies; the owner calls Tick() once per engine tick and
// NotifyFrameCaptured() when the requested file was written (or failed).
class CHARACTERSHOWCASE_API FViewerCaptureSequence
{
public:
	// Starts a new sequence (any previous one is discarded). NumFrames < 1 is
	// treated as 1, SettleFrames < 0 as 0.
	void Begin(EViewerCaptureMode InMode, int32 InNumFrames, int32 InSettleFrames);

	EViewerCaptureStep Tick();

	// Only meaningful in the Capturing phase (ignored otherwise). Success
	// advances to the next frame (or Finished after the last one); failure
	// ends the sequence as Failed.
	void NotifyFrameCaptured(bool bSuccess);

	// Ends an active sequence as Cancelled (no-op when not active).
	void Cancel();

	bool IsActive() const { return Phase == EViewerCapturePhase::Settling || Phase == EViewerCapturePhase::Capturing; }
	EViewerCaptureMode GetMode() const { return Mode; }
	EViewerCapturePhase GetPhase() const { return Phase; }
	int32 GetCurrentFrame() const { return CurrentFrame; }
	int32 GetNumFrames() const { return NumFrames; }
	int32 GetCompletedFrames() const { return CompletedFrames; }

	// "Capturing 12/36" (1-based current frame) for a turntable sequence,
	// "Capturing screenshot..." for a single shot, empty when not active.
	FString GetProgressText() const;

private:
	EViewerCaptureMode Mode = EViewerCaptureMode::None;
	EViewerCapturePhase Phase = EViewerCapturePhase::Idle;
	int32 NumFrames = 0;
	int32 SettleFrames = 0;
	int32 CurrentFrame = 0;
	int32 CompletedFrames = 0;
	int32 SettleRemaining = 0;
	bool bFramePrepared = false;
};
