#include "CharacterViewer/ViewerBatchCapture.h"

#include "Character/CharacterProfileData.h"
#include "CharacterViewer/ViewerCapture.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace ViewerBatchCapture
{
	static FString Token(FName Id)
	{
		return ViewerCapture::SanitizeToken(Id == NAME_None ? FString() : Id.ToString(), TEXT("Default"));
	}

	TArray<FViewerBatchShot> BuildBatchPlan(const UCharacterProfileData* Profile, const FViewerBatchOptions& Options)
	{
		TArray<FViewerBatchShot> Plan;
		if (!Profile || !Profile->SkeletalMesh)
		{
			return Plan;
		}

		const FString ProfileToken = ViewerCapture::SanitizeToken(Profile->GetName(), TEXT("NoProfile"));

		TArray<FName> Presets;
		for (const FViewerCameraPreset& Preset : Profile->CameraPresets)
		{
			Presets.Add(Preset.Id);
		}
		if (Presets.Num() == 0)
		{
			Presets.Add(NAME_None);
		}

		TArray<FName> Variants;
		for (const FViewerMaterialVariant& Variant : Profile->MaterialVariants)
		{
			Variants.Add(Variant.Id);
		}
		if (Variants.Num() == 0)
		{
			Variants.Add(NAME_None);
		}

		// The neutral expression alone adds nothing: only multiply when there is a real choice.
		TArray<FName> Expressions;
		const bool bMultiplyExpressions = Options.bIncludeExpressions && Profile->Expressions.Num() > 1;
		if (bMultiplyExpressions)
		{
			for (const FViewerExpression& Expression : Profile->Expressions)
			{
				Expressions.Add(Expression.Id);
			}
		}
		else
		{
			Expressions.Add(NAME_None);
		}

		TSet<FString> UsedNames;
		auto AddShot = [&Plan, &UsedNames](FViewerBatchShot&& Shot, const FString& BaseName)
		{
			FString Name = BaseName + TEXT(".png");
			for (int32 Suffix = 2; UsedNames.Contains(Name); ++Suffix)
			{
				Name = FString::Printf(TEXT("%s_%d.png"), *BaseName, Suffix);
			}
			UsedNames.Add(Name);
			Shot.FileName = Name;
			Plan.Add(MoveTemp(Shot));
		};

		for (const FName PresetId : Presets)
		{
			for (const FName VariantId : Variants)
			{
				for (const FName ExpressionId : Expressions)
				{
					FViewerBatchShot Shot;
					Shot.PresetId = PresetId;
					Shot.VariantId = VariantId;
					Shot.ExpressionId = ExpressionId;
					FString BaseName = FString::Printf(TEXT("%s_%s_%s"), *ProfileToken, *Token(PresetId), *Token(VariantId));
					if (bMultiplyExpressions)
					{
						BaseName += TEXT("_") + Token(ExpressionId);
					}
					AddShot(MoveTemp(Shot), BaseName);
				}
			}
		}

		if (Options.bIncludePoses)
		{
			const FName PosePreset = Profile->FindPreset(Profile->DefaultPresetId) ? Profile->DefaultPresetId : Presets[0];
			for (const FViewerAnimationEntry& Entry : Profile->Animations)
			{
				if (!Entry.bIsPose || Entry.Id == NAME_None)
				{
					continue;
				}
				FViewerBatchShot Shot;
				Shot.PresetId = PosePreset;
				Shot.VariantId = Variants[0];
				// Back to the first (normally neutral) expression after the expression shots.
				Shot.ExpressionId = Expressions[0];
				Shot.AnimationId = Entry.Id;
				AddShot(MoveTemp(Shot), FString::Printf(TEXT("%s_%s_%s_%s"), *ProfileToken, *Token(PosePreset), *Token(Variants[0]), *Token(Entry.Id)));
			}
		}

		return Plan;
	}

	FString MakeBatchFolderName(const FString& ProfileName, const FDateTime& Time)
	{
		return FString::Printf(TEXT("Batch_%s_%s"), *ViewerCapture::SanitizeToken(ProfileName, TEXT("NoProfile")), *ViewerCapture::FormatTimestamp(Time));
	}

	static bool ParseBool(const FString& Value, bool& OutValue)
	{
		if (Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("on"), ESearchCase::IgnoreCase))
		{
			OutValue = true;
			return true;
		}
		if (Value == TEXT("0") || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("off"), ESearchCase::IgnoreCase))
		{
			OutValue = false;
			return true;
		}
		return false;
	}

	FViewerBatchOptions ParseConsoleArgs(const TArray<FString>& Args, TArray<FString>& OutErrors)
	{
		FViewerBatchOptions Options;
		for (const FString& RawArg : Args)
		{
			const FString Arg = RawArg.TrimStartAndEnd();
			if (Arg.IsEmpty())
			{
				continue;
			}

			FString Key;
			FString Value;
			if (!Arg.Split(TEXT("="), &Key, &Value))
			{
				OutErrors.Add(FString::Printf(TEXT("'%s' (expected key=value)"), *Arg));
				continue;
			}
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			bool bFlag = false;
			if (Key.Equals(TEXT("profile"), ESearchCase::IgnoreCase))
			{
				if (Value.Equals(TEXT("all"), ESearchCase::IgnoreCase))
				{
					Options.bAllProfiles = true;
					Options.ProfileAssetName = NAME_None;
				}
				else if (Value.IsEmpty() || Value.Equals(TEXT("current"), ESearchCase::IgnoreCase))
				{
					Options.bAllProfiles = false;
					Options.ProfileAssetName = NAME_None;
				}
				else
				{
					Options.bAllProfiles = false;
					Options.ProfileAssetName = FName(*Value);
				}
			}
			else if (Key.Equals(TEXT("expressions"), ESearchCase::IgnoreCase) && ParseBool(Value, bFlag))
			{
				Options.bIncludeExpressions = bFlag;
			}
			else if (Key.Equals(TEXT("poses"), ESearchCase::IgnoreCase) && ParseBool(Value, bFlag))
			{
				Options.bIncludePoses = bFlag;
			}
			else if (Key.Equals(TEXT("quit"), ESearchCase::IgnoreCase) && ParseBool(Value, bFlag))
			{
				Options.bQuitWhenDone = bFlag;
			}
			else
			{
				OutErrors.Add(FString::Printf(TEXT("'%s'"), *Arg));
			}
		}
		return Options;
	}
}

bool FViewerBatchCaptureRunner::Start(const TSharedRef<IViewerBatchCaptureHost>& InHost, const TArray<UCharacterProfileData*>& Profiles,
	const FViewerBatchOptions& InOptions, const FString& RootFolder, const FDateTime& Time, int32 InSettleFrames)
{
	if (IsActive())
	{
		return false;
	}

	TArray<FJob> NewJobs;
	TSet<const UCharacterProfileData*> Seen;
	int32 NewTotal = 0;
	for (UCharacterProfileData* Profile : Profiles)
	{
		if (!Profile || Seen.Contains(Profile))
		{
			continue;
		}
		Seen.Add(Profile);

		FJob Job;
		Job.Profile = Profile;
		Job.ProfileName = Profile->GetName();
		Job.Folder = RootFolder / ViewerBatchCapture::MakeBatchFolderName(Job.ProfileName, Time);
		Job.Shots = ViewerBatchCapture::BuildBatchPlan(Profile, InOptions);
		if (Job.Shots.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] %s has nothing to shoot (no Skeletal Mesh); skipped."), *Job.ProfileName);
			continue;
		}
		NewTotal += Job.Shots.Num();
		NewJobs.Add(MoveTemp(Job));
	}

	if (NewJobs.Num() == 0)
	{
		return false;
	}

	Host = InHost;
	Options = InOptions;
	Jobs = MoveTemp(NewJobs);
	TotalShots = NewTotal;
	WrittenShots = 0;
	FailedShots = 0;
	WrittenFiles.Reset();
	JobIndex = 0;
	ShotIndex = 0;
	SettleFrames = FMath::Max(0, InSettleFrames);
	SettleRemaining = 0;
	PhaseElapsedSeconds = 0.f;
	bSwitchingProfile = false;
	bRestoreSelectionOnFinish = true;
	Phase = EViewerBatchPhase::ShowingProfile;

	UE_LOG(LogTemp, Log, TEXT("[CharacterViewerBatch] Started: %d profile(s), %d shot(s), expressions=%d poses=%d quit=%d."),
		Jobs.Num(), TotalShots, Options.bIncludeExpressions ? 1 : 0, Options.bIncludePoses ? 1 : 0, Options.bQuitWhenDone ? 1 : 0);
	Host->SetBatchStatus(GetProgressText(), false);
	return true;
}

bool FViewerBatchCaptureRunner::IsActive() const
{
	switch (Phase)
	{
	case EViewerBatchPhase::ShowingProfile:
	case EViewerBatchPhase::ProfileSettling:
	case EViewerBatchPhase::ApplyingShot:
	case EViewerBatchPhase::ShotSettling:
	case EViewerBatchPhase::Capturing:
		return true;
	default:
		return false;
	}
}

const FViewerBatchShot* FViewerBatchCaptureRunner::GetCurrentShot() const
{
	return (Jobs.IsValidIndex(JobIndex) && Jobs[JobIndex].Shots.IsValidIndex(ShotIndex)) ? &Jobs[JobIndex].Shots[ShotIndex] : nullptr;
}

FString FViewerBatchCaptureRunner::GetCurrentShotPath() const
{
	const FViewerBatchShot* Shot = GetCurrentShot();
	return Shot ? Jobs[JobIndex].Folder / Shot->FileName : FString();
}

void FViewerBatchCaptureRunner::Tick(float DeltaSeconds)
{
	if (!IsActive() || !Host.IsValid())
	{
		return;
	}

	switch (Phase)
	{
	case EViewerBatchPhase::ShowingProfile:
	{
		FJob& Job = Jobs[JobIndex];
		UCharacterProfileData* Profile = Job.Profile.Get();
		bool bShown = false;
		if (Profile)
		{
			TGuardValue<bool> SwitchGuard(bSwitchingProfile, true);
			bShown = Host->ShowBatchProfile(Profile);
		}
		if (!IsActive())
		{
			return; // cancelled from inside the host call
		}
		if (!bShown)
		{
			UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] Could not show profile %s; its %d shot(s) are skipped."), *Job.ProfileName, Job.Shots.Num());
			FailedShots += Job.Shots.Num();
			AdvanceJob();
			return;
		}
		PhaseElapsedSeconds = 0.f;
		Phase = EViewerBatchPhase::ProfileSettling;
		return;
	}

	case EViewerBatchPhase::ProfileSettling:
	{
		PhaseElapsedSeconds += FMath::Max(0.f, DeltaSeconds);
		if (PhaseElapsedSeconds < Options.ProfileSettleSeconds)
		{
			return;
		}
		if (!Host->IsBatchSceneReady())
		{
			if (!IsActive() || PhaseElapsedSeconds < Options.ProfileSettleSeconds + Options.SceneReadyTimeoutSeconds)
			{
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] Scene still not ready after %.0f s (shaders compiling?); shooting %s anyway."),
				PhaseElapsedSeconds, *Jobs[JobIndex].ProfileName);
		}
		ShotIndex = 0;
		Phase = EViewerBatchPhase::ApplyingShot;
		return;
	}

	case EViewerBatchPhase::ApplyingShot:
		if (const FViewerBatchShot* Shot = GetCurrentShot())
		{
			Host->SetBatchStatus(GetProgressText(), false);
			Host->ApplyBatchShot(*Shot);
			if (!IsActive())
			{
				return;
			}
			SettleRemaining = SettleFrames;
			PhaseElapsedSeconds = 0.f;
			Phase = EViewerBatchPhase::ShotSettling;
		}
		return;

	case EViewerBatchPhase::ShotSettling:
	{
		if (SettleRemaining > 0)
		{
			--SettleRemaining;
			return;
		}
		PhaseElapsedSeconds += FMath::Max(0.f, DeltaSeconds);
		if (!Host->IsBatchCameraSettled())
		{
			if (PhaseElapsedSeconds < Options.CameraSettleTimeoutSeconds)
			{
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerBatch] Camera still moving after %.1f s; taking the shot anyway."), PhaseElapsedSeconds);
		}

		const FString Path = GetCurrentShotPath();
		if (!Host->StartBatchShot(Path))
		{
			UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] Could not start the shot '%s'."), *Path);
			AdvanceShot(false);
			return;
		}
		if (!IsActive())
		{
			return;
		}
		Phase = EViewerBatchPhase::Capturing;
		// The single-shot capture sets its own "Capturing screenshot..." line; keep the batch progress instead.
		Host->SetBatchStatus(GetProgressText(), false);
		return;
	}

	case EViewerBatchPhase::Capturing:
		if (Host->IsBatchShotRunning())
		{
			return;
		}
		AdvanceShot(Host->WasBatchShotWritten(GetCurrentShotPath()));
		return;

	default:
		return;
	}
}

void FViewerBatchCaptureRunner::AdvanceShot(bool bWritten)
{
	const FString Path = GetCurrentShotPath();
	if (bWritten)
	{
		++WrittenShots;
		WrittenFiles.Add(Path);
	}
	else
	{
		++FailedShots;
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerBatch] Shot not written: %s"), *Path);
	}

	++ShotIndex;
	if (Jobs.IsValidIndex(JobIndex) && ShotIndex < Jobs[JobIndex].Shots.Num())
	{
		Phase = EViewerBatchPhase::ApplyingShot;
		return;
	}
	AdvanceJob();
}

void FViewerBatchCaptureRunner::AdvanceJob()
{
	++JobIndex;
	ShotIndex = 0;
	if (JobIndex < Jobs.Num())
	{
		Phase = EViewerBatchPhase::ShowingProfile;
		return;
	}
	Finish(WrittenShots > 0 ? EViewerBatchPhase::Finished : EViewerBatchPhase::Failed);
}

void FViewerBatchCaptureRunner::Cancel(bool bRestoreSelection)
{
	if (!IsActive())
	{
		return;
	}
	bRestoreSelectionOnFinish = bRestoreSelection;
	const bool bWasCapturing = (Phase == EViewerBatchPhase::Capturing);
	// Not active from here on, so a CancelCapture() reached through the host
	// below only stops the single shot instead of re-entering this function.
	Phase = EViewerBatchPhase::Cancelled;
	if (bWasCapturing && Host.IsValid())
	{
		Host->CancelBatchShot();
	}
	Finish(EViewerBatchPhase::Cancelled);
}

void FViewerBatchCaptureRunner::Finish(EViewerBatchPhase EndPhase)
{
	Phase = EndPhase;
	bSwitchingProfile = false;

	const FString Summary = GetSummaryText();
	UE_LOG(LogTemp, Log, TEXT("[CharacterViewerBatch] %s (written %d, failed %d, total %d)"), *Summary, WrittenShots, FailedShots, TotalShots);
	for (const FString& Folder : GetOutputFolders())
	{
		UE_LOG(LogTemp, Log, TEXT("[CharacterViewerBatch] Output folder: %s"), *Folder);
	}

	// Released before the callback so a host that starts a new batch from it would work.
	const TSharedPtr<IViewerBatchCaptureHost> FinishedHost = MoveTemp(Host);
	Host.Reset();
	if (FinishedHost.IsValid())
	{
		FinishedHost->SetBatchStatus(Summary, true);
		FinishedHost->OnBatchFinished(*this);
	}
}

TArray<FString> FViewerBatchCaptureRunner::GetOutputFolders() const
{
	TArray<FString> Folders;
	for (const FString& File : WrittenFiles)
	{
		Folders.AddUnique(FPaths::GetPath(File));
	}
	return Folders;
}

FString FViewerBatchCaptureRunner::GetProgressText() const
{
	if (!IsActive())
	{
		return FString();
	}
	return FString::Printf(TEXT("Batch %d/%d"), FMath::Min(GetProcessedShots() + 1, TotalShots), TotalShots);
}

FString FViewerBatchCaptureRunner::GetSummaryText() const
{
	const TArray<FString> Folders = GetOutputFolders();
	FString Where;
	if (Folders.Num() == 1)
	{
		Where = FString::Printf(TEXT(" -> %s"), *ViewerCapture::MakeDisplayPath(Folders[0]));
	}
	else if (Folders.Num() > 1)
	{
		Where = FString::Printf(TEXT(" -> %d folders in %s"), Folders.Num(), *ViewerCapture::MakeDisplayPath(FPaths::GetPath(Folders[0])));
	}

	switch (Phase)
	{
	case EViewerBatchPhase::Cancelled:
		return FString::Printf(TEXT("Batch cancelled (%d/%d saved)%s"), WrittenShots, TotalShots, *Where);
	case EViewerBatchPhase::Failed:
		return FString::Printf(TEXT("Batch failed (0/%d saved, see log)"), TotalShots);
	case EViewerBatchPhase::Finished:
		return FailedShots > 0
			? FString::Printf(TEXT("Batch saved %d/%d, %d failed (see log)%s"), WrittenShots, TotalShots, FailedShots, *Where)
			: FString::Printf(TEXT("Batch saved %d/%d%s"), WrittenShots, TotalShots, *Where);
	default:
		return GetProgressText();
	}
}
