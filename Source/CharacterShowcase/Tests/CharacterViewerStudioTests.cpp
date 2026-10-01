#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/UserWidget.h"
#include "Character/CharacterProfileData.h"
#include "Character/CharacterProfileValidator.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "CharacterViewer/ViewerHeightRuler.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "UI/CharacterViewerWidget.h"

// Height ruler (G), lighting presets (N) and the profile check status line
// (Docs/CHARACTER_VIEWER_SETUP.md 6.20). Transient test worlds only; content
// assets (SKM_Manny_Simple, SkeletalCube, DA_Character_Manny) are loaded
// read-only and never modified or saved.

namespace CharacterViewerStudioTestsPrivate
{
	const TCHAR* MannyMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* CubeMeshPath = TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube");
	const TCHAR* MannyProfilePath = TEXT("/Game/Portfolio/Data/DA_Character_Manny.DA_Character_Manny");

	UWorld* CreateTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
		}
		return World;
	}

	UCharacterProfileData* MakeProfile(UObject* Outer, USkeletalMesh* Mesh, const TCHAR* Name)
	{
		UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(Outer);
		Profile->SkeletalMesh = Mesh;
		Profile->DisplayName = FText::FromString(Name);
		return Profile;
	}

	// The level's studio lights (Docs/CHARACTER_VIEWER_SETUP.md 2 ⑨).
	ULightComponent* SpawnLight(UWorld* World, float Intensity, FColor Color, const FRotator& Rotation, bool bShadows, const TCHAR* Label)
	{
		ADirectionalLight* Actor = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, Rotation);
		if (!Actor)
		{
			return nullptr;
		}
#if WITH_EDITOR
		if (Label)
		{
			Actor->SetActorLabel(Label);
		}
#endif
		ULightComponent* Light = Actor->GetLightComponent();
		Light->SetMobility(EComponentMobility::Movable);
		// SpawnActor composes the spawn rotation with the class's default root
		// rotation; set the authored world rotation explicitly (root component).
		Light->SetRelativeRotationExact(Rotation);
		Light->SetIntensity(Intensity);
		Light->SetLightFColor(Color);
		Light->SetCastShadows(bShadows);
		return Light;
	}

	FViewerLightSettings Read(const ULightComponent* Light)
	{
		FViewerLightSettings Settings;
		if (Light)
		{
			Settings.Intensity = Light->Intensity;
			Settings.Color = Light->LightColor;
			Settings.Rotation = Light->GetRelativeRotation();
			Settings.bCastShadows = Light->CastShadows != 0;
		}
		return Settings;
	}
}

// Ruler spawn/visibility, Manny height, Clean View, profile switch, INSPECTION height.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerHeightRulerTest,
	"CharacterShowcase.Viewer.HeightRuler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerHeightRulerTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerStudioTestsPrivate;

	// Pure helpers.
	TestEqual(TEXT("Ruler top for 182 cm"), AViewerHeightRuler::ComputeRulerTopCm(182.f), 200.f);
	TestEqual(TEXT("Ruler top without height"), AViewerHeightRuler::ComputeRulerTopCm(-1.f), 200.f);
	TestEqual(TEXT("Ruler top for 230 cm"), AViewerHeightRuler::ComputeRulerTopCm(230.f), 250.f);
	TestEqual(TEXT("Height label"), AViewerHeightRuler::FormatHeightLabel(181.6f), FString(TEXT("Height 182 cm")));

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	USkeletalMesh* Cube = LoadObject<USkeletalMesh>(nullptr, CubeMeshPath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny) || !TestNotNull(TEXT("SkeletalCube loads"), Cube))
	{
		return false;
	}

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor) || !TestNotNull(TEXT("Viewer controller exists"), Controller))
	{
		World->DestroyWorld(false);
		return false;
	}

	UCharacterProfileData* MannyProfile = MakeProfile(World, Manny, TEXT("Manny"));
	UCharacterProfileData* CubeProfile = MakeProfile(World, Cube, TEXT("Cube"));
	Actor->ApplyProfile(MannyProfile);
	Controller->SetViewerActor(Actor);

	// Measured height of SKM_Manny_Simple (imported bounds).
	float Height = 0.f;
	float Bottom = 0.f;
	float Top = 0.f;
	float HalfWidth = 0.f;
	FVector Center = FVector::ZeroVector;
	TestTrue(TEXT("GetMeshHeightInfo succeeds for Manny"), Actor->GetMeshHeightInfo(Height, Bottom, Top, HalfWidth, Center));
	AddInfo(FString::Printf(TEXT("SKM_Manny_Simple measured height %.2f cm (bottom Z %.2f, top Z %.2f, half-width %.2f cm)"), Height, Bottom, Top, HalfWidth));
	TestTrue(FString::Printf(TEXT("Manny height ~180 cm (+-10), got %.2f"), Height), FMath::Abs(Height - 180.f) <= 10.f);
	TestTrue(TEXT("GetMeshStats().HeightCm matches"), FMath::IsNearlyEqual(Actor->GetMeshStats().HeightCm, Height, 0.01f));

	// Off by default: nothing spawned.
	TestFalse(TEXT("Ruler is off by default"), Controller->IsHeightRulerEnabled());
	TestNull(TEXT("No ruler before the first G"), Controller->GetHeightRuler());

	Controller->ToggleHeightRuler();
	AViewerHeightRuler* Ruler = Controller->GetHeightRuler();
	if (!TestNotNull(TEXT("G spawns the ruler"), Ruler))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestTrue(TEXT("Ruler enabled"), Controller->IsHeightRulerEnabled());
	TestTrue(TEXT("Ruler visible"), Ruler->IsRulerVisible());
	TestTrue(TEXT("Ruler is transient"), Ruler->HasAnyFlags(RF_Transient));
	TestEqual(TEXT("Ruler top 200 cm for Manny"), Ruler->GetRulerTopCm(), 200.f);
	TestEqual(TEXT("21 ticks (0..200 every 10 cm)"), Ruler->GetTickCount(), 21);
	TestEqual(TEXT("4 scale labels (50/100/150/200)"), Ruler->GetScaleLabelCount(), 4);
	TestTrue(TEXT("Height marker + label exist"), Ruler->HasHeightMarker());
	TestEqual(TEXT("Height label text"), Ruler->GetHeightLabelText(), AViewerHeightRuler::FormatHeightLabel(Height));
	TestTrue(FString::Printf(TEXT("Ruler measured height %.2f matches the actor"), Ruler->GetMeasuredHeight()), FMath::Abs(Ruler->GetMeasuredHeight() - Height) <= 0.06f);
	AddInfo(FString::Printf(TEXT("Ruler components: %d ticks, %d scale labels (%d visible), height label '%s'"),
		Ruler->GetTickCount(), Ruler->GetScaleLabelCount(), Ruler->GetVisibleScaleLabelCount(), *Ruler->GetHeightLabelText()));

	// No collision (never blocks the Inspection trace), no shadow.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Ruler);
	int32 Colliding = 0;
	int32 Shadowing = 0;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		Colliding += Primitive->IsCollisionEnabled() ? 1 : 0;
		Shadowing += Primitive->CastShadow ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("Ruler primitives: %d"), Primitives.Num()));
	TestEqual(TEXT("Primitive count = bar + ticks + labels + marker + height label"), Primitives.Num(), 1 + 21 + 4 + 2);
	TestEqual(TEXT("No ruler primitive has collision"), Colliding, 0);
	TestEqual(TEXT("No ruler primitive casts shadows"), Shadowing, 0);

	// Placement: feet level, screen-left of the character (default view yaw 0 -> -Y), facing the camera.
	const FVector RulerLocation = Ruler->GetActorLocation();
	AddInfo(FString::Printf(TEXT("Ruler at %s, yaw %.1f; actor at %s"), *RulerLocation.ToString(), Ruler->GetActorRotation().Yaw, *Actor->GetActorLocation().ToString()));
	TestTrue(TEXT("Ruler base at the mesh bottom"), FMath::IsNearlyEqual(RulerLocation.Z, static_cast<double>(Bottom), 0.1));
	TestTrue(TEXT("Ruler beside the bounds (|dY| = half-width + gap)"), FMath::IsNearlyEqual(FMath::Abs(RulerLocation.Y - Actor->GetActorLocation().Y), static_cast<double>(HalfWidth + Controller->RulerSideGapCm), 0.5));
	TestTrue(TEXT("Ruler on the screen-left (-Y for the default view)"), RulerLocation.Y < Actor->GetActorLocation().Y);
	TestTrue(TEXT("Ruler faces the camera (yaw 180)"), FMath::IsNearlyEqual(FMath::Abs(Ruler->GetActorRotation().Yaw), 180.0, 0.5));

	// The turntable does not move the ruler.
	Actor->SetTurntableEnabled(true);
	Actor->AdvanceTurntable(1.f);
	Controller->UpdateHeightRuler();
	TestTrue(TEXT("Turntable rotation does not move the ruler"), Ruler->GetActorLocation().Equals(RulerLocation, 0.1));
	Actor->SetTurntableEnabled(false);

	// Clean View hides it, leaving Clean View shows it again.
	Controller->ToggleCleanView();
	TestFalse(TEXT("Clean View hides the ruler"), Ruler->IsRulerVisible());
	TestTrue(TEXT("Toggle stays on during Clean View"), Controller->IsHeightRulerEnabled());
	Controller->ToggleCleanView();
	TestTrue(TEXT("Leaving Clean View shows the ruler"), Ruler->IsRulerVisible());

	// Profile switch keeps the toggle and re-measures.
	Controller->SwitchProfile(CubeProfile);
	float CubeHeight = 0.f;
	Actor->GetMeshHeightInfo(CubeHeight, Bottom, Top, HalfWidth, Center);
	AddInfo(FString::Printf(TEXT("SkeletalCube measured height %.2f cm, ruler %.2f cm"), CubeHeight, Ruler->GetMeasuredHeight()));
	TestTrue(TEXT("Toggle survives the profile switch"), Controller->IsHeightRulerEnabled());
	TestTrue(TEXT("Same ruler actor after the switch"), Controller->GetHeightRuler() == Ruler);
	TestTrue(TEXT("Ruler visible after the switch"), Ruler->IsRulerVisible());
	TestTrue(TEXT("Ruler re-measured the new mesh"), FMath::Abs(Ruler->GetMeasuredHeight() - CubeHeight) <= 0.06f && !FMath::IsNearlyEqual(CubeHeight, Height, 1.f));
	Controller->SwitchProfile(MannyProfile);
	TestTrue(TEXT("Ruler back to Manny's height"), FMath::Abs(Ruler->GetMeasuredHeight() - Height) <= 0.06f);

	// No mesh: hidden, toggle kept.
	Controller->SwitchProfile(MakeProfile(World, nullptr, TEXT("Empty")));
	TestFalse(TEXT("No mesh: ruler hidden"), Ruler->IsRulerVisible());
	Controller->SwitchProfile(MannyProfile);
	TestTrue(TEXT("Mesh back: ruler visible"), Ruler->IsRulerVisible());

	// Off.
	Controller->ToggleHeightRuler();
	TestFalse(TEXT("Second G turns it off"), Controller->IsHeightRulerEnabled());
	TestFalse(TEXT("Ruler hidden when off"), Ruler->IsRulerVisible());

	// INSPECTION summary line carries the same height.
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (TestNotNull(TEXT("Widget exists"), Widget))
	{
		Widget->BindToViewer(Controller, Actor, nullptr);
		const FString Inspection = Widget->BuildInspectionText().ToString();
		const FString Expected = FString::Printf(TEXT("Morphs 0 · Height %d cm"), FMath::RoundToInt(Height));
		TestTrue(FString::Printf(TEXT("INSPECTION summary contains '%s'"), *Expected), Inspection.Contains(Expected));
	}

	World->DestroyWorld(false);
	return true;
}

// Lighting presets on three directional lights with the LV_Portfolio values.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerLightingPresetsTest,
	"CharacterShowcase.Viewer.LightingPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerLightingPresetsTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerStudioTestsPrivate;

	TestEqual(TEXT("Display name Studio"), ACharacterViewerController::GetLightingPresetDisplayName(EViewerLightingPreset::Studio), FString(TEXT("Studio")));
	TestEqual(TEXT("Display name Top"), ACharacterViewerController::GetLightingPresetDisplayName(EViewerLightingPreset::Top), FString(TEXT("Top")));

	const FRotator KeyRotation(-40.f, 30.f, 0.f);
	const FRotator FillRotation(-15.f, -50.f, 0.f);
	const FRotator RimRotation(-35.f, -150.f, 0.f);

	// --- Cooked-build rule (no labels): shadows/intensity/direction.
	{
		UWorld* World = CreateTestWorld();
		if (!TestNotNull(TEXT("Transient world exists"), World))
		{
			return false;
		}
		// Spawn order deliberately not Key/Fill/Rim.
		ULightComponent* Rim = SpawnLight(World, 1.8f, FColor(255, 255, 255), RimRotation, false, nullptr);
		ULightComponent* Fill = SpawnLight(World, 0.9f, FColor(222, 232, 255), FillRotation, false, nullptr);
		ULightComponent* Key = SpawnLight(World, 2.7f, FColor(255, 244, 229), KeyRotation, true, nullptr);
		ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
		if (!TestNotNull(TEXT("Lights exist"), Key) || !Fill || !Rim || !TestNotNull(TEXT("Controller exists"), Controller))
		{
			World->DestroyWorld(false);
			return false;
		}
		const FViewerLightSettings KeyOriginal = Read(Key);
		const FViewerLightSettings FillOriginal = Read(Fill);
		const FViewerLightSettings RimOriginal = Read(Rim);

		TestEqual(TEXT("Studio before any preset"), Controller->GetLightingPreset(), EViewerLightingPreset::Studio);
		TestEqual(TEXT("No targets before the first preset"), Controller->GetLightingTargetCount(), 0);

		Controller->CycleLightingPreset();
		TestEqual(TEXT("N: Studio -> Flat"), Controller->GetLightingPreset(), EViewerLightingPreset::Flat);
		TestEqual(TEXT("3 lights resolved"), Controller->GetLightingTargetCount(), 3);
		TestTrue(TEXT("Key = the shadow-casting brightest light"), Controller->GetLightingTarget(EViewerLightRole::Key) == Key);
		TestTrue(TEXT("Rim = the light opposite the Key"), Controller->GetLightingTarget(EViewerLightRole::Rim) == Rim);
		TestTrue(TEXT("Fill = the remaining light"), Controller->GetLightingTarget(EViewerLightRole::Fill) == Fill);

		for (ULightComponent* Light : { Key, Fill, Rim })
		{
			TestEqual(TEXT("Flat: 1.5 lux"), Light->Intensity, 1.5f);
			TestTrue(TEXT("Flat: white"), Light->LightColor == FColor::White);
			TestFalse(TEXT("Flat: no shadows"), Light->CastShadows != 0);
		}
		TestTrue(TEXT("Flat: key direction unchanged"), Key->GetRelativeRotation().Equals(KeyOriginal.Rotation, 0.f));
		TestTrue(FString::Printf(TEXT("Key authored rotation (-40, 30) (got %s)"), *KeyOriginal.Rotation.ToString()), KeyOriginal.Rotation.Equals(KeyRotation, 0.01f));

		Controller->CycleLightingPreset();
		TestEqual(TEXT("N: Flat -> Rim"), Controller->GetLightingPreset(), EViewerLightingPreset::Rim);
		TestEqual(TEXT("Rim: key 0.3"), Key->Intensity, 0.3f);
		TestEqual(TEXT("Rim: fill 0.1"), Fill->Intensity, 0.1f);
		TestEqual(TEXT("Rim: rim 3.0"), Rim->Intensity, 3.0f);
		TestTrue(TEXT("Rim: key colour authored"), Key->LightColor == KeyOriginal.Color);
		TestTrue(TEXT("Rim: key shadows authored"), Key->CastShadows != 0);

		Controller->CycleLightingPreset();
		TestEqual(TEXT("N: Rim -> Top"), Controller->GetLightingPreset(), EViewerLightingPreset::Top);
		TestEqual(TEXT("Top: key 2.5"), Key->Intensity, 2.5f);
		TestEqual(TEXT("Top: fill 0.5"), Fill->Intensity, 0.5f);
		TestEqual(TEXT("Top: rim 0"), Rim->Intensity, 0.f);
		TestTrue(FString::Printf(TEXT("Top: key pitch -80, yaw 30 (got %s)"), *Key->GetComponentRotation().ToString()),
			Key->GetComponentRotation().Equals(FRotator(-80.f, 30.f, 0.f), 0.01f));
		TestTrue(TEXT("Top: fill direction unchanged"), Fill->GetRelativeRotation().Equals(FillOriginal.Rotation, 0.f));

		Controller->CycleLightingPreset();
		TestEqual(TEXT("N: Top -> Studio"), Controller->GetLightingPreset(), EViewerLightingPreset::Studio);
		auto ExpectRestored = [this](const TCHAR* Name, const ULightComponent* Light, const FViewerLightSettings& Was)
		{
			const FViewerLightSettings Now = Read(Light);
			TestTrue(FString::Printf(TEXT("Studio restores %s intensity exactly (%f vs %f)"), Name, Now.Intensity, Was.Intensity), Now.Intensity == Was.Intensity);
			TestTrue(FString::Printf(TEXT("Studio restores %s colour exactly (%s vs %s)"), Name, *Now.Color.ToString(), *Was.Color.ToString()), Now.Color == Was.Color);
			TestTrue(FString::Printf(TEXT("Studio restores %s rotation exactly (%s vs %s)"), Name, *Now.Rotation.ToString(), *Was.Rotation.ToString()), Now.Rotation.Equals(Was.Rotation, 0.f));
			TestTrue(FString::Printf(TEXT("Studio restores %s shadows"), Name), Now.bCastShadows == Was.bCastShadows);
		};
		ExpectRestored(TEXT("Key"), Key, KeyOriginal);
		ExpectRestored(TEXT("Fill"), Fill, FillOriginal);
		ExpectRestored(TEXT("Rim"), Rim, RimOriginal);
		AddInfo(FString::Printf(TEXT("Studio restored: key %.2f lux %s, fill %.2f, rim %.2f"), Key->Intensity, *Key->GetRelativeRotation().ToString(), Fill->Intensity, Rim->Intensity));

		// Clean View does not touch the lights.
		Controller->SetLightingPreset(EViewerLightingPreset::Rim);
		Controller->ToggleCleanView();
		TestEqual(TEXT("Clean View keeps the preset"), Controller->GetLightingPreset(), EViewerLightingPreset::Rim);
		TestEqual(TEXT("Clean View keeps the rim intensity"), Rim->Intensity, 3.0f);
		Controller->ToggleCleanView();
		Controller->SetLightingPreset(EViewerLightingPreset::Studio);
		TestEqual(TEXT("Studio again: key intensity"), Key->Intensity, KeyOriginal.Intensity);

		World->DestroyWorld(false);
	}

#if WITH_EDITOR
	// --- Editor labels win over the rule (Fill is brightest and casts shadows here).
	{
		UWorld* World = CreateTestWorld();
		if (World)
		{
			ULightComponent* Key = SpawnLight(World, 1.0f, FColor::White, KeyRotation, false, TEXT("KeyLight"));
			ULightComponent* Fill = SpawnLight(World, 5.0f, FColor::White, FillRotation, true, TEXT("FillLight"));
			ULightComponent* Rim = SpawnLight(World, 0.5f, FColor::White, RimRotation, false, TEXT("RimLight"));
			ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
			if (Controller && Key && Fill && Rim)
			{
				Controller->SetLightingPreset(EViewerLightingPreset::Rim);
				TestTrue(TEXT("Labels: KeyLight is Key"), Controller->GetLightingTarget(EViewerLightRole::Key) == Key);
				TestTrue(TEXT("Labels: FillLight is Fill"), Controller->GetLightingTarget(EViewerLightRole::Fill) == Fill);
				TestTrue(TEXT("Labels: RimLight is Rim"), Controller->GetLightingTarget(EViewerLightRole::Rim) == Rim);
				TestEqual(TEXT("Labels: Rim preset on RimLight"), Rim->Intensity, 3.0f);
			}
			World->DestroyWorld(false);
		}
	}
#endif

	// --- No lights: no crash, preset recorded, nothing driven.
	{
		UWorld* World = CreateTestWorld();
		if (!TestNotNull(TEXT("Empty world exists"), World))
		{
			return false;
		}
		ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
		if (TestNotNull(TEXT("Controller in empty world"), Controller))
		{
			Controller->SetLightingPreset(EViewerLightingPreset::Flat);
			Controller->CycleLightingPreset();
			TestEqual(TEXT("No lights: preset recorded"), Controller->GetLightingPreset(), EViewerLightingPreset::Rim);
			TestEqual(TEXT("No lights: 0 targets"), Controller->GetLightingTargetCount(), 0);
			TestNull(TEXT("No lights: no key"), Controller->GetLightingTarget(EViewerLightRole::Key));
		}
		World->DestroyWorld(false);
	}
	return true;
}

// Profile check result in the panel status line.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerProfileStatusLineTest,
	"CharacterShowcase.Viewer.ProfileStatusLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerProfileStatusLineTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerStudioTestsPrivate;

	TestEqual(TEXT("0/0 -> OK"), ACharacterViewerController::FormatProfileValidationStatus(0, 0), FString(TEXT("프로필 OK")));
	TestEqual(TEXT("2/3 text"), ACharacterViewerController::FormatProfileValidationStatus(2, 3), FString(TEXT("프로필 검사: 오류 2 · 경고 3 (로그/2.10절 참고)")));
	const FLinearColor Red = ACharacterViewerController::GetProfileValidationStatusColor(1, 0);
	const FLinearColor Yellow = ACharacterViewerController::GetProfileValidationStatusColor(0, 2);
	TestTrue(TEXT("Errors are red"), Red.R > 0.9f && Red.G < 0.5f);
	TestTrue(TEXT("Warnings only are yellow"), Yellow.R > 0.9f && Yellow.G > 0.7f && Yellow.B < 0.5f);
	TestTrue(TEXT("Errors win over warnings"), ACharacterViewerController::GetProfileValidationStatusColor(1, 5).Equals(Red));

	UCharacterProfileData* MannyContent = LoadObject<UCharacterProfileData>(nullptr, MannyProfilePath);
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("DA_Character_Manny loads"), MannyContent) || !TestNotNull(TEXT("Transient world exists"), World))
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (!TestNotNull(TEXT("Actor"), Actor) || !TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Widget"), Widget))
	{
		World->DestroyWorld(false);
		return false;
	}
	// RebuildWidget() -> fallback tree (StatusText); keep the Slate widget alive.
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	Controller->SetViewerActor(Actor);
	Controller->SetViewerWidget(Widget);
	const FLinearColor DefaultColor = Widget->GetStatusColor();

	// Transient profile with one known error: no Skeletal Mesh (Display Name set, so no warning).
	UCharacterProfileData* Broken = MakeProfile(World, nullptr, TEXT("Broken"));
	const TArray<FViewerProfileIssue> Issues = UCharacterProfileValidator::ValidateProfile(Broken);
	AddInfo(UCharacterProfileValidator::FormatReport(Issues));
	TestEqual(TEXT("Broken profile has exactly 1 error"), UCharacterProfileValidator::CountBySeverity(Issues, EViewerIssueSeverity::Error), 1);

	Controller->SwitchProfile(Broken);
	const FString BrokenStatus = Widget->GetCaptureStatus().ToString();
	AddInfo(FString::Printf(TEXT("Status after the broken profile: '%s' colour %s"), *BrokenStatus, *Widget->GetStatusColor().ToString()));
	TestTrue(FString::Printf(TEXT("Status contains '오류 1' (got '%s')"), *BrokenStatus), BrokenStatus.Contains(TEXT("오류 1")));
	TestTrue(TEXT("Status points to the log / section 2.10"), BrokenStatus.Contains(TEXT("2.10")));
	TestTrue(TEXT("Status line is red"), Widget->GetStatusColor().Equals(Red));

	// A clean content profile: "프로필 OK" in the status line's own colour.
	Controller->SwitchProfile(MannyContent);
	const FString OkStatus = Widget->GetCaptureStatus().ToString();
	AddInfo(FString::Printf(TEXT("Status after DA_Character_Manny: '%s'"), *OkStatus));
	TestEqual(TEXT("Manny: 프로필 OK"), OkStatus, FString(TEXT("프로필 OK")));
	TestTrue(TEXT("OK uses the default status colour"), Widget->GetStatusColor().Equals(DefaultColor));

	// A capture status afterwards resets the colour.
	Controller->SwitchProfile(Broken);
	Widget->SetCaptureStatus(FText::FromString(TEXT("Saved: x.png")));
	TestTrue(TEXT("Capture status uses the default colour again"), Widget->GetStatusColor().Equals(DefaultColor));

	Widget->RemoveFromParent();
	SlateWidget.Reset();
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
