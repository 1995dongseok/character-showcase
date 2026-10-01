#include "CharacterViewer/ViewerCapture.h"

#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace ViewerCapture
{
	static constexpr float DefaultTurntableStepDegrees = 10.f;

	static float SanitizeStep(float StepDegrees)
	{
		return (FMath::IsFinite(StepDegrees) && StepDegrees > KINDA_SMALL_NUMBER) ? StepDegrees : DefaultTurntableStepDegrees;
	}

	FString FormatTimestamp(const FDateTime& Time)
	{
		return Time.ToString(TEXT("%Y%m%d-%H%M%S"));
	}

	FString SanitizeToken(const FString& In, const TCHAR* Fallback)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (const TCHAR Ch : In.TrimStartAndEnd())
		{
			const bool bSafe = FChar::IsAlnum(Ch) || Ch == TEXT('-') || Ch == TEXT('_');
			Out.AppendChar(bSafe ? Ch : TEXT('_'));
		}
		return Out.IsEmpty() ? FString(Fallback) : Out;
	}

	FString MakeScreenshotFileName(const FString& ProfileName, const FString& PresetId, const FDateTime& Time)
	{
		return FString::Printf(TEXT("%s_%s_%s.png"),
			*SanitizeToken(ProfileName, TEXT("NoProfile")),
			*SanitizeToken(PresetId, TEXT("Default")),
			*FormatTimestamp(Time));
	}

	FString MakeTurntableFolderName(const FString& ProfileName, const FDateTime& Time)
	{
		return FString::Printf(TEXT("Turntable_%s_%s"), *SanitizeToken(ProfileName, TEXT("NoProfile")), *FormatTimestamp(Time));
	}

	FString MakeTurntableFrameFileName(int32 FrameIndex)
	{
		return FString::Printf(TEXT("frame_%03d.png"), FMath::Max(0, FrameIndex));
	}

	int32 GetTurntableFrameCount(float StepDegrees)
	{
		return FMath::Max(1, FMath::RoundToInt(360.f / SanitizeStep(StepDegrees)));
	}

	float GetTurntableYawOffset(int32 FrameIndex, float StepDegrees)
	{
		return static_cast<float>(FMath::Max(0, FrameIndex)) * SanitizeStep(StepDegrees);
	}

	FString GetPortfolioDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("Portfolio"));
	}

	FString MakeDisplayPath(const FString& FullPath)
	{
		FString Normalized = FPaths::ConvertRelativePathToFull(FullPath);
		FPaths::NormalizeFilename(Normalized);
		FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(ProjectDir);
		ProjectDir += TEXT("/");

		if (Normalized.StartsWith(ProjectDir, ESearchCase::IgnoreCase))
		{
			return Normalized.RightChop(ProjectDir.Len());
		}
		return Normalized;
	}
}

void FViewerCaptureSequence::Begin(EViewerCaptureMode InMode, int32 InNumFrames, int32 InSettleFrames)
{
	Mode = InMode;
	NumFrames = FMath::Max(1, InNumFrames);
	SettleFrames = FMath::Max(0, InSettleFrames);
	CurrentFrame = 0;
	CompletedFrames = 0;
	SettleRemaining = 0;
	bFramePrepared = false;
	Phase = (InMode == EViewerCaptureMode::None) ? EViewerCapturePhase::Idle : EViewerCapturePhase::Settling;
}

EViewerCaptureStep FViewerCaptureSequence::Tick()
{
	if (Phase != EViewerCapturePhase::Settling)
	{
		return EViewerCaptureStep::None;
	}

	if (!bFramePrepared)
	{
		bFramePrepared = true;
		SettleRemaining = SettleFrames;
		return EViewerCaptureStep::PrepareFrame;
	}

	if (SettleRemaining > 0)
	{
		--SettleRemaining;
		return EViewerCaptureStep::None;
	}

	Phase = EViewerCapturePhase::Capturing;
	return EViewerCaptureStep::RequestCapture;
}

void FViewerCaptureSequence::NotifyFrameCaptured(bool bSuccess)
{
	if (Phase != EViewerCapturePhase::Capturing)
	{
		return;
	}

	if (!bSuccess)
	{
		Phase = EViewerCapturePhase::Failed;
		return;
	}

	++CompletedFrames;
	++CurrentFrame;
	if (CurrentFrame >= NumFrames)
	{
		Phase = EViewerCapturePhase::Finished;
		return;
	}

	bFramePrepared = false;
	Phase = EViewerCapturePhase::Settling;
}

void FViewerCaptureSequence::Cancel()
{
	if (IsActive())
	{
		Phase = EViewerCapturePhase::Cancelled;
	}
}

FString FViewerCaptureSequence::GetProgressText() const
{
	if (!IsActive())
	{
		return FString();
	}
	if (Mode == EViewerCaptureMode::Turntable)
	{
		return FString::Printf(TEXT("Capturing %d/%d"), FMath::Min(CurrentFrame + 1, NumFrames), NumFrames);
	}
	return TEXT("Capturing screenshot...");
}
