#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UI/CharacterViewerWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"

// Docs/CHARACTER_VIEWER_SETUP.md section 13 (deliverable C): drives the
// *actual* Character Portfolio Viewer end to end in a -game process (real
// GameMode/level/BeginPlay, real rendering, no NullRHI) using latent
// automation commands. Deliberately EAutomationTestFlags::ClientContext (not
// EditorContext), so this test only runs under a -game launch, never inside
// the Editor/PIE automation context that Tests/CharacterViewerTests.cpp and
// Tests/CharacterProfileTests.cpp use.
//
// Requires Content/Portfolio/Maps/LV_Portfolio (created by
// Scripts/CreatePortfolioAssets.py) to be the map the -game process is
// launched into, with exactly one APortfolioCharacterActor whose Profile is
// DA_Character (Face/Upper/Full presets, Idle/Walk/Pose animations, a
// Neutral expression, Default/Grid material variants).

namespace CharacterViewerGameSmokeTest
{
	// Shared across every latent command below; each command reads/writes it
	// as the multi-step scenario progresses.
	struct FSharedState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<APortfolioCharacterActor> Actor;
		TWeakObjectPtr<ACharacterViewerController> Controller;
		TWeakObjectPtr<ACharacterViewerCameraPawn> Pawn;
		TWeakObjectPtr<UCharacterViewerWidget> Widget;

		FViewerCameraFraming ResetFraming;
		float TurntableStartYaw = 0.f;
		TWeakObjectPtr<UMaterialInterface> OriginalSlot0Material;

		// Captured once in FValidateSceneCommand, before any turntable/orbit
		// manipulation: the actor's placed rotation (ClearRuntimeState()'s
		// InitialRotation), used by FSwitchProfileAndVerifyCommand (deliverable A,
		// section 6) to confirm ApplyProfile() resets turntable rotation on a
		// profile switch instead of comparing against a hardcoded yaw.
		FRotator RestRotation = FRotator::ZeroRotator;

		// UUserWidget's own constructor defaults Visibility to
		// SelfHitTestInvisible (not Visible; see UserWidget.cpp), so the
		// "starts visible" / "Clean View restores it" checks below compare
		// against whatever the widget's actual pre-Clean-View state was
		// instead of hardcoding ESlateVisibility::Visible -- exactly what
		// ACharacterViewerController::ToggleCleanView() itself does.
		ESlateVisibility InitialWidgetVisibility = ESlateVisibility::SelfHitTestInvisible;
	};

	static UWorld* FindGameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				if (UWorld* World = Context.World())
				{
					return World;
				}
			}
		}
		return nullptr;
	}

	// Step 1: locate the world/actor/controller/pawn/widget and assert the
	// basic scene shape (exactly one viewer actor, mesh/profile assigned,
	// controller/pawn/widget wired up).
	class FValidateSceneCommand : public IAutomationLatentCommand
	{
	public:
		FValidateSceneCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			UWorld* World = FindGameWorld();
			if (!Test->TestNotNull(TEXT("A Game/PIE world exists"), World))
			{
				return true;
			}
			State->World = World;

			int32 ActorCount = 0;
			APortfolioCharacterActor* FoundActor = nullptr;
			for (TActorIterator<APortfolioCharacterActor> It(World); It; ++It)
			{
				FoundActor = *It;
				++ActorCount;
			}
			Test->TestEqual(TEXT("Exactly one APortfolioCharacterActor exists in LV_Portfolio"), ActorCount, 1);
			if (Test->TestNotNull(TEXT("APortfolioCharacterActor was found"), FoundActor))
			{
				State->Actor = FoundActor;
				State->RestRotation = FoundActor->GetActorRotation();
				Test->TestNotNull(TEXT("Viewer actor's Mesh has a SkeletalMesh assigned"), FoundActor->Mesh ? FoundActor->Mesh->GetSkeletalMeshAsset() : nullptr);
				Test->TestNotNull(TEXT("Viewer actor's Profile is assigned"), FoundActor->Profile.Get());
			}

			APlayerController* PC = World->GetFirstPlayerController();
			ACharacterViewerController* ViewerController = Cast<ACharacterViewerController>(PC);
			if (Test->TestNotNull(TEXT("PlayerController is an ACharacterViewerController"), ViewerController))
			{
				State->Controller = ViewerController;
				Test->TestEqual(TEXT("Controller's viewer actor matches the actor found in the level"), ViewerController->GetViewerActor(), FoundActor);
				Test->TestTrue(TEXT("Controller reports input enabled"), ViewerController->IsInputEnabled());

				ACharacterViewerCameraPawn* CameraPawn = ViewerController->GetCameraPawn();
				if (Test->TestNotNull(TEXT("Controller's pawn is an ACharacterViewerCameraPawn"), CameraPawn))
				{
					State->Pawn = CameraPawn;
				}

				UCharacterViewerWidget* Widget = ViewerController->GetViewerWidget();
				if (Test->TestNotNull(TEXT("Viewer widget instance exists"), Widget))
				{
					State->Widget = Widget;
					Test->TestTrue(TEXT("Viewer widget is in the viewport"), Widget->IsInViewport());
					State->InitialWidgetVisibility = Widget->GetVisibility();
					Test->TestNotEqual(TEXT("Viewer widget does not start Collapsed"), State->InitialWidgetVisibility, ESlateVisibility::Collapsed);
				}
			}

			if (State->Actor.IsValid() && State->Actor->Mesh)
			{
				const int32 NumMaterials = State->Actor->Mesh->GetNumMaterials();
				if (NumMaterials > 0)
				{
					State->OriginalSlot0Material = State->Actor->Mesh->GetMaterial(0);
				}
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Step 2: orbit/zoom clamp, then kick off ResetCamera() (its interpolation is checked one wait later).
	class FCameraClampAndResetCommand : public IAutomationLatentCommand
	{
	public:
		FCameraClampAndResetCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerCameraPawn* Pawn = State->Pawn.Get();
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Pawn || !Controller || !Actor || !Actor->Profile)
			{
				Test->AddError(TEXT("FCameraClampAndResetCommand: missing pawn/controller/actor/profile from the previous step."));
				return true;
			}

			const FViewerCameraFraming ResetFraming = Actor->Profile->GetResetFraming();
			State->ResetFraming = ResetFraming;

			// A very large orbit delta must clamp to the current framing's pitch limits, not overshoot.
			Pawn->Orbit(FVector2D(0.f, 1000000.f));
			Test->TestEqual(TEXT("Large orbit-up clamps pitch to MaxPitch"), Pawn->GetPitch(), ResetFraming.MaxPitch, 0.5f);

			Pawn->Orbit(FVector2D(0.f, -2000000.f));
			Test->TestEqual(TEXT("Large orbit-down clamps pitch to MinPitch"), Pawn->GetPitch(), ResetFraming.MinPitch, 0.5f);

			// Zooming far beyond either limit must clamp to Min/MaxDistance.
			Pawn->Zoom(-100000.f);
			Test->TestEqual(TEXT("Zoom-out clamps distance to MaxDistance"), Pawn->GetDistance(), ResetFraming.MaxDistance, 0.5f);

			Pawn->Zoom(100000.f);
			Test->TestEqual(TEXT("Zoom-in clamps distance to MinDistance"), Pawn->GetDistance(), ResetFraming.MinDistance, 0.5f);

			Controller->ResetCamera();
			Test->TestTrue(TEXT("ResetCamera() starts a camera interpolation"), Pawn->IsInterpolating());

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Step 3 (after a 1s wait for the reset interpolation to finish): check the camera landed back on the reset framing.
	class FResetCheckCommand : public IAutomationLatentCommand
	{
	public:
		FResetCheckCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerCameraPawn* Pawn = State->Pawn.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Pawn || !Actor)
			{
				Test->AddError(TEXT("FResetCheckCommand: missing pawn/actor from the previous step."));
				return true;
			}

			Test->TestFalse(TEXT("Camera interpolation finished within 1s"), Pawn->IsInterpolating());
			Test->TestEqual(TEXT("Reset restores Distance"), Pawn->GetDistance(), State->ResetFraming.Distance, 1.f);
			Test->TestEqual(TEXT("Reset restores Pitch to 0"), Pawn->GetPitch(), 0.f, 1.f);
			Test->TestEqual(TEXT("Reset restores Yaw to 0"), FMath::UnwindDegrees(Pawn->GetYaw()), 0.f, 1.f);

			State->TurntableStartYaw = Actor->GetActorRotation().Yaw;
			Actor->SetTurntableEnabled(true);
			Test->TestTrue(TEXT("SetTurntableEnabled(true) reports enabled"), Actor->IsTurntableEnabled());
			Test->TestTrue(TEXT("Turntable enabled means actor Tick is enabled"), Actor->IsActorTickEnabled());

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Step 4 (after a 0.5s wait): turntable actually rotated the actor; then
	// exercise Animation/MaterialVariant/Expression selection. All of these
	// apply instantly (no interpolation), so they are safe to run
	// back-to-back in a single Update(). The UI screenshot is requested later
	// by FCaptureWindowScreenshotCommand.
	class FTurntableAnimMaterialExpressionCommand : public IAutomationLatentCommand
	{
	public:
		FTurntableAnimMaterialExpressionCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Actor || !Actor->Mesh || !Actor->Profile)
			{
				Test->AddError(TEXT("FTurntableAnimMaterialExpressionCommand: missing actor/mesh/profile from the previous step."));
				return true;
			}

			// --- Turntable ---
			const float YawAfterHalfSecond = Actor->GetActorRotation().Yaw;
			Test->TestTrue(TEXT("Turntable yaw changed after 0.5s"), !FMath::IsNearlyEqual(YawAfterHalfSecond, State->TurntableStartYaw, 0.1f));
			Actor->SetTurntableEnabled(false);
			Test->TestFalse(TEXT("SetTurntableEnabled(false) reports disabled"), Actor->IsTurntableEnabled());

			USkeletalMeshComponent* Mesh = Actor->Mesh;

			// --- Animation ---
			const bool bIdleSelected = Actor->SetAnimation(FName(TEXT("Idle")));
			Test->TestTrue(TEXT("SelectAnimation('Idle') returns true"), bIdleSelected);
			Test->TestEqual(TEXT("Mesh animation mode is AnimationSingleNode after selecting Idle"), Mesh->GetAnimationMode(), EAnimationMode::AnimationSingleNode);
			if (UAnimSingleNodeInstance* SingleNode = Mesh->GetSingleNodeInstance())
			{
				Test->TestNotNull(TEXT("SingleNodeInstance has a current animation asset after selecting Idle"), SingleNode->GetCurrentAsset());
			}
			else
			{
				Test->AddError(TEXT("Mesh->GetSingleNodeInstance() is null after selecting Idle"));
			}

			const bool bPoseSelected = Actor->SetAnimation(FName(TEXT("Pose")));
			Test->TestTrue(TEXT("SelectAnimation('Pose') returns true"), bPoseSelected);
			if (UAnimSingleNodeInstance* SingleNode = Mesh->GetSingleNodeInstance())
			{
				Test->TestFalse(TEXT("Mesh is not playing after selecting a still Pose"), SingleNode->IsPlaying());
			}
			else
			{
				Test->AddError(TEXT("Mesh->GetSingleNodeInstance() is null after selecting Pose"));
			}

			// --- Material variant ---
			const bool bGridSelected = Actor->SetMaterialVariant(FName(TEXT("Grid")));
			Test->TestTrue(TEXT("SelectMaterialVariant('Grid') returns true"), bGridSelected);
			UMaterialInterface* GridMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
			Test->TestEqual(TEXT("Mesh slot 0 material is WorldGridMaterial after selecting 'Grid'"), Mesh->GetMaterial(0), GridMaterial);

			const bool bDefaultSelected = Actor->SetMaterialVariant(FName(TEXT("Default")));
			Test->TestTrue(TEXT("SelectMaterialVariant('Default') returns true"), bDefaultSelected);
			if (State->OriginalSlot0Material.IsValid())
			{
				Test->TestEqual(TEXT("Mesh slot 0 material is restored after selecting 'Default'"), Mesh->GetMaterial(0), State->OriginalSlot0Material.Get());
			}
			Test->TestNotEqual(TEXT("Mesh slot 0 material is no longer WorldGridMaterial after selecting 'Default'"), Mesh->GetMaterial(0), GridMaterial);

			// --- Expression ---
			const bool bNeutralSelected = Actor->SetExpression(FName(TEXT("Neutral")));
			Test->TestTrue(TEXT("SelectExpression('Neutral') returns true"), bNeutralSelected);
			const bool bUnknownSelected = Actor->SetExpression(FName(TEXT("Nope")));
			Test->TestFalse(TEXT("SelectExpression('Nope') (unknown id) returns false without crashing"), bUnknownSelected);

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Captures the whole game window INCLUDING Slate/UMG UI and writes it to
	// Saved/Screenshots/<Platform>/<Name>.png. FScreenshotRequest with
	// bInShowUI=false only reads the 3D scene viewport (the viewer panel is
	// never in the image, so UI vs Clean View looked identical), and with
	// bInShowUI=true it failed silently in this -game setup (no file, no log),
	// so the capture is done explicitly here and every failure is reported.
	class FCaptureWindowScreenshotCommand : public IAutomationLatentCommand
	{
	public:
		FCaptureWindowScreenshotCommand(FAutomationTestBase* InTest, const TCHAR* InName)
			: Test(InTest), Name(InName)
		{
		}

		virtual bool Update() override
		{
			TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
			if (!Window.IsValid() || !FSlateApplication::IsInitialized())
			{
				Test->AddError(FString::Printf(TEXT("Screenshot '%s': no game window / Slate application to capture."), *Name));
				return true;
			}

			TArray<FColor> Bitmap;
			FIntVector Size(0, 0, 0);
			if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Bitmap, Size))
			{
				Test->AddError(FString::Printf(TEXT("Screenshot '%s': FSlateApplication::TakeScreenshot failed (window visible=%d minimized=%d size=%dx%d)."),
					*Name, Window->IsVisible() ? 1 : 0, Window->IsWindowMinimized() ? 1 : 0, Size.X, Size.Y));
				return true;
			}

			// Slate readback leaves alpha undefined; force opaque for the PNG.
			for (FColor& Pixel : Bitmap)
			{
				Pixel.A = 255;
			}

			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (Name + TEXT(".png")));
			const bool bSaved = FImageUtils::SaveImageByExtension(*Path, FImageView(Bitmap.GetData(), Size.X, Size.Y));
			if (Test->TestTrue(FString::Printf(TEXT("Screenshot '%s' saved"), *Name), bSaved))
			{
				Test->AddInfo(FString::Printf(TEXT("Screenshot '%s' (%dx%d, with UI) -> %s"), *Name, Size.X, Size.Y, *Path));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		FString Name;
	};

	// Step 5 (after a 1s wait for the UI screenshot to be captured): toggle Clean View on (the Clean screenshot is requested 1s later).
	class FCleanViewOnCommand : public IAutomationLatentCommand
	{
	public:
		FCleanViewOnCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Controller)
			{
				Test->AddError(TEXT("FCleanViewOnCommand: missing controller from the previous step."));
				return true;
			}

			Controller->ToggleCleanView();

			if (Widget)
			{
				Test->TestEqual(TEXT("Clean View collapses the viewer widget"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
			}
			Test->TestFalse(TEXT("Clean View hides the mouse cursor"), Controller->bShowMouseCursor);

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Step 6 (after a further 1s wait for the Clean View screenshot): toggle Clean View back off and check the UI is restored.
	class FCleanViewOffCommand : public IAutomationLatentCommand
	{
	public:
		FCleanViewOffCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Controller)
			{
				Test->AddError(TEXT("FCleanViewOffCommand: missing controller from the previous step."));
				return true;
			}

			Controller->ToggleCleanView();

			if (Widget)
			{
				Test->TestEqual(TEXT("Clean View restores the viewer widget's original visibility"), Widget->GetVisibility(), State->InitialWidgetVisibility);
			}
			Test->TestTrue(TEXT("Clean View restores the mouse cursor"), Controller->bShowMouseCursor);

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Step 7/8 (P1 completion evidence, Docs/CHARACTER_VIEWER_SETUP.md section 6,
	// deliverable A): switch the live viewer to a *different* profile
	// (DA_Character_Cube, then back to DA_Character) purely via
	// ACharacterViewerController::SwitchProfile() / SelectCharacterProfile() --
	// no C++/Blueprint code change between profiles -- and confirm the actor,
	// camera and widget all followed the new profile's own data. Symmetric: both
	// switches (to Cube and back to Character) run through the same command and
	// check state against whatever ProfilePath's own asset says, so "switch back
	// restores the original mesh/framing" falls out of re-running this with
	// DA_Character's path rather than needing separate before/after assertions.
	class FSwitchProfileAndVerifyCommand : public IAutomationLatentCommand
	{
	public:
		FSwitchProfileAndVerifyCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, FString InProfilePath)
			: Test(InTest), State(InState), ProfilePath(MoveTemp(InProfilePath))
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			ACharacterViewerCameraPawn* Pawn = State->Pawn.Get();
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Controller || !Actor || !Pawn)
			{
				Test->AddError(TEXT("FSwitchProfileAndVerifyCommand: missing controller/actor/pawn from a previous step."));
				return true;
			}

			UCharacterProfileData* NewProfile = LoadObject<UCharacterProfileData>(nullptr, *ProfilePath);
			if (!Test->TestNotNull(FString::Printf(TEXT("Loaded profile asset '%s'"), *ProfilePath), NewProfile))
			{
				return true;
			}

			// Exercises the same public entry point the CHARACTER section
			// fallback-UI buttons use (UCharacterViewerWidget::RequestCharacterProfile ->
			// ACharacterViewerController::SelectCharacterProfile), not SwitchProfile()
			// directly, so this also proves ProfileLibrary lookup by asset name works.
			Controller->SelectCharacterProfile(NewProfile->GetFName());

			Test->TestEqual(FString::Printf(TEXT("Actor Profile is now '%s'"), *ProfilePath), Actor->Profile.Get(), NewProfile);

			USkeletalMesh* ExpectedMesh = NewProfile->SkeletalMesh.Get();
			USkeletalMesh* ActualMesh = Actor->Mesh ? Actor->Mesh->GetSkeletalMeshAsset() : nullptr;
			Test->TestEqual(TEXT("Mesh->GetSkeletalMeshAsset() matches the switched-to profile's SkeletalMesh"), ActualMesh, ExpectedMesh);

			// ApplyProfile() clears the OLD selection unconditionally (ClearRuntimeState()),
			// but then RestoreDefaultAnimationState() immediately re-selects the NEW profile's
			// own DefaultAnimationId if it has one (DA_Character: "Idle"; DA_Character_Cube has
			// no Animations/DefaultAnimationId, so it stays None) -- mirror that logic here
			// instead of asserting NAME_None unconditionally, which is only true for a profile
			// with no default animation.
			const FName ExpectedAnimationId = (!NewProfile->DefaultAnimClass && NewProfile->DefaultAnimationId != NAME_None)
				? NewProfile->DefaultAnimationId
				: NAME_None;
			Test->TestEqual(TEXT("Animation id matches the switched-to profile's default playback state"), Actor->GetCurrentAnimationId(), ExpectedAnimationId);
			Test->TestEqual(TEXT("Expression id cleared by the profile switch"), Actor->GetCurrentExpressionId(), NAME_None);
			Test->TestEqual(TEXT("Material variant id cleared by the profile switch"), Actor->GetCurrentVariantId(), NAME_None);

			Test->TestTrue(TEXT("Turntable rotation reset to the actor's placed rotation after the profile switch"),
				Actor->GetActorRotation().Equals(State->RestRotation, 0.5f));

			const FViewerCameraFraming ExpectedFraming = NewProfile->GetResetFraming();
			const float ExpectedDistance = FMath::Clamp(ExpectedFraming.Distance, ExpectedFraming.MinDistance, ExpectedFraming.MaxDistance);
			Test->TestEqual(TEXT("Camera distance matches the switched-to profile's reset framing"), Pawn->GetDistance(), ExpectedDistance, 1.f);
			Test->TestTrue(TEXT("Camera target offset matches the switched-to profile's reset framing"),
				Pawn->GetTargetOffset().Equals(ExpectedFraming.TargetOffset, 0.5f));

			if (Widget)
			{
				Test->TestEqual(TEXT("Widget panel rebuilt: fallback DisplayName text matches the switched-to profile"),
					Widget->GetFallbackDisplayNameText().ToString(), NewProfile->DisplayName.ToString());
			}
			else
			{
				Test->AddError(TEXT("FSwitchProfileAndVerifyCommand: widget missing."));
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		FString ProfilePath;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerGameSmokeTest,
	"CharacterShowcase.Game.ViewerSmoke",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FCharacterViewerGameSmokeTest::RunTest(const FString& Parameters)
{
	using namespace CharacterViewerGameSmokeTest;

	TSharedRef<FSharedState> State = MakeShared<FSharedState>();

	// 1. Give BeginPlay/PostLogin and first-frame shader compilation time to settle.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FValidateSceneCommand(this, State));

	// 2/3. Camera clamp + Reset (checked after the interpolation has had time to finish).
	ADD_LATENT_AUTOMATION_COMMAND(FCameraClampAndResetCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FResetCheckCommand(this, State));

	// 4. Turntable is checked after it has had time to actually rotate the actor.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTurntableAnimMaterialExpressionCommand(this, State));

	// 5. Screenshots (window capture incl. Slate UI). The 3s wait lets the
	// Pose/material selection above and the (temporally accumulated)
	// lighting settle; each capture is given 1s before the next state change.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_UI")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCleanViewOnCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Clean")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCleanViewOffCommand(this, State));

	// 7/8. Profile switch (P1 completion evidence, section 6, deliverable A):
	// DA_Character -> DA_Character_Cube -> DA_Character, entirely through
	// ProfileLibrary/SelectCharacterProfile(), no code change between profiles.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSwitchProfileAndVerifyCommand(this, State, TEXT("/Game/Portfolio/Data/DA_Character_Cube.DA_Character_Cube")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Profile2")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSwitchProfileAndVerifyCommand(this, State, TEXT("/Game/Portfolio/Data/DA_Character.DA_Character")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Profile1")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
