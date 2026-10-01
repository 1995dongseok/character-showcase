#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/CharacterProfileData.h"
#include "CharacterViewer/ViewerBatchCapture.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

// Batch portfolio capture (Viewer.CaptureAll, Docs/CHARACTER_VIEWER_SETUP.md
// sections 1.8 / 6.19): the plan builder against the real DA_Character_Manny
// and transient profiles, and the runner state machine against a fake host
// (no world, no rendering, no files). Nothing is modified or saved.

namespace ViewerBatchCaptureTestsPrivate
{
	const TCHAR* MannyProfilePath = TEXT("/Game/Portfolio/Data/DA_Character_Manny.DA_Character_Manny");
	const TCHAR* RootFolder = TEXT("C:/BatchTestRoot/Saved/Screenshots/Portfolio");

	UCharacterProfileData* MakeTransientProfile(const TCHAR* Name, const UCharacterProfileData* CopyFrom)
	{
		UCharacterProfileData* Profile = NewObject<UCharacterProfileData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UCharacterProfileData::StaticClass(), FName(Name)));
		if (CopyFrom)
		{
			Profile->SkeletalMesh = CopyFrom->SkeletalMesh;
			Profile->CameraPresets = CopyFrom->CameraPresets;
			Profile->DefaultPresetId = CopyFrom->DefaultPresetId;
			Profile->MaterialVariants = CopyFrom->MaterialVariants;
			Profile->Expressions = CopyFrom->Expressions;
			Profile->Animations = CopyFrom->Animations;
		}
		return Profile;
	}

	TArray<FString> FileNames(const TArray<FViewerBatchShot>& Plan)
	{
		TArray<FString> Names;
		for (const FViewerBatchShot& Shot : Plan)
		{
			Names.Add(Shot.FileName);
		}
		return Names;
	}

	// Records every runner -> host call. Knobs make the camera/shot "busy" for
	// a number of polls and make chosen shots fail, like a slow real viewer.
	class FFakeBatchHost : public IViewerBatchCaptureHost
	{
	public:
		const FViewerBatchCaptureRunner* Runner = nullptr;
		int32 TickNo = 0;

		int32 SceneNotReadyPolls = 0;	// per profile
		int32 CameraBusyPolls = 0;		// per shot
		int32 ShotBusyPolls = 0;		// per shot
		TSet<FString> StartFailures;	// file names whose StartBatchShot returns false
		TSet<FString> WriteFailures;	// file names never "written"
		TSet<FString> ShowFailures;		// profile names ShowBatchProfile refuses

		TArray<FString> Shown;
		bool bSwitchFlagDuringShow = true;
		bool bSwitchFlagOutsideShow = false;
		TArray<FString> Applied;
		TArray<FString> Started;
		TArray<int32> ApplyTick;
		TArray<int32> StartTick;
		int32 CancelShotCalls = 0;
		TArray<FString> Statuses;
		bool bLastStatusFinal = false;
		int32 FinishedCalls = 0;
		EViewerBatchPhase PhaseAtFinish = EViewerBatchPhase::Idle;
		bool bActiveAtFinish = true;
		bool bRestoreAtFinish = false;

		virtual bool ShowBatchProfile(UCharacterProfileData* Profile) override
		{
			bSwitchFlagDuringShow &= (Runner && Runner->IsSwitchingProfile());
			Shown.Add(Profile ? Profile->GetName() : FString());
			SceneLeft = SceneNotReadyPolls;
			return Profile && !ShowFailures.Contains(Profile->GetName());
		}
		virtual bool IsBatchSceneReady() override
		{
			bSwitchFlagOutsideShow |= (Runner && Runner->IsSwitchingProfile());
			return SceneLeft-- <= 0;
		}
		virtual void ApplyBatchShot(const FViewerBatchShot& Shot) override
		{
			Applied.Add(Shot.FileName);
			ApplyTick.Add(TickNo);
			CameraLeft = CameraBusyPolls;
		}
		virtual bool IsBatchCameraSettled() const override
		{
			return CameraLeft-- <= 0;
		}
		virtual bool StartBatchShot(const FString& FilePath) override
		{
			const FString Name = FPaths::GetCleanFilename(FilePath);
			if (StartFailures.Contains(Name))
			{
				return false;
			}
			Started.Add(FilePath);
			StartTick.Add(TickNo);
			ShotLeft = ShotBusyPolls;
			return true;
		}
		virtual bool IsBatchShotRunning() const override
		{
			return ShotLeft-- > 0;
		}
		virtual bool WasBatchShotWritten(const FString& FilePath) const override
		{
			return Started.Num() > 0 && Started.Last() == FilePath && !WriteFailures.Contains(FPaths::GetCleanFilename(FilePath));
		}
		virtual void CancelBatchShot() override
		{
			++CancelShotCalls;
		}
		virtual void SetBatchStatus(const FString& Status, bool bFinal) override
		{
			Statuses.Add(Status);
			bLastStatusFinal = bFinal;
		}
		virtual void OnBatchFinished(const FViewerBatchCaptureRunner& InRunner) override
		{
			++FinishedCalls;
			PhaseAtFinish = InRunner.GetPhase();
			bActiveAtFinish = InRunner.IsActive();
			bRestoreAtFinish = InRunner.ShouldRestoreSelection();
		}

		// Polls left until the running shot reports done (set by StartBatchShot from ShotBusyPolls).
		mutable int32 ShotLeft = 0;

	private:
		int32 SceneLeft = 0;
		mutable int32 CameraLeft = 0;
	};

	// Ticks until the runner is idle (or MaxTicks); returns the distinct phases seen, in order.
	TArray<EViewerBatchPhase> RunToEnd(FViewerBatchCaptureRunner& Runner, FFakeBatchHost& Host, float DeltaSeconds, int32 MaxTicks = 5000)
	{
		TArray<EViewerBatchPhase> Phases;
		Phases.Add(Runner.GetPhase());
		for (int32 Tick = 0; Tick < MaxTicks && Runner.IsActive(); ++Tick)
		{
			++Host.TickNo;
			Runner.Tick(DeltaSeconds);
			if (Phases.Last() != Runner.GetPhase())
			{
				Phases.Add(Runner.GetPhase());
			}
		}
		return Phases;
	}
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewerBatchCapturePlanTest,
	"CharacterShowcase.Viewer.BatchCapturePlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FViewerBatchCapturePlanTest::RunTest(const FString& Parameters)
{
	using namespace ViewerBatchCaptureTestsPrivate;

	const UCharacterProfileData* Manny = LoadObject<UCharacterProfileData>(nullptr, MannyProfilePath);
	if (!TestNotNull(TEXT("DA_Character_Manny loads"), Manny))
	{
		return false;
	}

	// 1. Manny: 3 presets (Face/Upper/Full) x 2 variants (Default/Grid) = 6, preset-major.
	FViewerBatchOptions Options;
	const TArray<FViewerBatchShot> Plan = ViewerBatchCapture::BuildBatchPlan(Manny, Options);
	const TArray<FString> ExpectedNames = {
		TEXT("DA_Character_Manny_Face_Default.png"), TEXT("DA_Character_Manny_Face_Grid.png"),
		TEXT("DA_Character_Manny_Upper_Default.png"), TEXT("DA_Character_Manny_Upper_Grid.png"),
		TEXT("DA_Character_Manny_Full_Default.png"), TEXT("DA_Character_Manny_Full_Grid.png") };
	TestEqual(TEXT("Manny: 3 presets x 2 variants = 6 shots"), Plan.Num(), 6);
	TestEqual(TEXT("Manny: file names <Profile>_<Preset>_<Variant>.png in preset-major order"), FileNames(Plan), ExpectedNames);
	if (Plan.Num() == 6)
	{
		TestEqual(TEXT("Shot 2 preset id"), Plan[1].PresetId, FName(TEXT("Face")));
		TestEqual(TEXT("Shot 2 variant id"), Plan[1].VariantId, FName(TEXT("Grid")));
		TestEqual(TEXT("Shot 5 preset id"), Plan[4].PresetId, FName(TEXT("Full")));
		bool bNoExpressionOrAnimation = true;
		for (const FViewerBatchShot& Shot : Plan)
		{
			bNoExpressionOrAnimation &= (Shot.ExpressionId == NAME_None && Shot.AnimationId == NAME_None);
		}
		TestTrue(TEXT("Default options keep the expression/animation on screen (both NAME_None)"), bNoExpressionOrAnimation);
	}
	TestTrue(TEXT("The plan is deterministic (second build identical)"), ViewerBatchCapture::BuildBatchPlan(Manny, Options) == Plan);

	// 2. +poses: Manny has one pose entry ("Pose") -> +1, on the default preset (Full) and the first variant.
	FViewerBatchOptions PoseOptions;
	PoseOptions.bIncludePoses = true;
	const TArray<FViewerBatchShot> PosePlan = ViewerBatchCapture::BuildBatchPlan(Manny, PoseOptions);
	TestEqual(TEXT("Manny +poses: 6 + 1 = 7 shots"), PosePlan.Num(), 7);
	if (PosePlan.Num() == 7)
	{
		TestEqual(TEXT("Pose shot file name"), PosePlan[6].FileName, FString(TEXT("DA_Character_Manny_Full_Default_Pose.png")));
		TestEqual(TEXT("Pose shot animation id"), PosePlan[6].AnimationId, FName(TEXT("Pose")));
		TestEqual(TEXT("Pose shot uses DefaultPresetId"), PosePlan[6].PresetId, FName(TEXT("Full")));
		TestEqual(TEXT("Pose shot uses the first variant"), PosePlan[6].VariantId, FName(TEXT("Default")));
		TArray<FString> FirstSix = FileNames(PosePlan);
		FirstSix.SetNum(6);
		TestEqual(TEXT("The first 6 shots are unchanged by +poses"), FirstSix, ExpectedNames);
	}

	// 3. expressions=1 on Manny (Neutral only): nothing is multiplied, names unchanged.
	FViewerBatchOptions ExpressionOptions;
	ExpressionOptions.bIncludeExpressions = true;
	TestEqual(TEXT("Manny expressions=1 (only Neutral): still the same 6 names"), FileNames(ViewerBatchCapture::BuildBatchPlan(Manny, ExpressionOptions)), ExpectedNames);

	// 4. Two expressions: x2, expression token appended; poses go back to the first expression.
	UCharacterProfileData* TwoFaces = MakeTransientProfile(TEXT("DA_BatchTwoFaces"), Manny);
	{
		FViewerExpression Smile;
		Smile.Id = TEXT("Smile");
		TwoFaces->Expressions.Add(Smile);
	}
	const FString TwoFacesName = TwoFaces->GetName();
	FViewerBatchOptions BothOptions;
	BothOptions.bIncludeExpressions = true;
	BothOptions.bIncludePoses = true;
	const TArray<FViewerBatchShot> FacePlan = ViewerBatchCapture::BuildBatchPlan(TwoFaces, BothOptions);
	TestEqual(TEXT("2 expressions: 3 x 2 x 2 + 1 pose = 13 shots"), FacePlan.Num(), 13);
	if (FacePlan.Num() == 13)
	{
		TestEqual(TEXT("Expression shot 1 name"), FacePlan[0].FileName, TwoFacesName + TEXT("_Face_Default_Neutral.png"));
		TestEqual(TEXT("Expression shot 2 name"), FacePlan[1].FileName, TwoFacesName + TEXT("_Face_Default_Smile.png"));
		TestEqual(TEXT("Expression shot 2 id"), FacePlan[1].ExpressionId, FName(TEXT("Smile")));
		TestEqual(TEXT("Pose shot after expressions: no expression token"), FacePlan[12].FileName, TwoFacesName + TEXT("_Full_Default_Pose.png"));
		TestEqual(TEXT("Pose shot after expressions: back to the first expression"), FacePlan[12].ExpressionId, FName(TEXT("Neutral")));
	}
	TestEqual(TEXT("2 expressions but expressions=0: 6 shots"), ViewerBatchCapture::BuildBatchPlan(TwoFaces, Options).Num(), 6);

	// 5. Mesh but no presets/variants: one "Default_Default" shot (DefaultFraming, default materials).
	UCharacterProfileData* Bare = MakeTransientProfile(TEXT("DA_BatchBare"), nullptr);
	Bare->SkeletalMesh = Manny->SkeletalMesh;
	const TArray<FViewerBatchShot> BarePlan = ViewerBatchCapture::BuildBatchPlan(Bare, Options);
	TestEqual(TEXT("Mesh only: 1 shot"), BarePlan.Num(), 1);
	if (BarePlan.Num() == 1)
	{
		TestEqual(TEXT("Mesh only: <Profile>_Default_Default.png"), BarePlan[0].FileName, Bare->GetName() + TEXT("_Default_Default.png"));
		TestEqual(TEXT("Mesh only: preset NAME_None"), BarePlan[0].PresetId, FName(NAME_None));
		TestEqual(TEXT("Mesh only: variant NAME_None"), BarePlan[0].VariantId, FName(NAME_None));
	}

	// 6. Empty / null profile: nothing to photograph.
	const UCharacterProfileData* Empty = MakeTransientProfile(TEXT("DA_BatchEmpty"), nullptr);
	TestEqual(TEXT("Empty profile (no mesh) -> 0 shots"), ViewerBatchCapture::BuildBatchPlan(Empty, BothOptions).Num(), 0);
	TestEqual(TEXT("Null profile -> 0 shots"), ViewerBatchCapture::BuildBatchPlan(nullptr, BothOptions).Num(), 0);

	// 7. Unsafe ids are sanitized and repeated names get a suffix.
	UCharacterProfileData* Messy = MakeTransientProfile(TEXT("DA_BatchMessy"), nullptr);
	Messy->SkeletalMesh = Manny->SkeletalMesh;
	{
		FViewerCameraPreset A;
		A.Id = TEXT("Upper Body");
		FViewerCameraPreset B;
		B.Id = TEXT("Upper:Body");
		Messy->CameraPresets = { A, B };
	}
	const TArray<FViewerBatchShot> MessyPlan = ViewerBatchCapture::BuildBatchPlan(Messy, Options);
	TestEqual(TEXT("Sanitized + de-duplicated names"), FileNames(MessyPlan),
		TArray<FString>({ Messy->GetName() + TEXT("_Upper_Body_Default.png"), Messy->GetName() + TEXT("_Upper_Body_Default_2.png") }));

	// 8. Folder name and console arguments.
	const FDateTime Time(2026, 10, 1, 14, 5, 9);
	TestEqual(TEXT("Batch folder: Batch_<Profile>_<timestamp>"), ViewerBatchCapture::MakeBatchFolderName(TEXT("DA_Character_Manny"), Time),
		FString(TEXT("Batch_DA_Character_Manny_20261001-140509")));

	TArray<FString> Errors;
	FViewerBatchOptions Parsed = ViewerBatchCapture::ParseConsoleArgs({ TEXT("profile=all"), TEXT("expressions=1"), TEXT("poses=0"), TEXT("QUIT=1") }, Errors);
	TestTrue(TEXT("profile=all"), Parsed.bAllProfiles);
	TestTrue(TEXT("expressions=1"), Parsed.bIncludeExpressions);
	TestFalse(TEXT("poses=0"), Parsed.bIncludePoses);
	TestTrue(TEXT("QUIT=1 (keys are case-insensitive)"), Parsed.bQuitWhenDone);
	TestEqual(TEXT("No argument errors"), Errors.Num(), 0);

	Errors.Reset();
	Parsed = ViewerBatchCapture::ParseConsoleArgs({ TEXT("profile=DA_Character"), TEXT("poses=yes"), TEXT("bogus") }, Errors);
	TestFalse(TEXT("profile=<name> is not 'all'"), Parsed.bAllProfiles);
	TestEqual(TEXT("profile=<name>"), Parsed.ProfileAssetName, FName(TEXT("DA_Character")));
	TestFalse(TEXT("Invalid poses value is ignored"), Parsed.bIncludePoses);
	TestEqual(TEXT("Two invalid arguments reported"), Errors.Num(), 2);

	Errors.Reset();
	Parsed = ViewerBatchCapture::ParseConsoleArgs({}, Errors);
	TestTrue(TEXT("No arguments: current profile, nothing extra, no quit"),
		Parsed.ProfileAssetName == NAME_None && !Parsed.bAllProfiles && !Parsed.bIncludeExpressions && !Parsed.bIncludePoses && !Parsed.bQuitWhenDone);
	return true;
}

// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FViewerBatchCaptureRunnerTest,
	"CharacterShowcase.Viewer.BatchCaptureRunner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FViewerBatchCaptureRunnerTest::RunTest(const FString& Parameters)
{
	using namespace ViewerBatchCaptureTestsPrivate;

	UCharacterProfileData* Manny = LoadObject<UCharacterProfileData>(nullptr, MannyProfilePath);
	if (!TestNotNull(TEXT("DA_Character_Manny loads"), Manny))
	{
		return false;
	}
	// The failure scenarios below log on purpose (B: 1 shot not written + 1
	// not started -> 2 "not written" lines and 1 "could not start", E: 1
	// profile not shown; B/F: 2 profiles without a mesh skipped).
	AddExpectedError(TEXT("Shot not written"), EAutomationExpectedErrorFlags::Contains, 2, false);
	AddExpectedError(TEXT("Could not start the shot"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedError(TEXT("Could not show profile"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedMessage(TEXT("has nothing to shoot"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2, false);

	const FDateTime Time(2026, 10, 1, 12, 0, 0);
	const FString MannyFolder = FString(RootFolder) / TEXT("Batch_DA_Character_Manny_20261001-120000");
	constexpr float Dt = 0.25f;

	// A. Full run, Manny, current-profile batch: every transition in order,
	//    settle frames + camera wait before every shot, all 6 files "written".
	{
		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		Host->Runner = &Runner;
		Host->SceneNotReadyPolls = 2;
		Host->CameraBusyPolls = 3;
		Host->ShotBusyPolls = 2;

		FViewerBatchOptions Options;
		Options.ProfileSettleSeconds = 1.0f;
		TestTrue(TEXT("A: Start succeeds"), Runner.Start(Host, { Manny }, Options, RootFolder, Time, 2));
		TestTrue(TEXT("A: active after Start"), Runner.IsActive());
		TestEqual(TEXT("A: total shots"), Runner.GetTotalShots(), 6);
		TestEqual(TEXT("A: first phase"), Runner.GetPhase(), EViewerBatchPhase::ShowingProfile);
		TestEqual(TEXT("A: progress text"), Runner.GetProgressText(), FString(TEXT("Batch 1/6")));
		TestEqual(TEXT("A: Start does not show the profile yet"), Host->Shown.Num(), 0);
		TestFalse(TEXT("A: a second Start while active is refused"), Runner.Start(Host, { Manny }, Options, RootFolder, Time, 2));

		++Host->TickNo;
		Runner.Tick(Dt);
		TestEqual(TEXT("A: tick 1 shows the profile"), Host->Shown, TArray<FString>({ TEXT("DA_Character_Manny") }));
		TestTrue(TEXT("A: IsSwitchingProfile() is true inside ShowBatchProfile"), Host->bSwitchFlagDuringShow);
		TestFalse(TEXT("A: ...and false afterwards"), Runner.IsSwitchingProfile());
		TestEqual(TEXT("A: then waits for the profile to settle"), Runner.GetPhase(), EViewerBatchPhase::ProfileSettling);
		for (int32 i = 0; i < 3; ++i)
		{
			++Host->TickNo;
			Runner.Tick(Dt);
		}
		TestEqual(TEXT("A: still settling after 0.75 s"), Runner.GetPhase(), EViewerBatchPhase::ProfileSettling);
		for (int32 i = 0; i < 2; ++i)	// 1.0 s reached, but the scene reports "not ready" twice
		{
			++Host->TickNo;
			Runner.Tick(Dt);
		}
		TestEqual(TEXT("A: waits while the scene is not ready (shaders)"), Runner.GetPhase(), EViewerBatchPhase::ProfileSettling);
		++Host->TickNo;
		Runner.Tick(Dt);
		TestEqual(TEXT("A: scene ready -> first shot"), Runner.GetPhase(), EViewerBatchPhase::ApplyingShot);
		TestEqual(TEXT("A: nothing applied before the profile settled"), Host->Applied.Num(), 0);

		const TArray<EViewerBatchPhase> Phases = RunToEnd(Runner, *Host, Dt);
		TArray<EViewerBatchPhase> Expected;
		for (int32 Shot = 0; Shot < 6; ++Shot)
		{
			Expected.Append({ EViewerBatchPhase::ApplyingShot, EViewerBatchPhase::ShotSettling, EViewerBatchPhase::Capturing });
		}
		Expected.Add(EViewerBatchPhase::Finished);
		TestTrue(TEXT("A: phases ApplyingShot -> ShotSettling -> Capturing per shot, then Finished"), Phases == Expected);

		TestFalse(TEXT("A: idle at the end"), Runner.IsActive());
		TestEqual(TEXT("A: 6 written"), Runner.GetWrittenShots(), 6);
		TestEqual(TEXT("A: 0 failed"), Runner.GetFailedShots(), 0);
		TestEqual(TEXT("A: 6 applied"), Host->Applied.Num(), 6);
		if (Host->Started.Num() == 6)
		{
			TestEqual(TEXT("A: first file path"), Host->Started[0], MannyFolder / TEXT("DA_Character_Manny_Face_Default.png"));
			TestEqual(TEXT("A: last file path"), Host->Started[5], MannyFolder / TEXT("DA_Character_Manny_Full_Grid.png"));
		}
		else
		{
			AddError(FString::Printf(TEXT("A: expected 6 started shots, got %d"), Host->Started.Num()));
		}
		bool bWaitedEveryShot = Host->ApplyTick.Num() == Host->StartTick.Num();
		for (int32 i = 0; bWaitedEveryShot && i < Host->ApplyTick.Num(); ++i)
		{
			// apply tick t, 2 settle ticks, 3 "camera busy" polls, settled on the 4th poll.
			bWaitedEveryShot &= (Host->StartTick[i] - Host->ApplyTick[i] == 2 + 3 + 1);
		}
		TestTrue(TEXT("A: every shot waited SettleFrames (2) + the camera (3 busy polls) before it was requested"), bWaitedEveryShot);
		TestEqual(TEXT("A: written files listed"), Runner.GetWrittenFiles().Num(), 6);
		TestEqual(TEXT("A: one output folder"), Runner.GetOutputFolders(), TArray<FString>({ MannyFolder }));
		TestEqual(TEXT("A: OnBatchFinished exactly once"), Host->FinishedCalls, 1);
		TestEqual(TEXT("A: finished as Finished"), Host->PhaseAtFinish, EViewerBatchPhase::Finished);
		TestFalse(TEXT("A: no longer active inside OnBatchFinished"), Host->bActiveAtFinish);
		TestTrue(TEXT("A: selection is restored"), Host->bRestoreAtFinish);
		TestEqual(TEXT("A: no shot cancelled"), Host->CancelShotCalls, 0);
		TestTrue(TEXT("A: progress status 'Batch 3/6' was shown"), Host->Statuses.Contains(TEXT("Batch 3/6")));
		TestTrue(TEXT("A: final status is the summary"), Host->bLastStatusFinal && Host->Statuses.Num() > 0 && Host->Statuses.Last().StartsWith(TEXT("Batch saved 6/6")));
		TestFalse(TEXT("A: IsSwitchingProfile() never true outside ShowBatchProfile"), Host->bSwitchFlagOutsideShow);

		const int32 CallsBefore = Host->Applied.Num() + Host->Started.Num() + Host->Shown.Num();
		Runner.Tick(Dt);
		Runner.Cancel();
		TestEqual(TEXT("A: Tick/Cancel after the end call nothing"), Host->Applied.Num() + Host->Started.Num() + Host->Shown.Num() + Host->FinishedCalls, CallsBefore + 1);
	}

	// B. profile=all-like list: Manny (+poses = 7), an empty profile (skipped),
	//    a mesh-only profile (1 shot), Manny again (duplicate, skipped).
	//    One shot fails to start, one is never written.
	{
		UCharacterProfileData* Empty = MakeTransientProfile(TEXT("DA_BatchRunnerEmpty"), nullptr);
		UCharacterProfileData* Bare = MakeTransientProfile(TEXT("DA_BatchRunnerBare"), nullptr);
		Bare->SkeletalMesh = Manny->SkeletalMesh;

		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		Host->Runner = &Runner;
		Host->ShotBusyPolls = 1;
		Host->WriteFailures.Add(TEXT("DA_Character_Manny_Face_Grid.png"));
		Host->StartFailures.Add(Bare->GetName() + TEXT("_Default_Default.png"));

		FViewerBatchOptions Options;
		Options.bIncludePoses = true;
		Options.ProfileSettleSeconds = 0.5f;
		TestTrue(TEXT("B: Start succeeds"), Runner.Start(Host, { Manny, Empty, nullptr, Bare, Manny }, Options, RootFolder, Time, 1));
		TestEqual(TEXT("B: 2 jobs (empty, null and duplicate skipped)"), Runner.GetJobs().Num(), 2);
		TestEqual(TEXT("B: total = 7 + 1"), Runner.GetTotalShots(), 8);
		RunToEnd(Runner, *Host, Dt);
		TestEqual(TEXT("B: profiles shown in order"), Host->Shown, TArray<FString>({ TEXT("DA_Character_Manny"), Bare->GetName() }));
		TestEqual(TEXT("B: 6 written"), Runner.GetWrittenShots(), 6);
		TestEqual(TEXT("B: 2 failed (1 not written, 1 not started)"), Runner.GetFailedShots(), 2);
		TestEqual(TEXT("B: finished (some files were written)"), Runner.GetPhase(), EViewerBatchPhase::Finished);
		TestTrue(TEXT("B: summary mentions the failures"), Runner.GetSummaryText().StartsWith(TEXT("Batch saved 6/8, 2 failed")));
		TestTrue(TEXT("B: pose shot was taken"), Host->Started.Contains(MannyFolder / TEXT("DA_Character_Manny_Full_Default_Pose.png")));
		TestEqual(TEXT("B: one output folder (the bare profile wrote nothing)"), Runner.GetOutputFolders().Num(), 1);
		TestEqual(TEXT("B: OnBatchFinished once"), Host->FinishedCalls, 1);
	}

	// C. Cancel (Esc) while shot 2 is being written: the running shot is
	//    stopped, OnBatchFinished once, files already written are counted.
	{
		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		Host->Runner = &Runner;
		Host->ShotBusyPolls = 1000;	// the shot "never finishes" until cancelled

		FViewerBatchOptions Options;
		Options.ProfileSettleSeconds = 0.f;
		TestTrue(TEXT("C: Start succeeds"), Runner.Start(Host, { Manny }, Options, RootFolder, Time, 0));
		for (int32 Tick = 0; Tick < 100 && Host->Started.Num() < 1; ++Tick)
		{
			Runner.Tick(Dt);
		}
		Host->ShotLeft = 0;	// shot 1 completes on the next poll, shot 2 hangs
		Runner.Tick(Dt);
		for (int32 Tick = 0; Tick < 100 && Host->Started.Num() < 2; ++Tick)
		{
			Runner.Tick(Dt);
		}
		TestEqual(TEXT("C: capturing shot 2"), Runner.GetPhase(), EViewerBatchPhase::Capturing);
		TestEqual(TEXT("C: progress 'Batch 2/6'"), Runner.GetProgressText(), FString(TEXT("Batch 2/6")));
		Runner.Cancel();
		TestEqual(TEXT("C: Cancelled"), Runner.GetPhase(), EViewerBatchPhase::Cancelled);
		TestFalse(TEXT("C: not active"), Runner.IsActive());
		TestEqual(TEXT("C: the running shot was cancelled"), Host->CancelShotCalls, 1);
		TestEqual(TEXT("C: OnBatchFinished once"), Host->FinishedCalls, 1);
		TestEqual(TEXT("C: finished phase seen by the host"), Host->PhaseAtFinish, EViewerBatchPhase::Cancelled);
		TestTrue(TEXT("C: selection restored on Esc"), Host->bRestoreAtFinish);
		TestEqual(TEXT("C: 1 written before the cancel"), Runner.GetWrittenShots(), 1);
		TestEqual(TEXT("C: summary"), Runner.GetSummaryText(), FString::Printf(TEXT("Batch cancelled (1/6 saved) -> %s"), *MannyFolder));
		const int32 Started = Host->Started.Num();
		Runner.Tick(Dt);
		Runner.Cancel();
		TestEqual(TEXT("C: nothing more after the cancel"), Host->Started.Num() + Host->FinishedCalls + Host->CancelShotCalls, Started + 2);
	}

	// D. Cancel(false) (EndPlay) before any shot: no restore, no shot to stop.
	{
		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		Host->Runner = &Runner;
		TestTrue(TEXT("D: Start succeeds"), Runner.Start(Host, { Manny }, FViewerBatchOptions(), RootFolder, Time, 4));
		Runner.Tick(Dt);
		Runner.Cancel(false);
		TestEqual(TEXT("D: Cancelled"), Runner.GetPhase(), EViewerBatchPhase::Cancelled);
		TestFalse(TEXT("D: no restore on EndPlay"), Host->bRestoreAtFinish);
		TestEqual(TEXT("D: no shot to cancel"), Host->CancelShotCalls, 0);
		TestEqual(TEXT("D: OnBatchFinished once"), Host->FinishedCalls, 1);
	}

	// E. Nothing written at all -> Failed; a profile that cannot be shown counts its shots as failed.
	{
		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		Host->Runner = &Runner;
		Host->ShowFailures.Add(TEXT("DA_Character_Manny"));
		TestTrue(TEXT("E: Start succeeds"), Runner.Start(Host, { Manny }, FViewerBatchOptions(), RootFolder, Time, 0));
		RunToEnd(Runner, *Host, Dt);
		TestEqual(TEXT("E: Failed"), Runner.GetPhase(), EViewerBatchPhase::Failed);
		TestEqual(TEXT("E: all 6 failed"), Runner.GetFailedShots(), 6);
		TestEqual(TEXT("E: nothing applied"), Host->Applied.Num(), 0);
		TestEqual(TEXT("E: summary"), Runner.GetSummaryText(), FString(TEXT("Batch failed (0/6 saved, see log)")));
		TestEqual(TEXT("E: OnBatchFinished once"), Host->FinishedCalls, 1);
	}

	// F. Start refuses an empty plan without touching the host.
	{
		FViewerBatchCaptureRunner Runner;
		const TSharedRef<FFakeBatchHost> Host = MakeShared<FFakeBatchHost>();
		UCharacterProfileData* Empty = MakeTransientProfile(TEXT("DA_BatchRunnerEmpty2"), nullptr);
		TestFalse(TEXT("F: no profiles -> Start fails"), Runner.Start(Host, {}, FViewerBatchOptions(), RootFolder, Time, 0));
		TestFalse(TEXT("F: only an empty profile -> Start fails"), Runner.Start(Host, { Empty, nullptr }, FViewerBatchOptions(), RootFolder, Time, 0));
		TestFalse(TEXT("F: still idle"), Runner.IsActive());
		TestEqual(TEXT("F: phase Idle"), Runner.GetPhase(), EViewerBatchPhase::Idle);
		TestEqual(TEXT("F: host untouched"), Host->Statuses.Num() + Host->FinishedCalls, 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
