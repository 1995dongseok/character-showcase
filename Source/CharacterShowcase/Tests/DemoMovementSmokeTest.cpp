#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "ImageUtils.h"
#include "InputAction.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "PlayDemo/DemoCharacter.h"
#include "PlayDemo/DemoGameMode.h"
#include "PlayDemo/DemoPlayerController.h"
#include "Widgets/SWindow.h"

// D1 (Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md section 12): drives the real
// ADemoGameMode / ADemoCharacter / ADemoPlayerController in a -game process
// launched into /Game/PlayDemo/Maps/LV_PlayDemo. Input goes through the real
// Enhanced Input path (InjectInputForAction re-injected every frame for
// action-level checks; PlayerController::InputKey with FInputKeyEventArgs for
// the key-mapping path: focus loss / key repeat (step 8) and step 11 A/S/D,
// mouse X/Y, wheel, R, Backspace, Esc). Deliberately ClientContext only, so it never runs in the Editor
// automation context. Timing is wall-clock based (this dev PC renders ~5 FPS).

namespace DemoMovementSmokeTest
{
	struct FState
	{
		TWeakObjectPtr<ADemoPlayerController> PC;
		TWeakObjectPtr<ADemoCharacter> Character;
		FVector StartLocation = FVector::ZeroVector;
		FVector StepStartLocation = FVector::ZeroVector;
		double MaxSpeedSeen = 0.0;
		bool bOnceDone = false;
	};

	// Geometry of LV_PlayDemo (Scripts/CreatePlayDemoAssets.py): +X wall inner face.
	static constexpr float WallInnerFaceX = 1475.f;
	static constexpr float CapsuleRadius = 42.f;

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

	static void Inject(const FState& State, const UInputAction* Action, const FInputActionValue& Value)
	{
		ADemoPlayerController* PC = State.PC.Get();
		if (!PC || !Action)
		{
			return;
		}
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->InjectInputForAction(Action, Value, {}, {});
		}
	}

	// Real key-state path: PlayerController -> PlayerInput -> Enhanced Input mappings.
	static void SendKey(const FState& State, const FKey& Key, EInputEvent Event)
	{
		if (ADemoPlayerController* PC = State.PC.Get())
		{
			FViewport* Viewport = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->Viewport : nullptr;
			const FInputDeviceId Device = FInputDeviceId::CreateFromInternalId(0);
			PC->InputKey(FInputKeyEventArgs(Viewport, Device, Key, Event, 1.0f, false, FPlatformTime::Cycles64()));
		}
	}

	// Real axis path (what UGameViewportClient::InputAxis forwards for mouse movement / wheel): IE_Axis with a
	// delta. UPlayerInput pairs MouseX/MouseY into Mouse2D. +MouseY = mouse moved up.
	static void SendAxis(const FState& State, const FKey& Key, float Delta)
	{
		if (ADemoPlayerController* PC = State.PC.Get())
		{
			FViewport* Viewport = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->Viewport : nullptr;
			const FInputDeviceId Device = FInputDeviceId::CreateFromInternalId(0);
			PC->InputKey(FInputKeyEventArgs(Viewport, Device, Key, Delta, static_cast<float>(FApp::GetDeltaTime()), 1, FPlatformTime::Cycles64()));
		}
	}

	static float Speed2D(const FState& State)
	{
		const ADemoCharacter* Character = State.Character.Get();
		return Character ? static_cast<float>(Character->GetVelocity().Size2D()) : -1.f;
	}

	static void SaveScreenshot(FAutomationTestBase* Test, const FString& Name)
	{
		TSharedPtr<SWindow> Window = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWindow() : nullptr;
		if (!Window.IsValid() || !FSlateApplication::IsInitialized())
		{
			Test->AddError(FString::Printf(TEXT("Screenshot '%s': no game window / Slate application to capture."), *Name));
			return;
		}

		TArray<FColor> Bitmap;
		FIntVector Size(0, 0, 0);
		if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(), Bitmap, Size))
		{
			Test->AddError(FString::Printf(TEXT("Screenshot '%s': FSlateApplication::TakeScreenshot failed."), *Name));
			return;
		}
		for (FColor& Pixel : Bitmap)
		{
			Pixel.A = 255;
		}
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (Name + TEXT(".png")));
		const bool bSaved = FImageUtils::SaveImageByExtension(*Path, FImageView(Bitmap.GetData(), Size.X, Size.Y));
		if (Test->TestTrue(FString::Printf(TEXT("Screenshot '%s' saved"), *Name), bSaved))
		{
			Test->AddInfo(FString::Printf(TEXT("Screenshot '%s' (%dx%d) -> %s"), *Name, Size.X, Size.Y, *Path));
		}
	}

	// The -game window is a real window: physical keyboard/mouse input reaching it would change the camera or
	// move the character and make the measurements meaningless. Every latent command therefore (re)asserts
	// "ignore physical input" on the game viewport (APlayerController::SetInputMode resets it, e.g. on the Esc
	// toggle). Test input goes only through InjectInputForAction / PlayerController::InputKey, which bypass it.
	static void IgnorePhysicalInput()
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->SetIgnoreInput(true);
		}
	}

	// One-shot step.
	class FStepCommand : public IAutomationLatentCommand
	{
	public:
		explicit FStepCommand(TFunction<void()> InFn) : Fn(MoveTemp(InFn)) {}
		virtual bool Update() override
		{
			IgnorePhysicalInput();
			Fn();
			return true;
		}

	private:
		TFunction<void()> Fn;
	};

	// Calls Tick(ElapsedSeconds) every frame for Duration seconds (wall clock), then End() once.
	class FHoldCommand : public IAutomationLatentCommand
	{
	public:
		FHoldCommand(double InDuration, TFunction<void(double)> InTick, TFunction<void()> InEnd)
			: Duration(InDuration), Tick(MoveTemp(InTick)), End(MoveTemp(InEnd))
		{
		}
		virtual bool Update() override
		{
			IgnorePhysicalInput();
			if (Start < 0.0)
			{
				Start = FPlatformTime::Seconds();
			}
			const double Elapsed = FPlatformTime::Seconds() - Start;
			if (Tick)
			{
				Tick(Elapsed);
			}
			if (Elapsed >= Duration)
			{
				if (End)
				{
					End();
				}
				return true;
			}
			return false;
		}

	private:
		double Duration;
		double Start = -1.0;
		TFunction<void(double)> Tick;
		TFunction<void()> End;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoMovementSmokeTest,
	"CharacterShowcase.Demo.MovementSmoke",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDemoMovementSmokeTest::RunTest(const FString& Parameters)
{
	using namespace DemoMovementSmokeTest;

	TSharedRef<FState> S = MakeShared<FState>();
	FDemoMovementSmokeTest* Test = this;

	auto Step = [](TFunction<void()> Fn)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FStepCommand(MoveTemp(Fn))));
	};
	auto Hold = [](double Duration, TFunction<void(double)> Tick, TFunction<void()> End)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FHoldCommand(Duration, MoveTemp(Tick), MoveTemp(End))));
	};
	auto Wait = [](double Duration)
	{
		FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(new FHoldCommand(Duration, nullptr, nullptr)));
	};
	// Fails the test (and stops further reads) if the character/controller are gone.
	auto Valid = [Test, S]() -> bool
	{
		return Test->TestTrue(TEXT("Demo character and controller are valid"), S->Character.IsValid() && S->PC.IsValid());
	};

	// Holds a Move injection for Duration and records the peak 2D speed after 0.6 s.
	auto HoldMove = [=](double Duration, FVector2D Move, bool bWithRun, TFunction<void(double)> ExtraTick, TFunction<void()> End)
	{
		Step([=]()
		{
			S->MaxSpeedSeen = 0.0;
			if (S->Character.IsValid())
			{
				S->StepStartLocation = S->Character->GetActorLocation();
			}
		});
		Hold(Duration,
			[=](double Elapsed)
			{
				if (!S->PC.IsValid() || !S->Character.IsValid())
				{
					return;
				}
				Inject(*S, S->PC->MoveAction, FInputActionValue(Move));
				if (bWithRun)
				{
					Inject(*S, S->PC->RunAction, FInputActionValue(true));
				}
				if (Elapsed > 0.6)
				{
					S->MaxSpeedSeen = FMath::Max(S->MaxSpeedSeen, static_cast<double>(Speed2D(*S)));
				}
				if (ExtraTick)
				{
					ExtraTick(Elapsed);
				}
			},
			End);
	};

	// ---------------------------------------------------------------- 1. Idle / scene
	Wait(3.0);
	Step([=]()
	{
		UWorld* World = FindGameWorld();
		if (!Test->TestNotNull(TEXT("A Game world exists"), World))
		{
			return;
		}
		Test->TestTrue(TEXT("GameMode is ADemoGameMode"), World->GetAuthGameMode() && World->GetAuthGameMode()->IsA<ADemoGameMode>());
		S->PC = Cast<ADemoPlayerController>(World->GetFirstPlayerController());
		if (!Test->TestTrue(TEXT("PlayerController is an ADemoPlayerController"), S->PC.IsValid()))
		{
			return;
		}
		S->Character = S->PC->GetDemoCharacter();
		if (!Test->TestTrue(TEXT("Pawn is an ADemoCharacter"), S->Character.IsValid()))
		{
			return;
		}

		// The -game window is a real window: any physical keyboard/mouse input that reached it before this
		// point (a person touching it while the automation framework waits for an interactive frame rate)
		// would have moved the character or camera. Record that, then start from the true start state.
		ADemoCharacter* Character = S->Character.Get();
		S->StartLocation = Character->GetStartTransform().GetLocation();

		// Start composition, read BEFORE any reset: the controller records the camera state at the end of its
		// BeginPlay (possession resets the control pitch to 0 first; BeginPlay must restore DefaultPitch).
		// The recorded values are asserted so physical input during the framework's FPS wait cannot fail
		// this; the live pre-reset values are logged next to them.
		const float BeginPitch = S->PC->GetBeginPlayCameraPitch();
		const float BeginArm = S->PC->GetBeginPlayArmLength();
		Test->TestTrue(FString::Printf(TEXT("Start: BeginPlay camera pitch %.2f == DefaultPitch %.1f (+-0.5)"), BeginPitch, Character->DefaultPitch),
			FMath::Abs(BeginPitch - Character->DefaultPitch) <= 0.5f);
		Test->TestTrue(FString::Printf(TEXT("Start: BeginPlay arm %.1f == DefaultArmLength %.1f (+-0.5)"), BeginArm, Character->DefaultArmLength),
			FMath::Abs(BeginArm - Character->DefaultArmLength) <= 0.5f);
		Test->TestEqual(TEXT("Start: DefaultPitch is -15"), Character->DefaultPitch, -15.f, 0.5f);
		Test->TestEqual(TEXT("Start: DefaultArmLength is 350"), Character->DefaultArmLength, 350.f, 0.5f);
		const bool bLiveMatches = FMath::Abs(Character->GetCameraPitch() - Character->DefaultPitch) <= 0.5f
			&& FMath::Abs(Character->GetArmLength() - Character->DefaultArmLength) <= 0.5f;
		Test->AddInfo(FString::Printf(TEXT("[1 start camera] beginPlay pitch=%.2f arm=%.1f | live pre-reset pitch=%.2f arm=%.1f yaw=%.1f location=%s (start %s)%s"),
			BeginPitch, BeginArm, Character->GetCameraPitch(), Character->GetArmLength(), Character->GetCameraYaw(),
			*Character->GetActorLocation().ToString(), *S->StartLocation.ToString(),
			bLiveMatches ? TEXT("") : TEXT(" (live differs: input reached the window before the test)")));

		// Isolation: every run starts from the true start state.
		S->PC->ReleaseAllInput();
		Character->ResetToStart();
	});
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		ADemoCharacter* Character = S->Character.Get();
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		const USkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset();
		Test->TestTrue(TEXT("Mesh asset is SKM_Manny_Simple"), MeshAsset && MeshAsset->GetName() == TEXT("SKM_Manny_Simple"));
		UAnimInstance* Anim = Mesh->GetAnimInstance();
		Test->TestNotNull(TEXT("Anim instance exists"), Anim);
		Test->TestTrue(TEXT("Anim instance class is ABP_Unarmed"), Anim && Anim->GetClass()->GetName().StartsWith(TEXT("ABP_Unarmed")));
		Test->TestEqual(TEXT("WalkSpeed from profile is 300"), Character->WalkSpeed, 300.f);
		Test->TestEqual(TEXT("RunSpeed from profile is 600"), Character->RunSpeed, 600.f);
		Test->TestEqual(TEXT("MaxWalkSpeed starts at WalkSpeed"), Character->GetCharacterMovement()->MaxWalkSpeed, 300.f);
		Test->TestEqual(TEXT("Camera arm is DefaultArmLength"), Character->GetArmLength(), Character->DefaultArmLength);
		Test->TestFalse(TEXT("Not in cursor mode at start"), S->PC->IsCursorMode());
		Test->TestFalse(TEXT("Mouse cursor hidden at start"), S->PC->bShowMouseCursor);
		Test->TestTrue(TEXT("Character stands on the floor (capsule center Z 90..110)"), Character->GetActorLocation().Z > 90.f && Character->GetActorLocation().Z < 110.f);
		Test->TestTrue(TEXT("Idle: velocity ~ 0"), Speed2D(*S) < 5.f);
		Test->TestTrue(TEXT("Character is at the start location (< 5 cm)"), (Character->GetActorLocation() - S->StartLocation).Size() < 5.f);
		Test->AddInfo(FString::Printf(TEXT("[1 idle] start=%s speed=%.2f mesh=%s anim=%s"), *S->StartLocation.ToString(), Speed2D(*S),
			*GetNameSafe(MeshAsset), *GetNameSafe(Anim ? Anim->GetClass() : nullptr)));
	});
	Step([=]() { SaveScreenshot(Test, TEXT("DemoSmoke_Idle")); });

	// ---------------------------------------------------------------- 2. Walk forward, then right (camera-relative + orient to movement)
	HoldMove(2.0, FVector2D(0.f, 1.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const ADemoCharacter* Character = S->Character.Get();
		const float Speed = Speed2D(*S);
		const FVector Delta = Character->GetActorLocation() - S->StepStartLocation;
		const float Yaw = Character->GetActorRotation().Yaw;
		Test->TestTrue(FString::Printf(TEXT("Walk: speed %.1f within 250..310"), Speed), Speed >= 250.f && Speed <= 310.f);
		Test->TestTrue(FString::Printf(TEXT("Walk: peak speed %.1f <= 310"), S->MaxSpeedSeen), S->MaxSpeedSeen <= 310.0);
		Test->TestTrue(FString::Printf(TEXT("Walk: advanced %.0f cm along +X (> 300)"), Delta.X), Delta.X > 300.f);
		Test->TestTrue(FString::Printf(TEXT("Walk forward: yaw %.1f stays toward camera forward (|yaw| < 10)"), Yaw), FMath::Abs(Yaw) < 10.f);
		Test->AddInfo(FString::Printf(TEXT("[2a walk fwd] speed=%.1f peak=%.1f dX=%.0f yaw=%.1f"), Speed, S->MaxSpeedSeen, Delta.X, Yaw));
	});
	HoldMove(1.5, FVector2D(1.f, 0.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const ADemoCharacter* Character = S->Character.Get();
		const FVector Velocity = Character->GetVelocity();
		const float Yaw = Character->GetActorRotation().Yaw;
		Test->TestTrue(FString::Printf(TEXT("Walk right: yaw %.1f rotated to camera-right (+90 +-15)"), Yaw), FMath::Abs(Yaw - 90.f) < 15.f);
		Test->TestTrue(FString::Printf(TEXT("Walk right: velocity moves along +Y (%.1f)"), Velocity.Y), Velocity.Y > 250.f && FMath::Abs(Velocity.X) < 40.f);
		Test->AddInfo(FString::Printf(TEXT("[2b walk right] velocity=%s yaw=%.1f"), *Velocity.ToString(), Yaw));
	});

	// ---------------------------------------------------------------- 3. Run, then release Shift while still moving
	Step([=]()
	{
		if (S->Character.IsValid()) { S->Character->ResetToStart(); }
	});
	Wait(0.5);
	HoldMove(1.8, FVector2D(0.f, 1.f), true,
		[=](double Elapsed)
		{
			if (Elapsed > 1.2 && !S->bOnceDone)
			{
				S->bOnceDone = true;
				SaveScreenshot(Test, TEXT("DemoSmoke_Run"));
			}
		},
		[=]()
		{
			if (!Valid()) { return; }
			const float Speed = Speed2D(*S);
			Test->TestTrue(TEXT("Run: IsRunning"), S->Character->IsRunning());
			Test->TestTrue(FString::Printf(TEXT("Run: speed %.1f within 550..610"), Speed), Speed >= 550.f && Speed <= 610.f);
			Test->TestTrue(FString::Printf(TEXT("Run: peak speed %.1f <= 610"), S->MaxSpeedSeen), S->MaxSpeedSeen <= 610.0);
			Test->AddInfo(FString::Printf(TEXT("[3a run] speed=%.1f peak=%.1f"), Speed, S->MaxSpeedSeen));
			S->bOnceDone = false;
		});
	// Release Run (stop injecting it) but keep moving.
	Step([=]()
	{
		if (S->Character.IsValid()) { S->Character->ResetToStart(); }
	});
	Wait(0.5);
	HoldMove(1.0, FVector2D(0.f, 1.f), true, nullptr, nullptr); // spin up to run speed again
	HoldMove(1.5, FVector2D(0.f, 1.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestFalse(TEXT("Run released: IsRunning is false"), S->Character->IsRunning());
		Test->TestTrue(FString::Printf(TEXT("Run released: speed back to <= 310 while moving (%.1f, >= 250)"), Speed), Speed <= 310.f && Speed >= 250.f);
		Test->AddInfo(FString::Printf(TEXT("[3b run released] speed=%.1f"), Speed));
	});

	// ---------------------------------------------------------------- 4. Diagonal
	Step([=]()
	{
		if (S->Character.IsValid()) { S->Character->ResetToStart(); }
	});
	Wait(0.5);
	HoldMove(2.0, FVector2D(1.f, 1.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Diagonal: speed %.1f <= WalkSpeed + 5"), Speed), Speed <= 305.f && Speed >= 250.f);
		Test->TestTrue(FString::Printf(TEXT("Diagonal: peak speed %.1f <= WalkSpeed + 5"), S->MaxSpeedSeen), S->MaxSpeedSeen <= 305.0);
		Test->AddInfo(FString::Printf(TEXT("[4 diagonal] speed=%.1f peak=%.1f"), Speed, S->MaxSpeedSeen));
	});

	// ---------------------------------------------------------------- 5. Stop injecting -> Idle
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Stop: speed %.2f < 5 after 1 s without input"), Speed), Speed < 5.f);
		Test->AddInfo(FString::Printf(TEXT("[5 stop] speed=%.2f"), Speed));
	});

	// ---------------------------------------------------------------- 6. Wall
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		const FVector Location = S->Character->GetActorLocation();
		S->Character->SetActorLocation(FVector(1300.f, 0.f, Location.Z), false, nullptr, ETeleportType::TeleportPhysics);
	});
	Wait(0.5);
	HoldMove(3.0, FVector2D(0.f, 1.f), false,
		[=](double Elapsed)
		{
			if (Elapsed > 2.2 && !S->bOnceDone)
			{
				S->bOnceDone = true;
				SaveScreenshot(Test, TEXT("DemoSmoke_Wall"));
			}
		},
		[=]()
		{
			if (!Valid()) { return; }
			const float X = S->Character->GetActorLocation().X;
			const float Speed = Speed2D(*S);
			const float MaxX = WallInnerFaceX - CapsuleRadius;
			Test->TestTrue(FString::Printf(TEXT("Wall: X %.1f does not pass the wall inner face limit %.1f (+3)"), X, MaxX), X <= MaxX + 3.f);
			Test->TestTrue(FString::Printf(TEXT("Wall: character reached the wall (X %.1f > %.1f)"), X, MaxX - 40.f), X > MaxX - 40.f);
			Test->TestTrue(FString::Printf(TEXT("Wall: speed %.2f < 20 while pushing (no running in place)"), Speed), Speed < 20.f);
			Test->AddInfo(FString::Printf(TEXT("[6 wall] X=%.1f limit=%.1f speed=%.2f"), X, MaxX, Speed));
			S->bOnceDone = false;
		});

	// ---------------------------------------------------------------- 7. Camera
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		Test->TestEqual(TEXT("Camera yaw after reset is the actor yaw"), S->Character->GetCameraYaw(), 0.f, 0.5f);
		Inject(*S, S->PC->LookAction, FInputActionValue(FVector2D(200.f, 0.f)));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Yaw = S->Character->GetCameraYaw();
		const float Expected = 200.f * S->PC->LookSensitivity;
		Test->TestTrue(FString::Printf(TEXT("Look (200,0): controller yaw changed to %.1f (expected ~%.1f)"), Yaw, Expected), FMath::Abs(Yaw - Expected) < 1.f);
		Test->AddInfo(FString::Printf(TEXT("[7a look yaw] yaw=%.1f"), Yaw));
		Inject(*S, S->PC->LookAction, FInputActionValue(FVector2D(0.f, -200.f)));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Pitch = S->Character->GetCameraPitch();
		Test->TestTrue(FString::Printf(TEXT("Look (0,-200): pitch %.2f clamped at MinPitch %.1f"), Pitch, S->Character->MinPitch), FMath::IsNearlyEqual(Pitch, S->Character->MinPitch, 0.1f));
		Test->AddInfo(FString::Printf(TEXT("[7b look pitch down] pitch=%.2f"), Pitch));
		Inject(*S, S->PC->LookAction, FInputActionValue(FVector2D(0.f, 400.f)));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Pitch = S->Character->GetCameraPitch();
		Test->TestTrue(FString::Printf(TEXT("Look (0,+400): pitch %.2f clamped at MaxPitch %.1f"), Pitch, S->Character->MaxPitch), FMath::IsNearlyEqual(Pitch, S->Character->MaxPitch, 0.1f));
		Test->AddInfo(FString::Printf(TEXT("[7c look pitch up] pitch=%.2f"), Pitch));
		Inject(*S, S->PC->ZoomAction, FInputActionValue(-5.f));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Arm = S->Character->GetArmLength();
		Test->TestTrue(FString::Printf(TEXT("Zoom -5: arm %.1f clamped at MaxArmLength %.1f"), Arm, S->Character->MaxArmLength), FMath::IsNearlyEqual(Arm, S->Character->MaxArmLength, 0.1f));
		Test->AddInfo(FString::Printf(TEXT("[7d zoom out] arm=%.1f"), Arm));
	});
	// Camera collision: pitch +30 (camera below the pivot) with the longest arm must be pulled in above the floor.
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const ADemoCharacter* Character = S->Character.Get();
		const FVector CameraLocation = Character->GetFollowCamera()->GetComponentLocation();
		const FVector Pivot = Character->GetCameraBoom()->GetComponentLocation();
		const float Distance = static_cast<float>((CameraLocation - Pivot).Size());
		Test->TestTrue(FString::Printf(TEXT("Camera collision: camera Z %.1f is not below the floor (>= 0)"), CameraLocation.Z), CameraLocation.Z >= 0.f);
		Test->TestTrue(FString::Printf(TEXT("Camera collision: camera pulled in to %.1f < arm %.1f"), Distance, Character->GetArmLength()), Distance < Character->GetArmLength() - 5.f);
		Test->AddInfo(FString::Printf(TEXT("[7e camera collision] cameraZ=%.1f distance=%.1f arm=%.1f"), CameraLocation.Z, Distance, Character->GetArmLength()));
		Inject(*S, S->PC->ZoomAction, FInputActionValue(100.f));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Arm = S->Character->GetArmLength();
		Test->TestTrue(FString::Printf(TEXT("Zoom +100: arm %.1f clamped at MinArmLength %.1f"), Arm, S->Character->MinArmLength), FMath::IsNearlyEqual(Arm, S->Character->MinArmLength, 0.1f));
		Test->AddInfo(FString::Printf(TEXT("[7f zoom in] arm=%.1f"), Arm));
		Inject(*S, S->PC->ResetCameraAction, FInputActionValue(true));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const ADemoCharacter* Character = S->Character.Get();
		Test->TestTrue(FString::Printf(TEXT("R: arm %.1f == default %.1f"), Character->GetArmLength(), Character->DefaultArmLength), FMath::IsNearlyEqual(Character->GetArmLength(), Character->DefaultArmLength, 0.1f));
		Test->TestTrue(FString::Printf(TEXT("R: pitch %.2f == default %.1f"), Character->GetCameraPitch(), Character->DefaultPitch), FMath::IsNearlyEqual(Character->GetCameraPitch(), Character->DefaultPitch, 0.1f));
		Test->TestTrue(FString::Printf(TEXT("R: yaw %.2f == actor yaw %.2f"), Character->GetCameraYaw(), Character->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(Character->GetCameraYaw() - Character->GetActorRotation().Yaw)) < 0.5f);
		Test->AddInfo(FString::Printf(TEXT("[7g reset camera] arm=%.1f pitch=%.2f yaw=%.2f"), Character->GetArmLength(), Character->GetCameraPitch(), Character->GetCameraYaw()));
	});

	// ---------------------------------------------------------------- 8. Focus loss (real key-state path: PlayerController::InputKey)
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		SendKey(*S, EKeys::LeftShift, IE_Pressed);
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Focus: held Shift+W via InputKey runs (speed %.1f >= 550)"), Speed), Speed >= 550.f);
		Test->TestTrue(TEXT("Focus: IsRunning before focus loss"), S->Character->IsRunning());
		Test->AddInfo(FString::Printf(TEXT("[8a keys held] speed=%.1f"), Speed));
		FSlateApplication::Get().OnApplicationActivationStateChanged().Broadcast(false);
		Test->TestFalse(TEXT("Focus: IsRunning() false right after focus loss"), S->Character->IsRunning());
		S->StepStartLocation = S->Character->GetActorLocation();
	});
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Focus: speed %.2f < 5 within 1 s of focus loss (W was still 'held')"), Speed), Speed < 5.f);
		Test->AddInfo(FString::Printf(TEXT("[8b after focus loss] speed=%.2f running=%d"), Speed, S->Character->IsRunning() ? 1 : 0));
		FSlateApplication::Get().OnApplicationActivationStateChanged().Broadcast(true);
		S->StepStartLocation = S->Character->GetActorLocation();
	});
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		const float Moved = static_cast<float>((S->Character->GetActorLocation() - S->StepStartLocation).Size2D());
		Test->TestTrue(FString::Printf(TEXT("Focus back: speed %.2f < 5 and moved %.1f cm < 5 until a new key press"), Speed, Moved), Speed < 5.f && Moved < 5.f);
		Test->TestFalse(TEXT("Focus back: not running"), S->Character->IsRunning());
		Test->AddInfo(FString::Printf(TEXT("[8c focus back] speed=%.2f moved=%.1f"), Speed, Moved));
		// A fresh press works again.
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Focus back: a new W press moves again (speed %.1f >= 250)"), Speed), Speed >= 250.f);
		Test->AddInfo(FString::Printf(TEXT("[8d new press] speed=%.1f"), Speed));
		SendKey(*S, EKeys::W, IE_Released);
		SendKey(*S, EKeys::LeftShift, IE_Released);
	});
	Wait(1.0);

	// 8e-8g. OS key repeat after focus regain: W held through the focus loss keeps sending IE_Repeat.
	// Enhanced Input counts IE_Repeat as "down" and UPlayerInput rebuilds a pressed state from the first
	// repeat after FlushPressedKeys, so without the controller's repeat filter this would resume walking.
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.0);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Repeat: W held via InputKey walks (speed %.1f >= 250)"), Speed), Speed >= 250.f);
		FSlateApplication::Get().OnApplicationActivationStateChanged().Broadcast(false);
		Test->TestTrue(TEXT("Repeat: controller ignores repeats after focus loss"), S->PC->IsIgnoringRepeatUntilPress());
		Test->AddInfo(FString::Printf(TEXT("[8e W held, focus lost] speed=%.1f"), Speed));
	});
	Wait(0.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		FSlateApplication::Get().OnApplicationActivationStateChanged().Broadcast(true);
		S->StepStartLocation = S->Character->GetActorLocation();
		S->MaxSpeedSeen = 0.0;
	});
	Hold(1.0,
		[=](double)
		{
			if (!S->PC.IsValid() || !S->Character.IsValid()) { return; }
			SendKey(*S, EKeys::W, IE_Repeat);
			S->MaxSpeedSeen = FMath::Max(S->MaxSpeedSeen, static_cast<double>(Speed2D(*S)));
		},
		[=]()
		{
			if (!Valid()) { return; }
			const float Speed = Speed2D(*S);
			const float Moved = static_cast<float>((S->Character->GetActorLocation() - S->StepStartLocation).Size2D());
			Test->TestTrue(FString::Printf(TEXT("Repeat: W IE_Repeat for 1 s after focus regain does not move (speed %.2f < 5, peak %.2f < 5, moved %.1f cm < 5)"), Speed, S->MaxSpeedSeen, Moved),
				Speed < 5.f && S->MaxSpeedSeen < 5.0 && Moved < 5.f);
			Test->AddInfo(FString::Printf(TEXT("[8f repeat only] speed=%.2f peak=%.2f moved=%.1f"), Speed, S->MaxSpeedSeen, Moved));
			SendKey(*S, EKeys::W, IE_Pressed);
		});
	Hold(1.2,
		[=](double)
		{
			if (!S->PC.IsValid()) { return; }
			SendKey(*S, EKeys::W, IE_Repeat); // still held after the fresh press
		},
		[=]()
		{
			if (!Valid()) { return; }
			const float Speed = Speed2D(*S);
			Test->TestTrue(FString::Printf(TEXT("Repeat: a fresh W press walks again (speed %.1f within 250..310)"), Speed), Speed >= 250.f && Speed <= 310.f);
			Test->AddInfo(FString::Printf(TEXT("[8g fresh press] speed=%.1f"), Speed));
			SendKey(*S, EKeys::W, IE_Released);
		});
	Wait(1.0);

	// ---------------------------------------------------------------- 9. Fall safety
	Step([=]()
	{
		if (!Valid()) { return; }
		// KillZ is relative to the start transform: start Z + KillZOffset.
		const float FallZ = S->StartLocation.Z + S->Character->KillZOffset - 100.f;
		S->Character->SetActorLocation(FVector(200.f, 200.f, FallZ), false, nullptr, ETeleportType::TeleportPhysics);
		Test->AddInfo(FString::Printf(TEXT("[9 fall] teleported to Z %.1f (start Z %.1f + KillZOffset %.1f - 100)"), FallZ, S->StartLocation.Z, S->Character->KillZOffset));
	});
	Wait(1.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		const FVector Location = S->Character->GetActorLocation();
		const float Distance = static_cast<float>((Location - S->StartLocation).Size());
		Test->TestTrue(FString::Printf(TEXT("Fall: back within 50 cm of start (%.1f cm, Z %.1f)"), Distance, Location.Z), Distance < 50.f);
		Test->AddInfo(FString::Printf(TEXT("[9 fall] location=%s distance=%.1f"), *Location.ToString(), Distance));
	});

	// ---------------------------------------------------------------- 10. Esc: cursor mode ignores Move, toggling back restores it
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		Inject(*S, S->PC->ToggleCursorAction, FInputActionValue(true));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		Test->TestTrue(TEXT("Esc: cursor mode on"), S->PC->IsCursorMode());
		Test->TestTrue(TEXT("Esc: mouse cursor shown"), S->PC->bShowMouseCursor);
	});
	HoldMove(1.5, FVector2D(0.f, 1.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		const float Moved = static_cast<float>((S->Character->GetActorLocation() - S->StepStartLocation).Size2D());
		Test->TestTrue(FString::Printf(TEXT("Esc: Move ignored in cursor mode (speed %.2f < 5, moved %.1f cm < 5)"), Speed, Moved), Speed < 5.f && Moved < 5.f);
		Test->AddInfo(FString::Printf(TEXT("[10a cursor mode] speed=%.2f moved=%.1f"), Speed, Moved));
		Inject(*S, S->PC->ToggleCursorAction, FInputActionValue(true));
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		Test->TestFalse(TEXT("Esc again: cursor mode off"), S->PC->IsCursorMode());
		Test->TestFalse(TEXT("Esc again: mouse cursor hidden"), S->PC->bShowMouseCursor);
	});
	HoldMove(1.5, FVector2D(0.f, 1.f), false, nullptr, [=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Esc again: Move works (speed %.1f >= 250)"), Speed), Speed >= 250.f);
		Test->AddInfo(FString::Printf(TEXT("[10b back to game] speed=%.1f"), Speed));
	});
	Wait(0.5);

	// ---------------------------------------------------------------- 11. Key-mapping path (PlayerController::InputKey, no injection)
	// Camera yaw is 0 after ResetToStart, so camera-forward is +X and camera-right is +Y.
	auto KeyMove = [=](const FKey Key, const TCHAR* Label, TFunction<void(const FVector&)> Check)
	{
		Step([=]()
		{
			if (!Valid()) { return; }
			S->Character->ResetToStart();
		});
		Wait(0.5);
		Step([=]()
		{
			if (!Valid()) { return; }
			S->StepStartLocation = S->Character->GetActorLocation();
			SendKey(*S, Key, IE_Pressed);
		});
		Wait(1.5);
		Step([=]()
		{
			if (!Valid()) { return; }
			const FVector Velocity = S->Character->GetVelocity();
			const FVector Delta = S->Character->GetActorLocation() - S->StepStartLocation;
			Check(Velocity);
			Test->AddInfo(FString::Printf(TEXT("[11 key %s] velocity=%s moved=%s yaw=%.1f"), Label, *Velocity.ToString(), *Delta.ToString(), S->Character->GetActorRotation().Yaw));
			SendKey(*S, Key, IE_Released);
		});
		Wait(1.0);
		Step([=]()
		{
			if (!Valid()) { return; }
			const float Speed = Speed2D(*S);
			Test->TestTrue(FString::Printf(TEXT("Key %s released: speed %.2f < 5"), Label, Speed), Speed < 5.f);
		});
	};
	KeyMove(EKeys::A, TEXT("A"), [=](const FVector& V)
	{
		Test->TestTrue(FString::Printf(TEXT("Key A: moves camera-left -Y (vel Y %.1f <= -250, |X| %.1f < 40)"), V.Y, V.X), V.Y <= -250.f && FMath::Abs(V.X) < 40.f);
	});
	KeyMove(EKeys::S, TEXT("S"), [=](const FVector& V)
	{
		Test->TestTrue(FString::Printf(TEXT("Key S: moves backward -X (vel X %.1f <= -250, |Y| %.1f < 40)"), V.X, V.Y), V.X <= -250.f && FMath::Abs(V.Y) < 40.f);
	});
	KeyMove(EKeys::D, TEXT("D"), [=](const FVector& V)
	{
		Test->TestTrue(FString::Printf(TEXT("Key D: moves camera-right +Y (vel Y %.1f >= 250, |X| %.1f < 40)"), V.Y, V.X), V.Y >= 250.f && FMath::Abs(V.X) < 40.f);
	});

	// Mouse look through the axis keys (MouseX / MouseY), then R.
	struct FLook { float Yaw = 0.f; float Pitch = 0.f; };
	TSharedRef<FLook> Look = MakeShared<FLook>();
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
	});
	Wait(0.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		Look->Yaw = S->Character->GetCameraYaw();
		SendAxis(*S, EKeys::MouseX, 100.f); // mouse moved right
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Yaw = S->Character->GetCameraYaw();
		const float DeltaYaw = FRotator::NormalizeAxis(Yaw - Look->Yaw);
		const float MaxDelta = 100.f * S->PC->LookSensitivity + 0.5f;
		Test->TestTrue(FString::Printf(TEXT("Mouse X +100: yaw turns right by %.2f (> 0.5 and <= %.1f)"), DeltaYaw, MaxDelta), DeltaYaw > 0.5f && DeltaYaw <= MaxDelta);
		Test->AddInfo(FString::Printf(TEXT("[11 mouse X] yaw %.2f -> %.2f (delta %.2f = %.4f deg per mouse unit)"), Look->Yaw, Yaw, DeltaYaw, DeltaYaw / 100.f));
		Look->Pitch = S->Character->GetCameraPitch();
		SendAxis(*S, EKeys::MouseY, 100.f); // mouse moved up
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Pitch = S->Character->GetCameraPitch();
		const float DeltaPitch = Pitch - Look->Pitch;
		const float MaxDelta = 100.f * S->PC->LookSensitivity + 0.5f;
		Test->TestTrue(FString::Printf(TEXT("Mouse Y +100 (mouse up): pitch rises by %.2f = look up (> 0.5 and <= %.1f)"), DeltaPitch, MaxDelta), DeltaPitch > 0.5f && DeltaPitch <= MaxDelta);
		Test->AddInfo(FString::Printf(TEXT("[11 mouse Y] pitch %.2f -> %.2f (delta %.2f = %.4f deg per mouse unit)"), Look->Pitch, Pitch, DeltaPitch, DeltaPitch / 100.f));
		SendKey(*S, EKeys::R, IE_Pressed);
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::R, IE_Released);
		const ADemoCharacter* Character = S->Character.Get();
		Test->TestTrue(FString::Printf(TEXT("Key R: arm %.1f == default %.1f"), Character->GetArmLength(), Character->DefaultArmLength), FMath::IsNearlyEqual(Character->GetArmLength(), Character->DefaultArmLength, 0.1f));
		Test->TestTrue(FString::Printf(TEXT("Key R: pitch %.2f == default %.1f"), Character->GetCameraPitch(), Character->DefaultPitch), FMath::IsNearlyEqual(Character->GetCameraPitch(), Character->DefaultPitch, 0.1f));
		Test->TestTrue(FString::Printf(TEXT("Key R: yaw %.2f == actor yaw %.2f"), Character->GetCameraYaw(), Character->GetActorRotation().Yaw), FMath::Abs(FRotator::NormalizeAxis(Character->GetCameraYaw() - Character->GetActorRotation().Yaw)) < 0.5f);
		Test->AddInfo(FString::Printf(TEXT("[11 key R] arm=%.1f pitch=%.2f yaw=%.2f"), Character->GetArmLength(), Character->GetCameraPitch(), Character->GetCameraYaw()));
		SendAxis(*S, EKeys::MouseWheelAxis, 1.f); // one notch forward (zoom in)
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		const ADemoCharacter* Character = S->Character.Get();
		const float Arm = Character->GetArmLength();
		Test->TestTrue(FString::Printf(TEXT("Wheel +1: arm %.1f shorter than %.1f and not run to MinArmLength %.1f"), Arm, Character->DefaultArmLength, Character->MinArmLength),
			Arm < Character->DefaultArmLength - 1.f && Arm > Character->MinArmLength + 1.f);
		Test->AddInfo(FString::Printf(TEXT("[11 wheel +1] arm %.1f -> %.1f (ZoomStep %.1f)"), Character->DefaultArmLength, Arm, Character->ZoomStep));
	});

	// Backspace: respawn after moving away with W.
	Step([=]()
	{
		if (!Valid()) { return; }
		S->Character->ResetToStart();
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::W, IE_Released);
		const float Away = static_cast<float>((S->Character->GetActorLocation() - S->StartLocation).Size2D());
		Test->TestTrue(FString::Printf(TEXT("Backspace: moved away first (%.1f cm > 150)"), Away), Away > 150.f);
		Test->AddInfo(FString::Printf(TEXT("[11 before Backspace] away=%.1f arm=%.1f"), Away, S->Character->GetArmLength()));
		SendKey(*S, EKeys::BackSpace, IE_Pressed);
	});
	Wait(0.7);
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::BackSpace, IE_Released);
		const ADemoCharacter* Character = S->Character.Get();
		const float Distance = static_cast<float>((Character->GetActorLocation() - S->StartLocation).Size2D());
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Backspace: back at start (%.1f cm < 5), speed %.2f < 5"), Distance, Speed), Distance < 5.f && Speed < 5.f);
		Test->TestTrue(FString::Printf(TEXT("Backspace: camera arm %.1f restored to default %.1f"), Character->GetArmLength(), Character->DefaultArmLength), FMath::IsNearlyEqual(Character->GetArmLength(), Character->DefaultArmLength, 0.1f));
		Test->AddInfo(FString::Printf(TEXT("[11 key Backspace] distance=%.1f speed=%.2f arm=%.1f pitch=%.2f"), Distance, Speed, Character->GetArmLength(), Character->GetCameraPitch()));
	});

	// Escape: cursor mode ignores W; Escape again restores it.
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::Escape, IE_Pressed);
	});
	Wait(0.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::Escape, IE_Released);
		Test->TestTrue(TEXT("Key Esc: cursor mode on"), S->PC->IsCursorMode());
		Test->TestTrue(TEXT("Key Esc: mouse cursor shown"), S->PC->bShowMouseCursor);
		S->StepStartLocation = S->Character->GetActorLocation();
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		const float Moved = static_cast<float>((S->Character->GetActorLocation() - S->StepStartLocation).Size2D());
		Test->TestTrue(FString::Printf(TEXT("Key Esc: W ignored in cursor mode (speed %.2f < 5, moved %.1f cm < 5)"), Speed, Moved), Speed < 5.f && Moved < 5.f);
		Test->AddInfo(FString::Printf(TEXT("[11 key Esc cursor] speed=%.2f moved=%.1f"), Speed, Moved));
		SendKey(*S, EKeys::W, IE_Released);
		SendKey(*S, EKeys::Escape, IE_Pressed);
	});
	Wait(0.5);
	Step([=]()
	{
		if (!Valid()) { return; }
		SendKey(*S, EKeys::Escape, IE_Released);
		Test->TestFalse(TEXT("Key Esc again: cursor mode off"), S->PC->IsCursorMode());
		Test->TestFalse(TEXT("Key Esc again: mouse cursor hidden"), S->PC->bShowMouseCursor);
		SendKey(*S, EKeys::W, IE_Pressed);
	});
	Wait(1.2);
	Step([=]()
	{
		if (!Valid()) { return; }
		const float Speed = Speed2D(*S);
		Test->TestTrue(FString::Printf(TEXT("Key Esc again: W works (speed %.1f >= 250)"), Speed), Speed >= 250.f);
		Test->AddInfo(FString::Printf(TEXT("[11 key Esc back] speed=%.1f"), Speed));
		SendKey(*S, EKeys::W, IE_Released);
	});
	Wait(0.5);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
