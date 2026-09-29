#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "Components/Border.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
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

		// Absolute (desktop) Slate position of the last synthesized pointer
		// event (FSynthPointerMoveCommand), reused by FSynthLeftButtonCommand.
		FVector2D SynthPointerPos = FVector2D::ZeroVector;

		// Scratch values for the P2 UMG-pass input-boundary checks below
		// (wheel-over-panel / drag-over-panel / focus loss): captured by
		// FCaptureCameraStateCommand, compared by FCheckCameraDistanceChangedCommand
		// / FCheckCameraYawChangedCommand.
		float CapturedDistance = 0.f;
		float CapturedYaw = 0.f;

		// Was a previously-reported "the -game window must be active/unobscured"
		// precondition failure already logged this run? Kept on FSharedState
		// (constructed fresh per RunTest()) instead of a function-local `static`
		// so a second RunTest() in the same process (e.g. the test re-run by
		// hand, or a future multi-run harness) reports the precondition failure
		// again instead of silently staying quiet forever after the first run
		// in that process ever hit it once.
		bool bPreconditionReported = false;
	};

	static const FName TorsoPartId(TEXT("Torso"));
	static const FName HeadPartId(TEXT("Head"));

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

	// --- P2 Inspection / Wireframe (Docs/CHARACTER_VIEWER_SETUP.md section 7/13.11) ---

	// Turns Inspection on (no part selected yet) and checks the fallback
	// panel's INSPECTION section box is actually Visible and shows the
	// "Click a part" placeholder (P2-2).
	class FToggleInspectionOnCommand : public IAutomationLatentCommand
	{
	public:
		FToggleInspectionOnCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Controller)
			{
				Test->AddError(TEXT("FToggleInspectionOnCommand: missing controller from a previous step."));
				return true;
			}

			Controller->ToggleInspection();
			Test->TestTrue(TEXT("ToggleInspection() enables inspection"), Controller->IsInspectionEnabled());
			if (Test->TestNotNull(TEXT("Widget exists for the INSPECTION section check"), Widget))
			{
				Test->TestTrue(TEXT("Widget reports Inspection enabled"), Widget->IsInspectionEnabled());
				Test->TestEqual(TEXT("Fallback INSPECTION section box is Visible right after Inspection on"),
					Widget->GetFallbackInspectionSectionVisibility(), ESlateVisibility::Visible);
				Test->TestEqual(TEXT("Fallback INSPECTION body shows 'Click a part' before any selection"),
					Widget->GetFallbackInspectionBodyText().ToString(), FString(TEXT("Click a part")));
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Projects (actor location + Z 120, roughly the mannequin's torso) to
	// screen space and clicks it via InspectAtScreenPosition -- the same
	// world-space trace helper the real click path (HandleOrbitPressCompleted)
	// uses, so there is a single inspection code path under test.
	class FInspectTorsoCommand : public IAutomationLatentCommand
	{
	public:
		FInspectTorsoCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor)
			{
				Test->AddError(TEXT("FInspectTorsoCommand: missing controller/actor from a previous step."));
				return true;
			}

			const FVector TorsoWorldLocation = Actor->GetActorLocation() + FVector(0.f, 0.f, 120.f);
			FVector2D ScreenPos;
			if (!Test->TestTrue(TEXT("ProjectWorldLocationToScreen succeeds for the torso point"), Controller->ProjectWorldLocationToScreen(TorsoWorldLocation, ScreenPos)))
			{
				return true;
			}

			const bool bSelected = Controller->InspectAtScreenPosition(ScreenPos);
			Test->TestTrue(TEXT("InspectAtScreenPosition(torso) returns true (a part was hit and mapped)"), bSelected);
			Test->TestEqual(TEXT("Selected part is 'Torso'"), Actor->GetSelectedPartId(), TorsoPartId);
			if (Actor->Mesh)
			{
				Test->TestNotNull(TEXT("Mesh OverlayMaterial is set (selection highlight) after selecting a part"), Actor->Mesh->GetOverlayMaterial());
				Test->TestTrue(TEXT("Mesh Custom Depth is on after selecting a part"), Actor->Mesh->bRenderCustomDepth != 0);
			}

			// P2-2: the fallback INSPECTION section body actually shows the
			// selected part's authored DisplayName (not just the data getter).
			UCharacterViewerWidget* Widget = State->Widget.Get();
			const FViewerPartInfo* TorsoInfo = Actor->Profile ? Actor->Profile->FindPart(TorsoPartId) : nullptr;
			if (Test->TestNotNull(TEXT("Widget exists for the INSPECTION body check"), Widget)
				&& Test->TestNotNull(TEXT("Profile has a 'Torso' part"), TorsoInfo))
			{
				const FString Body = Widget->GetFallbackInspectionBodyText().ToString();
				Test->TestEqual(TEXT("Fallback INSPECTION section box is Visible with a part selected"),
					Widget->GetFallbackInspectionSectionVisibility(), ESlateVisibility::Visible);
				Test->TestTrue(FString::Printf(TEXT("Fallback INSPECTION body contains the Torso DisplayName '%s' (body: '%s')"), *TorsoInfo->DisplayName.ToString(), *Body),
					!TorsoInfo->DisplayName.IsEmpty() && Body.Contains(TorsoInfo->DisplayName.ToString()));
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Clicking empty space (top-left corner, above/beside the mannequin) must clear the selection.
	class FInspectEmptySpaceCommand : public IAutomationLatentCommand
	{
	public:
		FInspectEmptySpaceCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor)
			{
				Test->AddError(TEXT("FInspectEmptySpaceCommand: missing controller/actor from a previous step."));
				return true;
			}

			const bool bSelected = Controller->InspectAtScreenPosition(FVector2D(5.f, 5.f));
			Test->TestFalse(TEXT("InspectAtScreenPosition(empty space) returns false"), bSelected);
			Test->TestEqual(TEXT("Selection cleared after clicking empty space"), Actor->GetSelectedPartId(), NAME_None);
			if (Actor->Mesh)
			{
				Test->TestNull(TEXT("Mesh OverlayMaterial cleared after clicking empty space"), Actor->Mesh->GetOverlayMaterial());
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Turns Wireframe on and checks every material slot became the profile's WireframeMaterial (M_Wireframe).
	class FToggleWireframeOnCommand : public IAutomationLatentCommand
	{
	public:
		FToggleWireframeOnCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor || !Actor->Mesh || !Actor->Profile)
			{
				Test->AddError(TEXT("FToggleWireframeOnCommand: missing controller/actor/mesh/profile from a previous step."));
				return true;
			}

			const bool bToggled = Controller->ToggleWireframe();
			Test->TestTrue(TEXT("ToggleWireframe() (on) returns true (profile has a WireframeMaterial)"), bToggled);
			Test->TestTrue(TEXT("Actor reports Wireframe enabled"), Actor->IsWireframeEnabled());

			UMaterialInterface* WireframeMat = Actor->Profile->WireframeMaterial;
			const int32 NumMaterials = Actor->Mesh->GetNumMaterials();
			for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
			{
				Test->TestEqual(FString::Printf(TEXT("Mesh slot %d material is the WireframeMaterial"), SlotIndex), Actor->Mesh->GetMaterial(SlotIndex), WireframeMat);
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Selecting a MaterialVariant while Wireframe is on must NOT change what is
	// visually shown (Wireframe wins); the variant selection is only recorded
	// (Docs/CHARACTER_VIEWER_SETUP.md section 13.11 precedence).
	class FSelectGridWhileWireframeCommand : public IAutomationLatentCommand
	{
	public:
		FSelectGridWhileWireframeCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor || !Actor->Mesh || !Actor->Profile)
			{
				Test->AddError(TEXT("FSelectGridWhileWireframeCommand: missing controller/actor/mesh/profile from a previous step."));
				return true;
			}

			Controller->SelectMaterialVariant(FName(TEXT("Grid")));
			Test->TestEqual(TEXT("Variant id is recorded even though Wireframe is still visually shown"), Actor->GetCurrentVariantId(), FName(TEXT("Grid")));
			Test->TestTrue(TEXT("Wireframe stays enabled while a variant is selected"), Actor->IsWireframeEnabled());

			UMaterialInterface* WireframeMat = Actor->Profile->WireframeMaterial;
			Test->TestEqual(TEXT("Mesh slot 0 material is still the WireframeMaterial (precedence: Wireframe over Variant)"), Actor->Mesh->GetMaterial(0), WireframeMat);

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Turning Wireframe off must restore the Grid variant that was selected while it was on.
	class FToggleWireframeOffCommand : public IAutomationLatentCommand
	{
	public:
		FToggleWireframeOffCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor || !Actor->Mesh)
			{
				Test->AddError(TEXT("FToggleWireframeOffCommand: missing controller/actor/mesh from a previous step."));
				return true;
			}

			const bool bToggled = Controller->ToggleWireframe();
			Test->TestTrue(TEXT("ToggleWireframe() (off) returns true"), bToggled);
			Test->TestFalse(TEXT("Actor reports Wireframe disabled"), Actor->IsWireframeEnabled());

			UMaterialInterface* GridMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
			Test->TestEqual(TEXT("Mesh slot 0 material equals the Grid variant material after Wireframe off"), Actor->Mesh->GetMaterial(0), GridMaterial);

			Controller->SelectMaterialVariant(FName(TEXT("Default")));

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Selects the torso again, then checks Clean View hides the highlight
	// (overlay + Custom Depth), ignores inspection clicks while it is on, and
	// keeps the highlight hidden even if the Actor's selection changes.
	class FInspectThenCleanViewCommand : public IAutomationLatentCommand
	{
	public:
		FInspectThenCleanViewCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor || !Actor->Mesh)
			{
				Test->AddError(TEXT("FInspectThenCleanViewCommand: missing controller/actor/mesh from a previous step."));
				return true;
			}

			const FVector TorsoWorldLocation = Actor->GetActorLocation() + FVector(0.f, 0.f, 120.f);
			FVector2D ScreenPos;
			Controller->ProjectWorldLocationToScreen(TorsoWorldLocation, ScreenPos);
			Controller->InspectAtScreenPosition(ScreenPos);
			Test->TestEqual(TEXT("Torso re-selected before Clean View"), Actor->GetSelectedPartId(), FName(TEXT("Torso")));
			Test->TestNotNull(TEXT("Overlay material set before Clean View"), Actor->Mesh->GetOverlayMaterial());

			Controller->ToggleCleanView();
			Test->TestNull(TEXT("Clean View removes the selection highlight overlay"), Actor->Mesh->GetOverlayMaterial());
			Test->TestFalse(TEXT("Clean View turns Custom Depth off"), Actor->Mesh->bRenderCustomDepth != 0);
			Test->TestFalse(TEXT("Actor reports highlight hidden during Clean View"), Actor->IsHighlightVisible());
			// The selection id itself is preserved (only the visual highlight is hidden), so it can be restored exactly.
			Test->TestEqual(TEXT("Clean View does not clear the selection id"), Actor->GetSelectedPartId(), TorsoPartId);

			// Inspection clicks are ignored during Clean View (section 4 / 13.11.3):
			// neither a part click nor an empty-space click changes the selection
			// or brings the highlight back.
			const bool bTorsoClick = Controller->InspectAtScreenPosition(ScreenPos);
			Test->TestFalse(TEXT("InspectAtScreenPosition(torso) is ignored (returns false) during Clean View"), bTorsoClick);
			Controller->InspectAtScreenPosition(FVector2D(5.f, 5.f));
			Test->TestEqual(TEXT("Empty-space click during Clean View does not clear the selection"), Actor->GetSelectedPartId(), TorsoPartId);
			Test->TestNull(TEXT("No highlight overlay after clicks during Clean View"), Actor->Mesh->GetOverlayMaterial());

			// Actor-level guarantee: even if the selection changes while the
			// highlight is hidden, nothing is shown until Clean View is left.
			Actor->SetSelectedPart(HeadPartId);
			Test->TestEqual(TEXT("Actor selection can change while the highlight is hidden"), Actor->GetSelectedPartId(), HeadPartId);
			Test->TestNull(TEXT("SetSelectedPart during Clean View keeps the overlay hidden"), Actor->Mesh->GetOverlayMaterial());
			Test->TestFalse(TEXT("SetSelectedPart during Clean View keeps Custom Depth off"), Actor->Mesh->bRenderCustomDepth != 0);
			Actor->SetSelectedPart(TorsoPartId);
			Test->TestNull(TEXT("Re-selecting Torso during Clean View keeps the overlay hidden"), Actor->Mesh->GetOverlayMaterial());

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	class FLeaveCleanViewRestoresHighlightCommand : public IAutomationLatentCommand
	{
	public:
		FLeaveCleanViewRestoresHighlightCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller || !Actor || !Actor->Mesh)
			{
				Test->AddError(TEXT("FLeaveCleanViewRestoresHighlightCommand: missing controller/actor/mesh from a previous step."));
				return true;
			}

			Controller->ToggleCleanView();
			Test->TestNotNull(TEXT("Leaving Clean View restores the selection highlight overlay"), Actor->Mesh->GetOverlayMaterial());
			Test->TestTrue(TEXT("Leaving Clean View restores Custom Depth"), Actor->Mesh->bRenderCustomDepth != 0);
			Test->TestEqual(TEXT("Selection id unchanged by the Clean View round-trip"), Actor->GetSelectedPartId(), TorsoPartId);

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Turns Inspection off while Torso is still selected (run right after the
	// Clean View restore step) and checks the selection AND its visible
	// highlight are cleared (section 4/7) and the INSPECTION section collapses.
	class FToggleInspectionOffCommand : public IAutomationLatentCommand
	{
	public:
		FToggleInspectionOffCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerController* Controller = State->Controller.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Controller)
			{
				Test->AddError(TEXT("FToggleInspectionOffCommand: missing controller from a previous step."));
				return true;
			}

			Controller->ToggleInspection();
			Test->TestFalse(TEXT("ToggleInspection() (off) disables inspection"), Controller->IsInspectionEnabled());
			if (Actor)
			{
				Test->TestEqual(TEXT("Turning Inspection off clears the selection"), Actor->GetSelectedPartId(), NAME_None);
				if (Actor->Mesh)
				{
					Test->TestNull(TEXT("Turning Inspection off removes the highlight overlay"), Actor->Mesh->GetOverlayMaterial());
					Test->TestFalse(TEXT("Turning Inspection off turns Custom Depth off"), Actor->Mesh->bRenderCustomDepth != 0);
				}
			}
			if (UCharacterViewerWidget* Widget = State->Widget.Get())
			{
				Test->TestEqual(TEXT("Fallback INSPECTION section box collapses when Inspection is turned off"),
					Widget->GetFallbackInspectionSectionVisibility(), ESlateVisibility::Collapsed);
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// --- UI-over-panel click guard (P2 evidence "UI 위 선택 차단") ---
	//
	// Synthesizes real Slate pointer input (FSlateApplication::Process*Event,
	// the same entry points the platform message handler uses) instead of only
	// warping the OS cursor: SetCursorPos() alone never produced a Slate
	// MouseMove, so UWidget::IsHovered() never updated. The OS cursor is also
	// warped to the same point so Slate's own synthesized mouse moves (which
	// read the real cursor) agree with the synthetic event.
	//
	// Panel point: 8 local units left of the widget's right edge, vertically
	// centred -- inside the fallback panel border's 16px padding (section
	// 13.10), so the press lands on the panel background, not on a button
	// (a UButton would consume the press itself and never exercise the guard).
	// Canvas point: 10%/10% of the full-screen widget -- empty viewport area.
	static bool ComputeSynthPointerPos(UCharacterViewerWidget* Widget, bool bOverPanel, FVector2D& OutAbsolutePos)
	{
		if (!Widget)
		{
			return false;
		}

		const FGeometry& Geometry = Widget->GetCachedGeometry();
		const FVector2D LocalSize = Geometry.GetLocalSize();
		if (LocalSize.X <= 16.f || LocalSize.Y <= 16.f)
		{
			return false;
		}

		const FVector2D LocalPos = bOverPanel
			? FVector2D(LocalSize.X - 8.f, LocalSize.Y * 0.5f)
			: FVector2D(LocalSize.X * 0.1f, LocalSize.Y * 0.1f);
		OutAbsolutePos = Geometry.LocalToAbsolute(LocalPos);
		return true;
	}

	class FSynthPointerMoveCommand : public IAutomationLatentCommand
	{
	public:
		FSynthPointerMoveCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, bool bInOverPanel)
			: Test(InTest), State(InState), bOverPanel(bInOverPanel)
		{
		}

		virtual bool Update() override
		{
			FVector2D TargetPos;
			if (!FSlateApplication::IsInitialized() || !ComputeSynthPointerPos(State->Widget.Get(), bOverPanel, TargetPos))
			{
				Test->AddError(TEXT("FSynthPointerMoveCommand: no Slate application / widget geometry to synthesize a pointer move in."));
				return true;
			}

			FSlateApplication& SlateApp = FSlateApplication::Get();
			const FVector2D LastPos = SlateApp.GetCursorPos();
			SlateApp.SetCursorPos(TargetPos);

			// Precondition for every pointer step that follows. Slate re-synthesizes
			// cursor moves on its own (FSlateUser::SynthesizeCursorMoveIfNeeded ->
			// FSlateApplication::ProcessMouseMoveEvent(..., bIsSynthetic=true)) and,
			// when the application is not active and the OS cursor is not directly
			// over a Slate window (e.g. another app's window covers the game
			// window), those synthesized moves get an empty widget path and clear
			// the panel's hover; Win32 also denies mouse capture to background
			// windows, which stops viewport mouse-axis input. So an inactive or
			// obscured -game window makes hover/wheel/orbit results meaningless.
			// Fail loudly here instead (Docs/CHARACTER_VIEWER_SETUP.md 13.10.2).
			const bool bAppActive = SlateApp.IsActive();
			const TSharedPtr<GenericApplication> PlatformApp = SlateApp.GetPlatformApplication();
			const bool bCursorOverSlateWindow = PlatformApp.IsValid() && PlatformApp->IsCursorDirectlyOverSlateWindow();
			if ((!bAppActive || !bCursorOverSlateWindow) && !State->bPreconditionReported)
			{
				State->bPreconditionReported = true;
				Test->AddError(FString::Printf(TEXT("Precondition failed: the -game window must be the active, unobscured foreground window for synthesized pointer steps (FSlateApplication::IsActive()=%d, IsCursorDirectlyOverSlateWindow()=%d). Bring the game window to the front with nothing covering it and re-run."), bAppActive ? 1 : 0, bCursorOverSlateWindow ? 1 : 0));
			}

			const TSet<FKey> NoButtons;
			const FPointerEvent MoveEvent(FSlateApplication::CursorPointerIndex, TargetPos, LastPos, NoButtons, EKeys::Invalid, 0.f, SlateApp.GetModifierKeys());
			SlateApp.ProcessMouseMoveEvent(MoveEvent);

			State->SynthPointerPos = TargetPos;
			Test->AddInfo(FString::Printf(TEXT("Synthesized pointer move to %s at (%.0f, %.0f)."), bOverPanel ? TEXT("the fallback panel") : TEXT("empty canvas"), TargetPos.X, TargetPos.Y));

			// Diagnostics only (no assertion): the synthesized events are
			// hit-tested against these rects, so a minimized/off-screen game
			// window (see Docs/CHARACTER_VIEWER_SETUP.md 13.10.2 launch recipe)
			// shows up here instead of as an unexplained hover/zoom/orbit failure.
			if (bOverPanel)
			{
				const TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
				const UCharacterViewerWidget* Widget = State->Widget.Get();
				const FGeometry PanelGeometry = (Widget && Widget->PanelRoot) ? Widget->PanelRoot->GetCachedGeometry() : FGeometry();
				const FVector2D PanelMin = PanelGeometry.GetAbsolutePosition();
				const FVector2D PanelMax = PanelMin + PanelGeometry.GetAbsoluteSize();
				Test->AddInfo(FString::Printf(TEXT("Window state: visible=%d minimized=%d pos=(%.0f, %.0f) size=%.0fx%.0f; PanelRoot absolute rect=(%.0f, %.0f)-(%.0f, %.0f)."),
					Window.IsValid() && Window->IsVisible() ? 1 : 0,
					Window.IsValid() && Window->IsWindowMinimized() ? 1 : 0,
					Window.IsValid() ? Window->GetPositionInScreen().X : 0.f, Window.IsValid() ? Window->GetPositionInScreen().Y : 0.f,
					Window.IsValid() ? Window->GetSizeInScreen().X : 0.f, Window.IsValid() ? Window->GetSizeInScreen().Y : 0.f,
					PanelMin.X, PanelMin.Y, PanelMax.X, PanelMax.Y));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		bool bOverPanel;
	};

	class FSynthLeftButtonCommand : public IAutomationLatentCommand
	{
	public:
		FSynthLeftButtonCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, bool bInDown)
			: Test(InTest), State(InState), bDown(bInDown)
		{
		}

		virtual bool Update() override
		{
			if (!FSlateApplication::IsInitialized())
			{
				Test->AddError(TEXT("FSynthLeftButtonCommand: no Slate application."));
				return true;
			}

			FSlateApplication& SlateApp = FSlateApplication::Get();
			TSet<FKey> PressedButtons;
			if (bDown)
			{
				PressedButtons.Add(EKeys::LeftMouseButton);
			}

			const FPointerEvent ButtonEvent(FSlateApplication::CursorPointerIndex, State->SynthPointerPos, State->SynthPointerPos, PressedButtons, EKeys::LeftMouseButton, 0.f, SlateApp.GetModifierKeys());
			if (bDown)
			{
				SlateApp.ProcessMouseButtonDownEvent(nullptr, ButtonEvent);
			}
			else
			{
				SlateApp.ProcessMouseButtonUpEvent(ButtonEvent);
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		bool bDown;
	};

	class FCheckPointerOverPanelCommand : public IAutomationLatentCommand
	{
	public:
		FCheckPointerOverPanelCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, bool bInExpectedOverPanel)
			: Test(InTest), State(InState), bExpectedOverPanel(bInExpectedOverPanel)
		{
		}

		virtual bool Update() override
		{
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Widget)
			{
				Test->AddError(TEXT("FCheckPointerOverPanelCommand: missing widget from a previous step."));
				return true;
			}

			Test->TestEqual(bExpectedOverPanel
					? TEXT("IsPointerOverPanel() is true after a synthesized pointer move over the fallback panel")
					: TEXT("IsPointerOverPanel() is false after a synthesized pointer move over empty canvas (control)"),
				Widget->IsPointerOverPanel(), bExpectedOverPanel);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		bool bExpectedOverPanel;
	};

	class FCheckSelectedPartCommand : public IAutomationLatentCommand
	{
	public:
		FCheckSelectedPartCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, FName InExpectedPartId, const TCHAR* InDescription)
			: Test(InTest), State(InState), ExpectedPartId(InExpectedPartId), Description(InDescription)
		{
		}

		virtual bool Update() override
		{
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Actor)
			{
				Test->AddError(TEXT("FCheckSelectedPartCommand: missing actor from a previous step."));
				return true;
			}

			Test->TestEqual(Description, Actor->GetSelectedPartId(), ExpectedPartId);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		FName ExpectedPartId;
		FString Description;
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
		// bInExpectWireframeOnBefore: asserts Wireframe is ON right before the
		// switch, so this switch also proves ApplyProfile() drops the old
		// profile's wireframe material overrides (P2-4 / section 7).
		FSwitchProfileAndVerifyCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, FString InProfilePath, bool bInExpectWireframeOnBefore = false)
			: Test(InTest), State(InState), ProfilePath(MoveTemp(InProfilePath)), bExpectWireframeOnBefore(bInExpectWireframeOnBefore)
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

			if (bExpectWireframeOnBefore)
			{
				Test->TestTrue(TEXT("Precondition: Wireframe is ON right before this profile switch"), Actor->IsWireframeEnabled());
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

			// P2 completion evidence (section 7): a profile switch clears the
			// part selection/highlight (ApplyProfile -> ClearRuntimeState()),
			// but the Inspection *toggle* itself is a Controller-level UI mode
			// that intentionally persists across a profile switch (documented
			// choice, section 13.11) so browsing stays in Inspection mode
			// after picking a different character.
			Test->TestEqual(TEXT("Part selection cleared by the profile switch"), Actor->GetSelectedPartId(), NAME_None);
			Test->TestTrue(TEXT("Inspection toggle state persists across a profile switch"), Controller->IsInspectionEnabled());
			Test->TestFalse(TEXT("Wireframe cleared by the profile switch"), Actor->IsWireframeEnabled());

			// No material override survives the switch (in particular not the
			// previous profile's wireframe material): every slot shows the new
			// mesh's own default material.
			if (USkeletalMeshComponent* Mesh = Actor->Mesh)
			{
				Test->TestEqual(TEXT("No material overrides remain after the profile switch"), Mesh->GetNumOverrideMaterials(), 0);
				if (ExpectedMesh)
				{
					const TArray<FSkeletalMaterial>& DefaultMaterials = ExpectedMesh->GetMaterials();
					Test->TestEqual(TEXT("Mesh slot count matches the switched-to mesh's material count"), Mesh->GetNumMaterials(), DefaultMaterials.Num());
					for (int32 SlotIndex = 0; SlotIndex < DefaultMaterials.Num(); ++SlotIndex)
					{
						Test->TestEqual(FString::Printf(TEXT("Mesh slot %d material equals the switched-to mesh's default material"), SlotIndex),
							Mesh->GetMaterial(SlotIndex), DefaultMaterials[SlotIndex].MaterialInterface.Get());
					}
				}
				Test->TestNull(TEXT("No highlight overlay after the profile switch"), Mesh->GetOverlayMaterial());
				Test->TestFalse(TEXT("Custom Depth off after the profile switch"), Mesh->bRenderCustomDepth != 0);
			}

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
		bool bExpectWireframeOnBefore = false;
	};

	// --- P2 UMG designer/fallback pass (Docs/CHARACTER_VIEWER_SETUP.md section 13.10) ---

	// Runs an arbitrary one-shot action on the next Update(); avoids a
	// dedicated latent command class for every single Controller/Widget call
	// below (SelectCameraPreset/SetTurntableEnabled/etc. are already exercised
	// elsewhere in this file through their own named commands -- these are
	// just wiring, not new behavior under test).
	class FGenericLatentCommand : public IAutomationLatentCommand
	{
	public:
		explicit FGenericLatentCommand(TFunction<void()> InFunc)
			: Func(MoveTemp(InFunc))
		{
		}

		virtual bool Update() override
		{
			Func();
			return true;
		}

	private:
		TFunction<void()> Func;
	};

	// WBP_CharacterViewer now HAS a designer-built tree: the 2026-09-29 P2 UMG
	// pass added UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout()
	// (Source/CharacterShowcase/Editor/CharacterViewerEditorTools.h/.cpp) and
	// Scripts/CreateViewerWidgetLayout.py ran it against WBP_CharacterViewer
	// (see Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1), so the designer
	// path -- not the C++ fallback -- is what is actually in the viewport now.
	// This asserts that (IsUsingDesignerLayout()==true) and separately proves
	// NameText/GetFallbackDisplayNameText() -- shared by both layout paths --
	// shows the live profile DisplayName regardless of which path filled it.
	class FCheckLayoutModeCommand : public IAutomationLatentCommand
	{
	public:
		FCheckLayoutModeCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			UCharacterViewerWidget* Widget = State->Widget.Get();
			APortfolioCharacterActor* Actor = State->Actor.Get();
			if (!Test->TestNotNull(TEXT("Widget exists for the layout-mode check"), Widget) || !Actor || !Actor->Profile)
			{
				return true;
			}

			Test->TestTrue(TEXT("WBP_CharacterViewer now has a designer-built tree (UCharacterViewerEditorTools), so IsUsingDesignerLayout() is true (see Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1)"),
				Widget->IsUsingDesignerLayout());
			Test->TestEqual(TEXT("NameText/GetFallbackDisplayNameText() (shared by both layout paths) shows the current profile's DisplayName"),
				Widget->GetFallbackDisplayNameText().ToString(), Actor->Profile->DisplayName.ToString());

			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	// Stores the camera pawn's current distance/yaw for a later
	// FCheckCameraDistanceChangedCommand/FCheckCameraYawChangedCommand comparison.
	class FCaptureCameraStateCommand : public IAutomationLatentCommand
	{
	public:
		FCaptureCameraStateCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState)
			: Test(InTest), State(InState)
		{
		}

		virtual bool Update() override
		{
			if (ACharacterViewerCameraPawn* Pawn = State->Pawn.Get())
			{
				State->CapturedDistance = Pawn->GetDistance();
				State->CapturedYaw = FMath::UnwindDegrees(Pawn->GetYaw());
			}
			else
			{
				Test->AddError(TEXT("FCaptureCameraStateCommand: missing pawn."));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
	};

	class FCheckCameraDistanceChangedCommand : public IAutomationLatentCommand
	{
	public:
		FCheckCameraDistanceChangedCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, bool bInExpectChanged, const TCHAR* InDescription)
			: Test(InTest), State(InState), bExpectChanged(bInExpectChanged), Description(InDescription)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerCameraPawn* Pawn = State->Pawn.Get();
			if (!Pawn)
			{
				Test->AddError(TEXT("FCheckCameraDistanceChangedCommand: missing pawn."));
				return true;
			}
			const bool bChanged = !FMath::IsNearlyEqual(Pawn->GetDistance(), State->CapturedDistance, 0.5f);
			if (bExpectChanged)
			{
				Test->TestTrue(FString::Printf(TEXT("%s (before: %.2f, after: %.2f)"), Description, State->CapturedDistance, Pawn->GetDistance()), bChanged);
			}
			else
			{
				Test->TestFalse(FString::Printf(TEXT("%s (before: %.2f, after: %.2f)"), Description, State->CapturedDistance, Pawn->GetDistance()), bChanged);
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		bool bExpectChanged;
		const TCHAR* Description;
	};

	class FCheckCameraYawChangedCommand : public IAutomationLatentCommand
	{
	public:
		FCheckCameraYawChangedCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, bool bInExpectChanged, const TCHAR* InDescription)
			: Test(InTest), State(InState), bExpectChanged(bInExpectChanged), Description(InDescription)
		{
		}

		virtual bool Update() override
		{
			ACharacterViewerCameraPawn* Pawn = State->Pawn.Get();
			if (!Pawn)
			{
				Test->AddError(TEXT("FCheckCameraYawChangedCommand: missing pawn."));
				return true;
			}
			const float CurrentYaw = FMath::UnwindDegrees(Pawn->GetYaw());
			const bool bChanged = !FMath::IsNearlyEqual(CurrentYaw, State->CapturedYaw, 0.5f);
			if (bExpectChanged)
			{
				Test->TestTrue(FString::Printf(TEXT("%s (before: %.2f, after: %.2f)"), Description, State->CapturedYaw, CurrentYaw), bChanged);
			}
			else
			{
				Test->TestFalse(FString::Printf(TEXT("%s (before: %.2f, after: %.2f)"), Description, State->CapturedYaw, CurrentYaw), bChanged);
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		bool bExpectChanged;
		const TCHAR* Description;
	};

	// Synthesizes a mouse-wheel event at State->SynthPointerPos (set by a
	// prior FSynthPointerMoveCommand), the same Slate entry point
	// (FSlateApplication::ProcessMouseWheelOrGestureEvent) the platform
	// message handler uses for a real wheel tick.
	class FSynthWheelCommand : public IAutomationLatentCommand
	{
	public:
		FSynthWheelCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, float InDelta)
			: Test(InTest), State(InState), Delta(InDelta)
		{
		}

		virtual bool Update() override
		{
			if (!FSlateApplication::IsInitialized())
			{
				Test->AddError(TEXT("FSynthWheelCommand: no Slate application."));
				return true;
			}

			FSlateApplication& SlateApp = FSlateApplication::Get();
			const TSet<FKey> NoButtons;
			const FPointerEvent WheelEvent(FSlateApplication::CursorPointerIndex, State->SynthPointerPos, State->SynthPointerPos, NoButtons, EKeys::Invalid, Delta, SlateApp.GetModifierKeys());
			SlateApp.ProcessMouseWheelOrGestureEvent(WheelEvent, nullptr);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		float Delta;
	};

	// Moves the synthesized pointer BY Delta from its last position
	// (State->SynthPointerPos), with the left mouse button reported held --
	// i.e. a drag step -- via FSlateApplication::ProcessMouseMoveEvent, the
	// same entry point the platform message handler uses while the mouse is
	// captured by the game viewport.
	class FSynthDragMoveCommand : public IAutomationLatentCommand
	{
	public:
		FSynthDragMoveCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, FVector2D InDelta)
			: Test(InTest), State(InState), Delta(InDelta)
		{
		}

		virtual bool Update() override
		{
			if (!FSlateApplication::IsInitialized())
			{
				Test->AddError(TEXT("FSynthDragMoveCommand: no Slate application."));
				return true;
			}

			FSlateApplication& SlateApp = FSlateApplication::Get();
			const FVector2D LastPos = State->SynthPointerPos;
			const FVector2D NewPos = LastPos + Delta;
			SlateApp.SetCursorPos(NewPos);

			TSet<FKey> PressedButtons;
			PressedButtons.Add(EKeys::LeftMouseButton);
			const FPointerEvent MoveEvent(FSlateApplication::CursorPointerIndex, NewPos, LastPos, PressedButtons, EKeys::Invalid, 0.f, SlateApp.GetModifierKeys());
			SlateApp.ProcessMouseMoveEvent(MoveEvent);

			State->SynthPointerPos = NewPos;
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		FVector2D Delta;
	};

	// Simulates the application (window) losing OS focus mid-drag/press by
	// broadcasting the same delegate ACharacterViewerController::BeginPlay()
	// subscribed to (FSlateApplication::OnApplicationActivationStateChanged()).
	class FBroadcastActivationStateCommand : public IAutomationLatentCommand
	{
	public:
		FBroadcastActivationStateCommand(FAutomationTestBase* InTest, bool bInActive)
			: Test(InTest), bActive(bInActive)
		{
		}

		virtual bool Update() override
		{
			if (!FSlateApplication::IsInitialized())
			{
				Test->AddError(TEXT("FBroadcastActivationStateCommand: no Slate application."));
				return true;
			}
			FSlateApplication::Get().OnApplicationActivationStateChanged().Broadcast(bActive);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		bool bActive;
	};

	// Checks the rendered label of the generated button for (Kind, Id)
	// (UCharacterViewerWidget::GetGeneratedButtonText(), section 13.10 "state
	// display") contains ExpectedSubstring.
	class FCheckGeneratedButtonTextCommand : public IAutomationLatentCommand
	{
	public:
		FCheckGeneratedButtonTextCommand(FAutomationTestBase* InTest, TSharedRef<FSharedState> InState, ECharacterViewerButtonKind InKind, FName InId, FString InExpectedSubstring, FString InDescription)
			: Test(InTest), State(InState), Kind(InKind), Id(InId), ExpectedSubstring(MoveTemp(InExpectedSubstring)), Description(MoveTemp(InDescription))
		{
		}

		virtual bool Update() override
		{
			UCharacterViewerWidget* Widget = State->Widget.Get();
			if (!Widget)
			{
				Test->AddError(TEXT("FCheckGeneratedButtonTextCommand: missing widget."));
				return true;
			}
			const FString Text = Widget->GetGeneratedButtonText(Kind, Id).ToString();
			Test->TestTrue(FString::Printf(TEXT("%s (actual button text: '%s')"), *Description, *Text), Text.Contains(ExpectedSubstring));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FSharedState> State;
		ECharacterViewerButtonKind Kind;
		FName Id;
		FString ExpectedSubstring;
		FString Description;
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

	// --- P2 Inspection / Wireframe (Docs/CHARACTER_VIEWER_SETUP.md section 7/13.11) ---
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FToggleInspectionOnCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FInspectTorsoCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Inspect")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FInspectEmptySpaceCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FToggleWireframeOnCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Wireframe")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSelectGridWhileWireframeCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FToggleWireframeOffCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FInspectThenCleanViewCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FLeaveCleanViewRestoresHighlightCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	// Inspection off while Torso is still selected: selection + overlay + Custom Depth cleared.
	ADD_LATENT_AUTOMATION_COMMAND(FToggleInspectionOffCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// Re-enter Inspection, select Torso and turn Wireframe on, so the first
	// profile switch below starts from "selection + Wireframe on".
	ADD_LATENT_AUTOMATION_COMMAND(FToggleInspectionOnCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FInspectTorsoCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FToggleWireframeOnCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// 7/8. Profile switch (P1 completion evidence, section 6, deliverable A):
	// DA_Character -> DA_Character_Cube -> DA_Character, entirely through
	// ProfileLibrary/SelectCharacterProfile(), no code change between profiles.
	// Also P2 completion evidence: the first switch happens with Torso selected
	// and Wireframe ON, and FSwitchProfileAndVerifyCommand checks selection/
	// highlight/Wireframe/material overrides are cleared while the Inspection
	// toggle persists (see its P2 assertions).
	ADD_LATENT_AUTOMATION_COMMAND(FSwitchProfileAndVerifyCommand(this, State, TEXT("/Game/Portfolio/Data/DA_Character_Cube.DA_Character_Cube"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Profile2")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSwitchProfileAndVerifyCommand(this, State, TEXT("/Game/Portfolio/Data/DA_Character.DA_Character")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureWindowScreenshotCommand(this, TEXT("ViewerSmoke_Profile1")));

	// P2 "UI 위 선택 차단": with Torso selected (Inspection still on), a
	// synthesized Slate pointer move + left press/release over the fallback
	// panel must report IsPointerOverPanel()==true and leave the selection
	// unchanged. Control: the same over empty canvas must report false, and
	// its press/release must reach the Inspection click path (clearing the
	// selection) -- proving the synthesized input does reach Enhanced Input,
	// so the "unchanged" result over the panel is the guard, not lost input.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FInspectTorsoCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckPointerOverPanelCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckSelectedPartCommand(this, State, TorsoPartId, TEXT("Press+release over the fallback panel does not change the selection")));
	// Re-select Torso (idempotent if the guard held) so the control click below
	// starts from a real selection and its "cleared" result is not vacuous.
	ADD_LATENT_AUTOMATION_COMMAND(FInspectTorsoCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckPointerOverPanelCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckSelectedPartCommand(this, State, NAME_None, TEXT("Control: press+release over empty canvas reaches the Inspection click path and clears the selection")));

	// --- P2 UMG designer/fallback pass (Docs/CHARACTER_VIEWER_SETUP.md section 13.10) ---

	// 1. Layout mode: WBP_CharacterViewer now has a designer-built tree
	// (UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout(), run via
	// Scripts/CreateViewerWidgetLayout.py -- see that script's docstring and
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1), so the designer path
	// -- not the C++ fallback -- is what is actually in the viewport here.
	ADD_LATENT_AUTOMATION_COMMAND(FCheckLayoutModeCommand(this, State));

	// 2. Wheel: over the panel must scroll it (never reach Zoom); outside the
	// panel must Zoom. Cursor positions/hover reuse FSynthPointerMoveCommand
	// (proven, section 13.11.5/13.11.9) and the wheel is a real synthesized
	// Slate wheel event (FSlateApplication::ProcessMouseWheelOrGestureEvent).
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureCameraStateCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthWheelCommand(this, State, -5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCameraDistanceChangedCommand(this, State, false, TEXT("Wheel over the panel's padding does not change camera distance (Controller::HandleZoom's IsPointerOverPanel() guard blocks Zoom)")));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureCameraStateCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthWheelCommand(this, State, -5.f));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCameraDistanceChangedCommand(this, State, true, TEXT("Wheel outside the panel (empty canvas) changes camera distance (Zoom)")));

	// 3. Drag: a press+move starting over the panel must not Orbit (the
	// panel's OnMouseButtonDownEvent consumes the press, section 13.10/13.11.9);
	// the same press+move starting over empty canvas must Orbit (yaw changes).
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureCameraStateCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthDragMoveCommand(this, State, FVector2D(200.f, 0.f)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCameraYawChangedCommand(this, State, false, TEXT("Press+200px drag starting over the panel's padding does not Orbit (PanelRoot->OnMouseButtonDownEvent consumes the press; yaw unchanged)")));

	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureCameraStateCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthDragMoveCommand(this, State, FVector2D(200.f, 0.f)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCameraYawChangedCommand(this, State, true, TEXT("Control: press+200px drag starting over empty canvas Orbits (yaw changes)")));

	// 4. Focus loss mid-press: press over empty canvas (bIsPressed true),
	// then simulate the application losing OS focus
	// (FSlateApplication::OnApplicationActivationStateChanged(false), the same
	// delegate ACharacterViewerController subscribes to) -- the drag must be
	// cleared, so a subsequent move does not Orbit.
	ADD_LATENT_AUTOMATION_COMMAND(FSynthPointerMoveCommand(this, State, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureCameraStateCommand(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FBroadcastActivationStateCommand(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthDragMoveCommand(this, State, FVector2D(200.f, 0.f)));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckCameraYawChangedCommand(this, State, false, TEXT("Losing OS focus mid-press clears the drag (subsequent move does not Orbit)")));
	ADD_LATENT_AUTOMATION_COMMAND(FBroadcastActivationStateCommand(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FSynthLeftButtonCommand(this, State, false));

	// 5. State display: the generated CHARACTER/VIEW/DISPLAY button labels
	// must reflect the current selection/toggle state (section 13.10).
	ADD_LATENT_AUTOMATION_COMMAND(FGenericLatentCommand([State]()
	{
		if (UCharacterViewerWidget* Widget = State->Widget.Get())
		{
			Widget->RequestCameraPreset(FName(TEXT("Face")));
		}
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckGeneratedButtonTextCommand(this, State, ECharacterViewerButtonKind::CameraPreset, FName(TEXT("Face")), FString(TEXT("▶")), TEXT("Face camera preset button shows the selected-state prefix after RequestCameraPreset('Face')")));

	ADD_LATENT_AUTOMATION_COMMAND(FGenericLatentCommand([State]()
	{
		if (ACharacterViewerController* Controller = State->Controller.Get())
		{
			Controller->SetTurntableEnabled(true);
		}
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckGeneratedButtonTextCommand(this, State, ECharacterViewerButtonKind::ToggleTurntable, NAME_None, FString(TEXT("On")), TEXT("Turntable button label shows 'On' after Controller->SetTurntableEnabled(true)")));
	ADD_LATENT_AUTOMATION_COMMAND(FGenericLatentCommand([State]()
	{
		if (ACharacterViewerController* Controller = State->Controller.Get())
		{
			Controller->SetTurntableEnabled(false);
		}
	}));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
