// Height ruler (G), lighting presets (N) and the profile check status line:
// the ACharacterViewerController side (Docs/CHARACTER_VIEWER_SETUP.md 6.20).
// Kept out of CharacterViewerController.cpp like the batch capture code; the
// controller only calls in from its input handlers, ToggleCleanView(),
// SwitchProfile() and PlayerTick().

#include "CharacterViewer/CharacterViewerController.h"

#include "Character/CharacterProfileData.h"
#include "Character/CharacterProfileValidator.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/ViewerHeightRuler.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UI/CharacterViewerWidget.h"

namespace CharacterViewerStudioPrivate
{
	// Preset values (lux / degrees), Docs/CHARACTER_VIEWER_SETUP.md 1.7 / 6.20.
	constexpr float FlatIntensity = 1.5f;
	constexpr float RimPresetKey = 0.3f;
	constexpr float RimPresetFill = 0.1f;
	constexpr float RimPresetRim = 3.0f;
	constexpr float TopPresetKeyPitch = -80.f;
	constexpr float TopPresetKey = 2.5f;
	constexpr float TopPresetFill = 0.5f;
	constexpr float TopPresetRim = 0.f;

	const FLinearColor ProfileErrorColor(1.f, 0.38f, 0.33f, 1.f);
	const FLinearColor ProfileWarningColor(1.f, 0.85f, 0.3f, 1.f);
	// Same green as the generated StatusText (UCharacterViewerWidget::BuildDefaultLayoutTree).
	const FLinearColor ProfileOkColor(0.55f, 0.9f, 0.55f, 1.f);

	FViewerLightSettings ReadLight(const ULightComponent& Light)
	{
		FViewerLightSettings Settings;
		Settings.Intensity = Light.Intensity;
		Settings.Color = Light.LightColor;
		// Relative rotation (= world for a placed Directional Light, whose
		// light component is the root): restoring the stored rotator through
		// SetRelativeRotationExact() is exact, a quaternion round trip is not.
		Settings.Rotation = Light.GetRelativeRotation();
		Settings.bCastShadows = Light.CastShadows != 0;
		return Settings;
	}

	void ApplyLight(ULightComponent& Light, const FViewerLightSettings& Settings)
	{
		Light.SetIntensity(Settings.Intensity);
		Light.SetLightFColor(Settings.Color);
		if (!Light.GetRelativeRotation().Equals(Settings.Rotation, 0.f))
		{
			// Exact: RelativeRotation becomes this very rotator (SetRelativeRotation()
			// goes through a quaternion and comes back a few ULPs off).
			Light.SetRelativeRotationExact(Settings.Rotation);
		}
		Light.SetCastShadows(Settings.bCastShadows);
	}

	// Horizontal (XY) direction the light travels in.
	FVector2D HorizontalDirection(const ULightComponent& Light)
	{
		const FVector Forward = Light.GetComponentRotation().Vector();
		return FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	}
}

// --- Height ruler ------------------------------------------------------------

void ACharacterViewerController::HandleToggleHeightRuler(const FInputActionValue& Value)
{
	ToggleHeightRuler();
}

void ACharacterViewerController::ToggleHeightRuler()
{
	SetHeightRulerEnabled(!bHeightRulerEnabled);
}

void ACharacterViewerController::SetHeightRulerEnabled(bool bEnabled)
{
	if (IsTurntableCaptureRunning())
	{
		return;
	}
	bHeightRulerEnabled = bEnabled;
	if (bHeightRulerEnabled)
	{
		UpdateHeightRuler();
	}
	ApplyHeightRulerVisibility();
	NotifyViewerWidget();
}

void ACharacterViewerController::UpdateHeightRuler()
{
	if (!bHeightRulerEnabled)
	{
		return;
	}

	float HeightCm = 0.f;
	float BottomZ = 0.f;
	float TopZ = 0.f;
	float HalfWidthCm = 0.f;
	FVector Center = FVector::ZeroVector;
	const bool bHasMesh = IsValid(ViewerActor) && ViewerActor->GetMeshHeightInfo(HeightCm, BottomZ, TopZ, HalfWidthCm, Center);

	UWorld* World = GetWorld();
	if (!IsValid(HeightRuler) && bHasMesh && World)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		HeightRuler = World->SpawnActor<AViewerHeightRuler>(AViewerHeightRuler::StaticClass(), FTransform::Identity, Params);
		if (HeightRuler)
		{
			UE_LOG(LogTemp, Log, TEXT("[CharacterViewer] Height ruler spawned (%s)."), *HeightRuler->GetName());
		}
	}

	if (IsValid(HeightRuler) && bHasMesh)
	{
		HeightRuler->SetMeasuredHeight(HeightCm);

		// Face the camera: the ruler's +X points back at the view, so its +Y
		// is screen-left. Placed at the feet (bounds bottom), beside the
		// actor's pivot by the bounds' half-width + gap, i.e. at the
		// character's depth and never inside the arms.
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		GetPlayerViewPoint(ViewLocation, ViewRotation);
		const float FacingYaw = FRotator::NormalizeAxis(ViewRotation.Yaw + 180.f);
		const FVector ScreenLeft = FRotator(0.f, FacingYaw, 0.f).RotateVector(FVector::YAxisVector);
		const FVector Pivot = ViewerActor->GetActorLocation();
		const FVector Feet = FVector(Pivot.X, Pivot.Y, BottomZ) + ScreenLeft * (HalfWidthCm + RulerSideGapCm);
		HeightRuler->SetRulerTransform(Feet, FacingYaw);
	}

	ApplyHeightRulerVisibility();
}

void ACharacterViewerController::ApplyHeightRulerVisibility()
{
	if (!IsValid(HeightRuler))
	{
		return;
	}
	const bool bHasMesh = IsValid(ViewerActor) && ViewerActor->Mesh && ViewerActor->Mesh->GetSkeletalMeshAsset();
	HeightRuler->SetRulerVisible(bHeightRulerEnabled && !bCleanViewActive && bHasMesh);
}

// --- Lighting presets ----------------------------------------------------------

void ACharacterViewerController::HandleCycleLighting(const FInputActionValue& Value)
{
	CycleLightingPreset();
}

void ACharacterViewerController::CycleLightingPreset()
{
	const uint8 Next = (static_cast<uint8>(LightingPreset) + 1) % (static_cast<uint8>(EViewerLightingPreset::Top) + 1);
	SetLightingPreset(static_cast<EViewerLightingPreset>(Next));
}

FString ACharacterViewerController::GetLightingPresetDisplayName(EViewerLightingPreset Preset)
{
	switch (Preset)
	{
	case EViewerLightingPreset::Flat:
		return TEXT("Flat");
	case EViewerLightingPreset::Rim:
		return TEXT("Rim");
	case EViewerLightingPreset::Top:
		return TEXT("Top");
	default:
		return TEXT("Studio");
	}
}

FViewerLightSettings ACharacterViewerController::GetLightingPresetSettings(EViewerLightingPreset Preset, EViewerLightRole LightRole, const FViewerLightSettings& Original)
{
	using namespace CharacterViewerStudioPrivate;

	FViewerLightSettings Out = Original;
	switch (Preset)
	{
	case EViewerLightingPreset::Flat:
		// Even, shadowless white light from the authored directions.
		Out.Intensity = FlatIntensity;
		Out.Color = FColor::White;
		Out.bCastShadows = false;
		break;
	case EViewerLightingPreset::Rim:
		Out.Intensity = LightRole == EViewerLightRole::Key ? RimPresetKey : (LightRole == EViewerLightRole::Fill ? RimPresetFill : RimPresetRim);
		break;
	case EViewerLightingPreset::Top:
		if (LightRole == EViewerLightRole::Key)
		{
			Out.Rotation = FRotator(TopPresetKeyPitch, Original.Rotation.Yaw, 0.f);
			Out.Intensity = TopPresetKey;
		}
		else
		{
			Out.Intensity = LightRole == EViewerLightRole::Fill ? TopPresetFill : TopPresetRim;
		}
		break;
	default:
		break;
	}
	return Out;
}

ULightComponent* ACharacterViewerController::GetLightingTarget(EViewerLightRole LightRole) const
{
	for (const FViewerLightTarget& Target : LightingTargets)
	{
		if (Target.Role == LightRole)
		{
			return Target.Light.Get();
		}
	}
	return nullptr;
}

void ACharacterViewerController::ResolveLightingTargets()
{
	using namespace CharacterViewerStudioPrivate;

	LightingTargets.RemoveAll([](const FViewerLightTarget& Target) { return !Target.Light.IsValid(); });
	if (bLightingTargetsResolved)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	bLightingTargetsResolved = true;

	struct FCandidate
	{
		UDirectionalLightComponent* Light = nullptr;
		FString Label;
	};
	TArray<FCandidate> Candidates;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UDirectionalLightComponent*> Lights(Actor);
		for (UDirectionalLightComponent* Light : Lights)
		{
			if (!Light)
			{
				continue;
			}
			if (Light->GetMobility() == EComponentMobility::Static)
			{
				// Intensity/colour of a Static light cannot change at runtime.
				UE_LOG(LogTemp, Log, TEXT("[CharacterViewer] Lighting presets: '%s' is Static, skipped."), *Actor->GetActorNameOrLabel());
				continue;
			}
			Candidates.Add({ Light, Actor->GetActorNameOrLabel() });
		}
	}

	auto AddTarget = [this](UDirectionalLightComponent* Light, EViewerLightRole LightRole)
	{
		FViewerLightTarget Target;
		Target.Light = Light;
		Target.Role = LightRole;
		Target.Original = ReadLight(*Light);
		LightingTargets.Add(Target);
	};

	// 1. Editor: the Outliner labels apply_studio_setup() gives the lights.
	UDirectionalLightComponent* Labelled[3] = { nullptr, nullptr, nullptr };
	const TCHAR* Labels[3] = { TEXT("KeyLight"), TEXT("FillLight"), TEXT("RimLight") };
	for (const FCandidate& Candidate : Candidates)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (!Labelled[Index] && Candidate.Label == Labels[Index])
			{
				Labelled[Index] = Candidate.Light;
			}
		}
	}
	const TCHAR* Rule = TEXT("labels");
	if (Labelled[0] && Labelled[1] && Labelled[2])
	{
		AddTarget(Labelled[0], EViewerLightRole::Key);
		AddTarget(Labelled[1], EViewerLightRole::Fill);
		AddTarget(Labelled[2], EViewerLightRole::Rim);
	}
	else if (Candidates.Num() > 0)
	{
		// 2. Cooked builds (no labels): Key = brightest shadow caster (or
		// brightest), Rim = most opposite horizontal direction to the Key,
		// Fill = brightest of the rest.
		Rule = TEXT("intensity/shadow/direction");
		TArray<UDirectionalLightComponent*> Pool;
		for (const FCandidate& Candidate : Candidates)
		{
			Pool.Add(Candidate.Light);
		}
		Pool.Sort([](const UDirectionalLightComponent& A, const UDirectionalLightComponent& B) { return A.Intensity > B.Intensity; });

		UDirectionalLightComponent* Key = Pool[0];
		for (UDirectionalLightComponent* Light : Pool)
		{
			if (Light->CastShadows)
			{
				Key = Light;
				break;
			}
		}
		Pool.Remove(Key);
		AddTarget(Key, EViewerLightRole::Key);

		if (Pool.Num() > 0)
		{
			const FVector2D KeyDirection = HorizontalDirection(*Key);
			UDirectionalLightComponent* Rim = Pool[0];
			double BestDot = TNumericLimits<double>::Max();
			for (UDirectionalLightComponent* Light : Pool)
			{
				const double Dot = FVector2D::DotProduct(KeyDirection, HorizontalDirection(*Light));
				if (Dot < BestDot)
				{
					BestDot = Dot;
					Rim = Light;
				}
			}
			Pool.Remove(Rim);
			if (Pool.Num() > 0)
			{
				AddTarget(Pool[0], EViewerLightRole::Fill);
				Pool.RemoveAt(0);
			}
			AddTarget(Rim, EViewerLightRole::Rim);
		}
		if (Pool.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[CharacterViewer] Lighting presets: %d extra directional light(s) left unchanged."), Pool.Num());
		}
	}

	if (LightingTargets.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[CharacterViewer] Lighting presets: no movable directional light in this level (KeyLight/FillLight/RimLight); presets change nothing."));
		return;
	}
	for (const FViewerLightTarget& Target : LightingTargets)
	{
		const ULightComponent* Light = Target.Light.Get();
		UE_LOG(LogTemp, Log, TEXT("[CharacterViewer] Lighting preset target (%s): %s = %s, %.2f lux, rot %s, shadows %d"),
			Rule, Target.Role == EViewerLightRole::Key ? TEXT("Key") : (Target.Role == EViewerLightRole::Fill ? TEXT("Fill") : TEXT("Rim")),
			Light && Light->GetOwner() ? *Light->GetOwner()->GetActorNameOrLabel() : TEXT("?"),
			Target.Original.Intensity, *Target.Original.Rotation.ToString(), Target.Original.bCastShadows ? 1 : 0);
	}
}

void ACharacterViewerController::SetLightingPreset(EViewerLightingPreset Preset)
{
	// Every frame of a turntable/batch capture shares one look.
	if (IsTurntableCaptureRunning() || BatchRunner.IsActive())
	{
		return;
	}

	LightingPreset = Preset;
	ResolveLightingTargets();
	for (const FViewerLightTarget& Target : LightingTargets)
	{
		if (ULightComponent* Light = Target.Light.Get())
		{
			CharacterViewerStudioPrivate::ApplyLight(*Light, GetLightingPresetSettings(Preset, Target.Role, Target.Original));
		}
	}
	NotifyViewerWidget();
}

// --- Profile check status line ---------------------------------------------------

FString ACharacterViewerController::FormatProfileValidationStatus(int32 ErrorCount, int32 WarningCount)
{
	if (ErrorCount <= 0 && WarningCount <= 0)
	{
		return TEXT("프로필 OK");
	}
	// U+00B7 middle dot, as in the INSPECTION text.
	return FString::Printf(TEXT("프로필 검사: 오류 %d · 경고 %d (로그/2.10절 참고)"), FMath::Max(0, ErrorCount), FMath::Max(0, WarningCount));
}

FLinearColor ACharacterViewerController::GetProfileValidationStatusColor(int32 ErrorCount, int32 WarningCount)
{
	using namespace CharacterViewerStudioPrivate;
	if (ErrorCount > 0)
	{
		return ProfileErrorColor;
	}
	return WarningCount > 0 ? ProfileWarningColor : ProfileOkColor;
}

void ACharacterViewerController::ShowProfileValidationStatus()
{
	// A running capture/batch owns the status line (and the batch switches
	// profiles on its own).
	if (!IsValid(ViewerActor) || IsCapturing() || BatchRunner.IsActive())
	{
		return;
	}
	const TArray<FViewerProfileIssue> Issues = UCharacterProfileValidator::ValidateProfile(ViewerActor->Profile);
	const int32 Errors = UCharacterProfileValidator::CountBySeverity(Issues, EViewerIssueSeverity::Error);
	const int32 Warnings = UCharacterProfileValidator::CountBySeverity(Issues, EViewerIssueSeverity::Warning);
	const FLinearColor Color = GetProfileValidationStatusColor(Errors, Warnings);
	// OK keeps the status line's own colour (a designer WBP's choice).
	SetStatusLine(FormatProfileValidationStatus(Errors, Warnings), ProfileStatusSeconds, (Errors > 0 || Warnings > 0) ? &Color : nullptr);
}

void ACharacterViewerController::SetStatusLine(const FString& Status, float Seconds, const FLinearColor* Color)
{
	CaptureStatusExpireSeconds = (Seconds > 0.f && !Status.IsEmpty()) ? FPlatformTime::Seconds() + Seconds : 0.0;
	if (!ViewerWidget)
	{
		return;
	}
	if (Color)
	{
		ViewerWidget->SetStatusLine(FText::FromString(Status), *Color);
	}
	else
	{
		ViewerWidget->SetCaptureStatus(FText::FromString(Status));
	}
}

void ACharacterViewerController::SetViewerWidget(UCharacterViewerWidget* InWidget)
{
	ViewerWidget = InWidget;
	if (ViewerWidget)
	{
		ViewerWidget->BindToViewer(this, ViewerActor, CameraPawn);
	}
}
