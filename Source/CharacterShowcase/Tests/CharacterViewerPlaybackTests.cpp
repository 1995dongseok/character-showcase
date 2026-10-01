#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/CharacterViewerWidget.h"

// Animation playback controls, forced LOD and backdrop presets
// (Docs/CHARACTER_VIEWER_SETUP.md section 6.18). Loads SKM_Manny_Simple,
// MM_Idle, MI_StudioBackdrop / MI_StudioFloor and the engine sphere read-only
// into transient test worlds; nothing is modified or saved (dynamic material
// instances only).

namespace CharacterViewerPlaybackTestsPrivate
{
	const TCHAR* MannyMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* MannyIdlePath = TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle");
	const TCHAR* BackdropMIPath = TEXT("/Game/Portfolio/Materials/MI_StudioBackdrop.MI_StudioBackdrop");
	const TCHAR* FloorMIPath = TEXT("/Game/Portfolio/Materials/MI_StudioFloor.MI_StudioFloor");
	const TCHAR* SphereMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	UWorld* CreateTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
		}
		return World;
	}

	FViewerAnimationEntry MakeEntry(const TCHAR* Id, UAnimSequence* Sequence, bool bLoop, bool bIsPose = false, float PoseTime = 0.f)
	{
		FViewerAnimationEntry Entry;
		Entry.Id = FName(Id);
		Entry.DisplayName = FText::FromString(Id);
		Entry.Sequence = Sequence;
		Entry.bLoop = bLoop;
		Entry.bIsPose = bIsPose;
		Entry.PoseTime = PoseTime;
		return Entry;
	}

	FLinearColor GetVector(const UMaterialInterface* Material, const TCHAR* Name)
	{
		FLinearColor Value(-1.f, -1.f, -1.f, -1.f);
		if (Material)
		{
			Material->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(Name)), Value);
		}
		return Value;
	}
}

// Pure helpers: frame wrap, rate clamp, time line format.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerPlaybackMathTest,
	"CharacterShowcase.Viewer.PlaybackMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerPlaybackMathTest::RunTest(const FString& Parameters)
{
	using A = APortfolioCharacterActor;

	// Frames are 0..NumFrames inclusive.
	TestEqual(TEXT("Wrap(1, 36) = 1"), A::WrapAnimationFrame(1, 36), 1);
	TestEqual(TEXT("Wrap(36, 36) = 36 (last frame)"), A::WrapAnimationFrame(36, 36), 36);
	TestEqual(TEXT("Wrap(37, 36) = 0 (past the end wraps to the first frame)"), A::WrapAnimationFrame(37, 36), 0);
	TestEqual(TEXT("Wrap(-1, 36) = 36 (before the first wraps to the last)"), A::WrapAnimationFrame(-1, 36), 36);
	TestEqual(TEXT("Wrap(-38, 36) = 36"), A::WrapAnimationFrame(-38, 36), 36);
	TestEqual(TEXT("Wrap(75, 36) = 1"), A::WrapAnimationFrame(75, 36), 1);
	TestEqual(TEXT("Wrap(5, 0) = 0 (single-frame sequence)"), A::WrapAnimationFrame(5, 0), 0);
	TestEqual(TEXT("Wrap(-5, -3) = 0 (invalid count)"), A::WrapAnimationFrame(-5, -3), 0);

	TestEqual(TEXT("Clamp(1.25) = 1.25"), A::ClampAnimationPlayRate(1.25f), 1.25f);
	TestEqual(TEXT("Clamp(5) = 2.0"), A::ClampAnimationPlayRate(5.f), 2.f);
	TestEqual(TEXT("Clamp(0) = 0.1"), A::ClampAnimationPlayRate(0.f), 0.1f);
	TestEqual(TEXT("Clamp(-1) = 0.1"), A::ClampAnimationPlayRate(-1.f), 0.1f);
	TestEqual(TEXT("Clamp(NaN) = 1.0"), A::ClampAnimationPlayRate(std::numeric_limits<float>::quiet_NaN()), 1.f);

	TestEqual(TEXT("FormatPlaybackTime"), UCharacterViewerWidget::FormatPlaybackTime(0.45f, 1.2f, 14, 36), FString(TEXT("0.45 s / 1.20 s · frame 14 / 36")));

	// Backdrop preset colours: flat, White 0.8 (no clipping), Studio = no flat colour.
	FLinearColor Top, Bottom, Base, Edge;
	TestFalse(TEXT("Studio has no flat colours (originals are restored)"), ACharacterViewerController::GetBackdropPresetColors(EViewerBackdropPreset::Studio, Top, Bottom, Base, Edge));
	TestTrue(TEXT("White has flat colours"), ACharacterViewerController::GetBackdropPresetColors(EViewerBackdropPreset::White, Top, Bottom, Base, Edge));
	TestEqual(TEXT("White backdrop top = 0.8"), Top.R, 0.8f);
	TestEqual(TEXT("White floor base = 0.8"), Base.G, 0.8f);
	TestTrue(TEXT("MidGrey has flat colours"), ACharacterViewerController::GetBackdropPresetColors(EViewerBackdropPreset::MidGrey, Top, Bottom, Base, Edge));
	TestEqual(TEXT("MidGrey = 0.18"), Edge.B, 0.18f);
	TestEqual(TEXT("Display name Mid Grey"), ACharacterViewerController::GetBackdropPresetDisplayName(EViewerBackdropPreset::MidGrey), FString(TEXT("Mid Grey")));
	return true;
}

// Pause / step / rate on SKM_Manny_Simple + MM_Idle; persistence across
// animations; Pose entry; AnimBP -> false; profile switch reset; controller
// and panel PLAYBACK wiring.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerAnimationPlaybackTest,
	"CharacterShowcase.Viewer.AnimationPlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerAnimationPlaybackTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerPlaybackTestsPrivate;

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	UAnimSequence* MannyIdle = LoadObject<UAnimSequence>(nullptr, MannyIdlePath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny) || !TestNotNull(TEXT("MM_Idle loads"), MannyIdle))
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
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (!TestNotNull(TEXT("Character actor exists"), Actor) || !TestNotNull(TEXT("Viewer controller exists"), Controller)
		|| !TestNotNull(TEXT("Fallback widget exists"), Widget))
	{
		World->DestroyWorld(false);
		return false;
	}

	// No profile / no sequence: nothing is controllable, nothing changes.
	TestFalse(TEXT("No profile: not controllable"), Actor->IsAnimationPlaybackControllable());
	TestFalse(TEXT("No profile: SetAnimationPaused(true) is false"), Actor->SetAnimationPaused(true));
	TestFalse(TEXT("No profile: StepAnimationFrames is false"), Actor->StepAnimationFrames(1));
	TestFalse(TEXT("No profile: SetAnimationPlayRate is false"), Actor->SetAnimationPlayRate(0.5f));
	TestEqual(TEXT("No profile: rate stays 1.0"), Actor->GetAnimationPlayRate(), 1.f);
	float Time = 0.f, Length = 0.f;
	int32 Frame = 0, NumFrames = 0;
	TestFalse(TEXT("No profile: GetAnimationTimeInfo is false"), Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames));

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Manny;
	Profile->Animations.Add(MakeEntry(TEXT("Idle"), MannyIdle, true));
	Profile->Animations.Add(MakeEntry(TEXT("IdleOnce"), MannyIdle, false));
	Profile->Animations.Add(MakeEntry(TEXT("Pose"), MannyIdle, false, true, 0.5f));
	Profile->DefaultAnimationId = FName(TEXT("Idle"));

	Actor->ApplyProfile(Profile);
	Controller->SetViewerActor(Actor);
	TestEqual(TEXT("Default animation is Idle"), Actor->GetCurrentAnimationId(), FName(TEXT("Idle")));
	if (!TestTrue(TEXT("Idle (single node + sequence) is controllable"), Actor->IsAnimationPlaybackControllable()))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestFalse(TEXT("Not paused initially"), Actor->IsAnimationPaused());

	TestTrue(TEXT("GetAnimationTimeInfo succeeds"), Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames));
	const FFrameRate Rate = MannyIdle->GetSamplingFrameRate();
	const double Fps = Rate.AsDecimal();
	AddInfo(FString::Printf(TEXT("MM_Idle: length %.3f s, %d sampled keys, %.2f fps -> NumFrames %d, frame %d at t=%.3f"),
		MannyIdle->GetPlayLength(), MannyIdle->GetNumberOfSampledKeys(), Fps, NumFrames, Frame, Time));
	TestEqual(TEXT("Length = sequence play length"), Length, MannyIdle->GetPlayLength());
	TestEqual(TEXT("NumFrames = sampled keys - 1"), NumFrames, MannyIdle->GetNumberOfSampledKeys() - 1);
	TestTrue(TEXT("MM_Idle has more than 2 frames"), NumFrames > 2);
	TestEqual(TEXT("Starts at frame 0"), Frame, 0);

	// Step +1: pauses and lands exactly on frame 1.
	TestTrue(TEXT("Step +1 succeeds"), Actor->StepAnimationFrames(1));
	TestTrue(TEXT("Step pauses"), Actor->IsAnimationPaused());
	TestFalse(TEXT("Single node is not playing after a step"), Actor->Mesh->IsPlaying());
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("Step +1 -> frame 1"), Frame, 1);
	TestTrue(FString::Printf(TEXT("Step +1 -> time 1/fps (%.4f)"), Time), FMath::IsNearlyEqual(Time, static_cast<float>(1.0 / Fps), 1e-3f));

	// Step -2 from frame 1 wraps to the last frame; +1 from the last wraps to 0.
	TestTrue(TEXT("Step -2 succeeds"), Actor->StepAnimationFrames(-2));
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("Step -2 from frame 1 wraps to the last frame"), Frame, NumFrames);
	TestTrue(TEXT("Last frame time = play length"), FMath::IsNearlyEqual(Time, Length, 1e-3f));
	Actor->StepAnimationFrames(1);
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("Step +1 from the last frame wraps to 0"), Frame, 0);
	Actor->StepAnimationFrames(5);
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("Step +5 -> frame 5"), Frame, 5);

	// Rate clamp.
	TestTrue(TEXT("SetAnimationPlayRate(5) succeeds"), Actor->SetAnimationPlayRate(5.f));
	TestEqual(TEXT("Rate clamps to 2.0"), Actor->GetAnimationPlayRate(), 2.f);
	TestEqual(TEXT("Single node rate is 2.0"), Actor->Mesh->GetPlayRate(), 2.f);
	Actor->SetAnimationPlayRate(0.f);
	TestEqual(TEXT("Rate clamps to 0.1"), Actor->GetAnimationPlayRate(), 0.1f);
	Actor->SetAnimationPlayRate(0.5f);
	TestEqual(TEXT("Rate 0.5"), Actor->GetAnimationPlayRate(), 0.5f);

	// Persistence: paused + 0.5x survive selecting another animation, which starts at 0.
	TestTrue(TEXT("Select IdleOnce"), Actor->SetAnimation(FName(TEXT("IdleOnce"))));
	TestTrue(TEXT("Pause persists across SetAnimation"), Actor->IsAnimationPaused());
	TestFalse(TEXT("New animation is not playing (paused)"), Actor->Mesh->IsPlaying());
	TestEqual(TEXT("Rate persists across SetAnimation (actor)"), Actor->GetAnimationPlayRate(), 0.5f);
	TestEqual(TEXT("Rate persists across SetAnimation (single node)"), Actor->Mesh->GetPlayRate(), 0.5f);
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("New animation starts at frame 0"), Frame, 0);

	// Resume: playing again; a resume persists too.
	TestTrue(TEXT("Resume succeeds"), Actor->SetAnimationPaused(false));
	TestFalse(TEXT("Resumed"), Actor->IsAnimationPaused());
	TestTrue(TEXT("Single node playing after resume"), Actor->Mesh->IsPlaying());
	Actor->SetAnimation(FName(TEXT("Idle")));
	TestTrue(TEXT("Playing state persists across SetAnimation"), Actor->Mesh->IsPlaying());

	// Pose: always paused; resume refused; stepping still works.
	TestTrue(TEXT("Select Pose"), Actor->SetAnimation(FName(TEXT("Pose"))));
	TestTrue(TEXT("Pose reports paused"), Actor->IsAnimationPaused());
	TestTrue(TEXT("Pose is a pose"), Actor->IsCurrentAnimationPose());
	TestFalse(TEXT("Pose is not playing"), Actor->Mesh->IsPlaying());
	TestFalse(TEXT("Resume is refused for a Pose"), Actor->SetAnimationPaused(false));
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	TestTrue(FString::Printf(TEXT("Pose sits at PoseTime 0.5 s (%.3f)"), Time), FMath::IsNearlyEqual(Time, 0.5f, 1e-3f));
	TestTrue(TEXT("Step works on a Pose"), Actor->StepAnimationFrames(1));

	// Controller wrappers + panel PLAYBACK.
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->BindToViewer(Controller, Actor, nullptr);
	Actor->SetAnimation(FName(TEXT("Idle")));
	Actor->SetAnimationPaused(false);
	Actor->SetAnimationPlayRate(1.f);
	TestTrue(TEXT("Controller ToggleAnimationPaused pauses"), Controller->ToggleAnimationPaused());
	TestTrue(TEXT("Paused via controller"), Actor->IsAnimationPaused());
	TestTrue(TEXT("Controller ToggleAnimationPaused resumes"), Controller->ToggleAnimationPaused());
	TestFalse(TEXT("Resumed via controller"), Actor->IsAnimationPaused());
	TestTrue(TEXT("Controller rate +step"), Controller->ChangeAnimationPlayRate(Controller->AnimationRateStep));
	TestEqual(TEXT("Rate 1.25 after one '=' step"), Actor->GetAnimationPlayRate(), 1.25f);
	for (int32 Press = 0; Press < 10; ++Press)
	{
		Controller->ChangeAnimationPlayRate(Controller->AnimationRateStep);
	}
	TestEqual(TEXT("Rate stops at 2.0"), Actor->GetAnimationPlayRate(), 2.f);
	TestTrue(TEXT("Controller rate reset"), Controller->ResetAnimationPlayRate());
	TestEqual(TEXT("Rate 1.0 after '0'"), Actor->GetAnimationPlayRate(), 1.f);
	TestTrue(TEXT("Controller step forward"), Controller->StepAnimationFrames(3));
	Widget->NotifySelectionChanged();
	Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames);
	const FString ExpectedLine = UCharacterViewerWidget::FormatPlaybackTime(Time, Length, Frame, NumFrames);
	TestEqual(TEXT("Panel time line"), Widget->GetPlaybackTimeText().ToString(), ExpectedLine);
	AddInfo(FString::Printf(TEXT("PLAYBACK line: %s"), *ExpectedLine));
	TestEqual(TEXT("Pause button shows Resume while paused"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::ToggleAnimationPause, NAME_None).ToString(), FString(TEXT("Resume (P)")));
	TestEqual(TEXT("Rate button label"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::AnimationRateReset, NAME_None).ToString(), FString(TEXT("Rate 1.00 (0)")));
	// The time line follows a step without a full RefreshUI (NativeTick path).
	Actor->StepAnimationFrames(1);
	Widget->UpdatePlaybackTimeText();
	TestTrue(TEXT("Time line updated to frame 4"), Widget->GetPlaybackTimeText().ToString().Contains(FString::Printf(TEXT("frame 4 / %d"), NumFrames)));

	// Profile switch resets pause/rate.
	Actor->SetAnimationPlayRate(1.5f);
	Actor->ApplyProfile(Profile);
	TestFalse(TEXT("ApplyProfile resets pause"), Actor->IsAnimationPaused());
	TestEqual(TEXT("ApplyProfile resets rate"), Actor->GetAnimationPlayRate(), 1.f);
	TestTrue(TEXT("Default animation plays after ApplyProfile"), Actor->Mesh->IsPlaying());

	// Animation Blueprint drives the mesh: every control is rejected.
	UCharacterProfileData* AnimBPProfile = NewObject<UCharacterProfileData>(World);
	AnimBPProfile->SkeletalMesh = Manny;
	AnimBPProfile->DefaultAnimClass = UAnimInstance::StaticClass();
	Actor->ApplyProfile(AnimBPProfile);
	TestEqual(TEXT("AnimBP profile: mesh is in AnimationBlueprint mode"), (int32)Actor->Mesh->GetAnimationMode(), (int32)EAnimationMode::AnimationBlueprint);
	TestFalse(TEXT("AnimBP: not controllable"), Actor->IsAnimationPlaybackControllable());
	TestFalse(TEXT("AnimBP: SetAnimationPaused(true) is false"), Actor->SetAnimationPaused(true));
	TestFalse(TEXT("AnimBP: not paused"), Actor->IsAnimationPaused());
	TestFalse(TEXT("AnimBP: StepAnimationFrames is false"), Actor->StepAnimationFrames(1));
	TestFalse(TEXT("AnimBP: SetAnimationPlayRate is false"), Actor->SetAnimationPlayRate(0.5f));
	TestFalse(TEXT("AnimBP: GetAnimationTimeInfo is false"), Actor->GetAnimationTimeInfo(Time, Length, Frame, NumFrames));
	TestFalse(TEXT("AnimBP: controller toggle is false"), Controller->ToggleAnimationPaused());
	Widget->BindToViewer(Controller, Actor, nullptr);
	TestEqual(TEXT("AnimBP: panel time line"), Widget->GetPlaybackTimeText().ToString(), FString(TEXT("Animation Blueprint drives this mesh")));

	Widget->RemoveFromParent();
	SlateWidget.Reset();
	World->DestroyWorld(false);
	return true;
}

// Forced LOD on SKM_Manny_Simple (3 LODs): stats follow the displayed LOD,
// range checks, controller cycle, panel label, reset on ApplyProfile.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerForcedLODTest,
	"CharacterShowcase.Viewer.ForcedLOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerForcedLODTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerPlaybackTestsPrivate;

	USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMeshPath);
	if (!TestNotNull(TEXT("SKM_Manny_Simple loads"), Manny))
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
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (!TestNotNull(TEXT("Character actor exists"), Actor) || !TestNotNull(TEXT("Viewer controller exists"), Controller)
		|| !TestNotNull(TEXT("Fallback widget exists"), Widget))
	{
		World->DestroyWorld(false);
		return false;
	}

	TestEqual(TEXT("No mesh: 0 LODs"), Actor->GetNumLODs(), 0);
	TestFalse(TEXT("No mesh: SetForcedLOD(1) is false"), Actor->SetForcedLOD(1));

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Profile->SkeletalMesh = Manny;
	Actor->ApplyProfile(Profile);
	Controller->SetViewerActor(Actor);

	if (!TestEqual(TEXT("SKM_Manny_Simple has 3 LODs"), Actor->GetNumLODs(), 3))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Auto by default"), Actor->GetForcedLOD(), 0);
	const FViewerMeshStats LOD0 = Actor->GetMeshStats(0);
	const FViewerMeshStats LOD1 = Actor->GetMeshStats(1);
	const FViewerMeshStats LOD2 = Actor->GetMeshStats(2);
	AddInfo(FString::Printf(TEXT("SKM_Manny_Simple triangles: LOD0 %d, LOD1 %d, LOD2 %d; vertices LOD0 %d, LOD2 %d"),
		LOD0.Triangles, LOD1.Triangles, LOD2.Triangles, LOD0.Vertices, LOD2.Vertices));
	TestTrue(TEXT("LOD0..2 stats are valid"), LOD0.bValid && LOD1.bValid && LOD2.bValid);
	TestEqual(TEXT("LOD0 triangles 92,178"), LOD0.Triangles, 92178);
	TestTrue(TEXT("LOD1 has fewer triangles than LOD0"), LOD1.Triangles < LOD0.Triangles);
	TestTrue(TEXT("LOD2 has fewer triangles than LOD1"), LOD2.Triangles < LOD1.Triangles);
	TestEqual(TEXT("LOD2 stats report LODIndex 2"), LOD2.LODIndex, 2);
	TestFalse(TEXT("GetMeshStats(3) is invalid (no such LOD)"), Actor->GetMeshStats(3).bValid);
	TestEqual(TEXT("Auto: default stats = LOD0"), Actor->GetMeshStats().Triangles, LOD0.Triangles);

	// Force LOD2 (engine value 3).
	TestTrue(TEXT("SetForcedLOD(3) succeeds"), Actor->SetForcedLOD(3));
	TestEqual(TEXT("GetForcedLOD() = 3"), Actor->GetForcedLOD(), 3);
	TestEqual(TEXT("Component forced LOD = 3"), Actor->Mesh->GetForcedLOD(), 3);
	TestEqual(TEXT("Displayed stats LOD = 2"), Actor->GetDisplayedStatsLOD(), 2);
	const FViewerMeshStats Displayed = Actor->GetMeshStats();
	TestEqual(TEXT("Default stats follow the forced LOD (LODIndex)"), Displayed.LODIndex, 2);
	TestEqual(TEXT("Default stats follow the forced LOD (triangles)"), Displayed.Triangles, LOD2.Triangles);
	TestEqual(TEXT("LODs count unchanged"), Displayed.LODs, 3);
	const TArray<FViewerSlotStats> Slots = Actor->GetSlotStats();
	int32 SlotSum = 0;
	for (const FViewerSlotStats& SlotStats : Slots)
	{
		SlotSum += SlotStats.Triangles;
	}
	TestEqual(TEXT("Per-slot triangles at the forced LOD add up to its total"), SlotSum, LOD2.Triangles);
	int32 LOD0SlotSum = 0;
	for (const FViewerSlotStats& SlotStats : Actor->GetSlotStats(0))
	{
		LOD0SlotSum += SlotStats.Triangles;
	}
	TestEqual(TEXT("GetSlotStats(0) still sums to LOD0"), LOD0SlotSum, LOD0.Triangles);

	TestFalse(TEXT("SetForcedLOD(4) is out of range"), Actor->SetForcedLOD(4));
	TestFalse(TEXT("SetForcedLOD(-1) is out of range"), Actor->SetForcedLOD(-1));
	TestEqual(TEXT("Rejected values keep the forced LOD"), Actor->GetForcedLOD(), 3);

	// Panel: DISPLAY label + INSPECTION numbers follow the forced LOD.
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	Controller->SetInspectionEnabled(true);
	Widget->BindToViewer(Controller, Actor, nullptr);
	TestEqual(TEXT("LOD row label"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::CycleLOD, NAME_None).ToString(), FString(TEXT("▶ LOD: LOD2 of 3 (L)")));
	const FString Inspection = Widget->GetFallbackInspectionBodyText().ToString();
	TestTrue(FString::Printf(TEXT("INSPECTION shows LOD2 triangles %s"), *UCharacterViewerWidget::FormatThousands(LOD2.Triangles)),
		Inspection.Contains(FString::Printf(TEXT("Triangles %s"), *UCharacterViewerWidget::FormatThousands(LOD2.Triangles))));

	// Controller cycle: 3 -> 0 (Auto) -> 1 -> 2 -> 3 -> 0.
	const int32 Expected[] = { 0, 1, 2, 3, 0 };
	for (const int32 Value : Expected)
	{
		TestTrue(TEXT("CycleForcedLOD succeeds"), Controller->CycleForcedLOD());
		TestEqual(FString::Printf(TEXT("CycleForcedLOD -> %d"), Value), Actor->GetForcedLOD(), Value);
	}
	Widget->NotifySelectionChanged();
	TestEqual(TEXT("LOD row label (Auto)"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::CycleLOD, NAME_None).ToString(), FString(TEXT("LOD: Auto (L)")));

	// Profile switch resets to Auto.
	Actor->SetForcedLOD(2);
	Actor->ApplyProfile(Profile);
	TestEqual(TEXT("ApplyProfile resets to Auto"), Actor->GetForcedLOD(), 0);
	TestEqual(TEXT("ApplyProfile resets the component"), Actor->Mesh->GetForcedLOD(), 0);
	TestEqual(TEXT("Stats back to LOD0"), Actor->GetMeshStats().Triangles, LOD0.Triangles);

	Widget->RemoveFromParent();
	SlateWidget.Reset();
	World->DestroyWorld(false);
	return true;
}

// Backdrop presets: safe without studio actors; with a backdrop (dynamic MI
// parented to MI_StudioBackdrop) and a floor (MI_StudioFloor) the colours
// change per preset and Studio restores the authored values; kept across a
// profile switch.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerBackdropPresetTest,
	"CharacterShowcase.Viewer.BackdropPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerBackdropPresetTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerPlaybackTestsPrivate;

	UMaterialInterface* BackdropMI = LoadObject<UMaterialInterface>(nullptr, BackdropMIPath);
	UMaterialInterface* FloorMI = LoadObject<UMaterialInterface>(nullptr, FloorMIPath);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SphereMeshPath);
	if (!TestNotNull(TEXT("MI_StudioBackdrop loads"), BackdropMI) || !TestNotNull(TEXT("MI_StudioFloor loads"), FloorMI)
		|| !TestNotNull(TEXT("Sphere loads"), Sphere))
	{
		return false;
	}
	const FLinearColor AuthoredTop = GetVector(BackdropMI, TEXT("TopColor"));
	const FLinearColor AuthoredBottom = GetVector(BackdropMI, TEXT("BottomColor"));
	const FLinearColor AuthoredBase = GetVector(FloorMI, TEXT("BaseColor"));
	const FLinearColor AuthoredEdge = GetVector(FloorMI, TEXT("EdgeColor"));
	AddInfo(FString::Printf(TEXT("Authored: TopColor %s, BottomColor %s, BaseColor %s, EdgeColor %s"),
		*AuthoredTop.ToString(), *AuthoredBottom.ToString(), *AuthoredBase.ToString(), *AuthoredEdge.ToString()));
	TestTrue(TEXT("MI_StudioBackdrop has a TopColor"), AuthoredTop.R >= 0.f);
	TestTrue(TEXT("MI_StudioFloor has a BaseColor"), AuthoredBase.R >= 0.f);

	// --- A: no studio actors -> no crash, preset still recorded. ---
	{
		UWorld* World = CreateTestWorld();
		if (!TestNotNull(TEXT("Transient world A exists"), World))
		{
			return false;
		}
		World->SpawnActor<APortfolioCharacterActor>();
		ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
		if (TestNotNull(TEXT("Controller A exists"), Controller))
		{
			TestEqual(TEXT("Studio by default"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::Studio);
			Controller->SetBackdropPreset(EViewerBackdropPreset::Black);
			TestEqual(TEXT("No actors: preset recorded (Black)"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::Black);
			TestEqual(TEXT("No actors: 0 targets"), Controller->GetBackdropTargetCount(), 0);
			Controller->CycleBackdropPreset();
			TestEqual(TEXT("No actors: cycle -> White"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::White);
			Controller->CycleBackdropPreset();
			Controller->CycleBackdropPreset();
			TestEqual(TEXT("No actors: cycle wraps to Studio"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::Studio);
		}
		World->DestroyWorld(false);
	}

	// --- B: backdrop (existing dynamic MI) + floor (plain MI -> MID created). ---
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Transient world B exists"), World))
	{
		return false;
	}
	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	ACharacterViewerController* Controller = World->SpawnActor<ACharacterViewerController>();
	AStaticMeshActor* Backdrop = World->SpawnActor<AStaticMeshActor>();
	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>();
	UCharacterViewerWidget* Widget = CreateWidget<UCharacterViewerWidget>(World, UCharacterViewerWidget::StaticClass());
	if (!TestNotNull(TEXT("Actor exists"), Actor) || !TestNotNull(TEXT("Controller exists"), Controller)
		|| !TestNotNull(TEXT("Backdrop actor exists"), Backdrop) || !TestNotNull(TEXT("Floor actor exists"), Floor) || !TestNotNull(TEXT("Widget exists"), Widget))
	{
		World->DestroyWorld(false);
		return false;
	}
	UStaticMeshComponent* BackdropComp = Backdrop->GetStaticMeshComponent();
	UStaticMeshComponent* FloorComp = Floor->GetStaticMeshComponent();
	BackdropComp->SetMobility(EComponentMobility::Movable);
	FloorComp->SetMobility(EComponentMobility::Movable);
	BackdropComp->SetStaticMesh(Sphere);
	FloorComp->SetStaticMesh(Sphere);
	UMaterialInstanceDynamic* BackdropDynamic = UMaterialInstanceDynamic::Create(BackdropMI, BackdropComp);
	BackdropComp->SetMaterial(0, BackdropDynamic);
	FloorComp->SetMaterial(0, FloorMI);

	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(Profile);
	Controller->SetViewerActor(Actor);

	Controller->SetBackdropPreset(EViewerBackdropPreset::Black);
	TestEqual(TEXT("2 targets (backdrop + floor)"), Controller->GetBackdropTargetCount(), 2);
	TestEqual(TEXT("Backdrop keeps its existing dynamic MI"), BackdropComp->GetMaterial(0), static_cast<UMaterialInterface*>(BackdropDynamic));
	UMaterialInstanceDynamic* FloorDynamic = Cast<UMaterialInstanceDynamic>(FloorComp->GetMaterial(0));
	if (!TestNotNull(TEXT("Floor got a dynamic MI"), FloorDynamic))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("Floor dynamic MI parent is MI_StudioFloor"), FloorDynamic->Parent.Get(), FloorMI);

	auto ExpectColors = [&](const TCHAR* Label, const FLinearColor& Top, const FLinearColor& Bottom, const FLinearColor& Base, const FLinearColor& Edge)
	{
		TestTrue(FString::Printf(TEXT("%s: TopColor %s"), Label, *Top.ToString()), GetVector(BackdropDynamic, TEXT("TopColor")).Equals(Top, 1e-4f));
		TestTrue(FString::Printf(TEXT("%s: BottomColor %s"), Label, *Bottom.ToString()), GetVector(BackdropDynamic, TEXT("BottomColor")).Equals(Bottom, 1e-4f));
		TestTrue(FString::Printf(TEXT("%s: BaseColor %s"), Label, *Base.ToString()), GetVector(FloorDynamic, TEXT("BaseColor")).Equals(Base, 1e-4f));
		TestTrue(FString::Printf(TEXT("%s: EdgeColor %s"), Label, *Edge.ToString()), GetVector(FloorDynamic, TEXT("EdgeColor")).Equals(Edge, 1e-4f));
	};
	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);
	const FLinearColor White(0.8f, 0.8f, 0.8f, 1.f);
	const FLinearColor Grey(0.18f, 0.18f, 0.18f, 1.f);
	ExpectColors(TEXT("Black"), Black, Black, Black, Black);

	Controller->CycleBackdropPreset();
	TestEqual(TEXT("Cycle -> White"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::White);
	ExpectColors(TEXT("White"), White, White, White, White);

	Controller->CycleBackdropPreset();
	TestEqual(TEXT("Cycle -> MidGrey"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::MidGrey);
	ExpectColors(TEXT("MidGrey"), Grey, Grey, Grey, Grey);

	// Profile switch and Clean View keep the preset.
	Controller->SwitchProfile(Profile);
	TestEqual(TEXT("Profile switch keeps the preset"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::MidGrey);
	ExpectColors(TEXT("MidGrey after profile switch"), Grey, Grey, Grey, Grey);
	Controller->ToggleCleanView();
	TestEqual(TEXT("Clean View keeps the preset"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::MidGrey);
	Controller->ToggleCleanView();

	// Panel label.
	TSharedPtr<SWidget> SlateWidget = Widget->TakeWidget();
	Widget->BindToViewer(Controller, Actor, nullptr);
	TestEqual(TEXT("Backdrop row label"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::CycleBackdrop, NAME_None).ToString(), FString(TEXT("▶ Backdrop: Mid Grey (B)")));

	Controller->CycleBackdropPreset();
	TestEqual(TEXT("Cycle -> Studio"), (int32)Controller->GetBackdropPreset(), (int32)EViewerBackdropPreset::Studio);
	ExpectColors(TEXT("Studio (authored values restored)"), AuthoredTop, AuthoredBottom, AuthoredBase, AuthoredEdge);
	Widget->NotifySelectionChanged();
	TestEqual(TEXT("Backdrop row label (Studio)"), Widget->GetGeneratedButtonText(ECharacterViewerButtonKind::CycleBackdrop, NAME_None).ToString(), FString(TEXT("Backdrop: Studio (B)")));

	TestEqual(TEXT("MI_StudioBackdrop asset unchanged (TopColor)"), GetVector(BackdropMI, TEXT("TopColor")), AuthoredTop);
	TestEqual(TEXT("MI_StudioFloor asset unchanged (BaseColor)"), GetVector(FloorMI, TEXT("BaseColor")), AuthoredBase);

	Widget->RemoveFromParent();
	SlateWidget.Reset();
	World->DestroyWorld(false);
	return true;
}

#endif
