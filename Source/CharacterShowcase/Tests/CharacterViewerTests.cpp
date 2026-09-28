#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

// None of these tests require any content asset (Skeletal Mesh, Animation
// Sequence, Material, etc.); see Docs/CHARACTER_VIEWER_SETUP.md section 10.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerActorFeatureNullSafetyTest,
	"CharacterShowcase.Viewer.ActorFeatureNullSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerActorFeatureNullSafetyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}

	APortfolioCharacterActor* Actor = World->SpawnActor<APortfolioCharacterActor>();
	if (!TestNotNull(TEXT("Character actor exists"), Actor))
	{
		World->DestroyWorld(false);
		return false;
	}

	// No profile at all: setters on any id must fail safely, not crash.
	TestFalse(TEXT("SetAnimation fails without a profile"), Actor->SetAnimation(FName(TEXT("Idle"))));
	TestFalse(TEXT("SetExpression fails without a profile"), Actor->SetExpression(FName(TEXT("Smile"))));
	TestFalse(TEXT("SetMaterialVariant fails without a profile"), Actor->SetMaterialVariant(FName(TEXT("Alt"))));

	// A profile with empty arrays: unknown ids must still fail safely.
	UCharacterProfileData* EmptyProfile = NewObject<UCharacterProfileData>(World);
	Actor->ApplyProfile(EmptyProfile);
	TestFalse(TEXT("SetAnimation fails for an unknown id"), Actor->SetAnimation(FName(TEXT("Idle"))));
	TestFalse(TEXT("SetExpression fails for an unknown id"), Actor->SetExpression(FName(TEXT("Smile"))));
	TestFalse(TEXT("SetMaterialVariant fails for an unknown id"), Actor->SetMaterialVariant(FName(TEXT("Alt"))));

	// Turntable: AdvanceTurntable is exposed publicly so it can be simulated without a running Tick loop.
	const float StartYaw = Actor->GetActorRotation().Yaw;
	Actor->AdvanceTurntable(1.f);
	TestEqual(TEXT("Turntable does not move while disabled"), (float)Actor->GetActorRotation().Yaw, StartYaw, 0.01f);

	Actor->SetTurntableEnabled(true);
	TestTrue(TEXT("Turntable reports enabled"), Actor->IsTurntableEnabled());
	Actor->AdvanceTurntable(1.f);
	const float ExpectedYaw = StartYaw + EmptyProfile->TurntableSpeedDegreesPerSecond * 1.f;
	TestEqual(TEXT("Turntable yaw advances by speed * delta seconds"), (float)Actor->GetActorRotation().Yaw, ExpectedYaw, 0.01f);

	// ApplyProfile(nullptr) must reset turntable rotation and every tracked selection id.
	Actor->ApplyProfile(nullptr);
	TestEqual(TEXT("ApplyProfile(nullptr) resets turntable rotation"), (float)Actor->GetActorRotation().Yaw, StartYaw, 0.01f);
	TestTrue(TEXT("ApplyProfile(nullptr) clears animation id"), Actor->GetCurrentAnimationId() == NAME_None);
	TestTrue(TEXT("ApplyProfile(nullptr) clears expression id"), Actor->GetCurrentExpressionId() == NAME_None);
	TestTrue(TEXT("ApplyProfile(nullptr) clears variant id"), Actor->GetCurrentVariantId() == NAME_None);
	TestNull(TEXT("ApplyProfile(nullptr) leaves no mesh"), Actor->Mesh->GetSkeletalMeshAsset());

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerCameraClampTest,
	"CharacterShowcase.Viewer.CameraClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerCameraClampTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}

	ACharacterViewerCameraPawn* CameraPawn = World->SpawnActor<ACharacterViewerCameraPawn>();
	if (!TestNotNull(TEXT("Camera pawn exists"), CameraPawn))
	{
		World->DestroyWorld(false);
		return false;
	}

	FViewerCameraFraming Framing;
	Framing.Distance = 300.f;
	Framing.FOV = 60.f;
	Framing.MinDistance = 100.f;
	Framing.MaxDistance = 500.f;
	Framing.MinPitch = -45.f;
	Framing.MaxPitch = 45.f;

	CameraPawn->SetFraming(Framing, true);
	TestEqual(TEXT("Instant framing sets distance"), CameraPawn->GetDistance(), 300.f, 0.01f);
	TestEqual(TEXT("Instant framing resets pitch to 0"), CameraPawn->GetPitch(), 0.f, 0.01f);
	TestEqual(TEXT("Instant framing resets yaw to 0"), CameraPawn->GetYaw(), 0.f, 0.01f);

	// A very large orbit delta must clamp to the framing's pitch limits, not overshoot.
	CameraPawn->Orbit(FVector2D(0.f, 100000.f));
	TestEqual(TEXT("Pitch clamps to MaxPitch"), CameraPawn->GetPitch(), 45.f, 0.01f);

	CameraPawn->Orbit(FVector2D(0.f, -200000.f));
	TestEqual(TEXT("Pitch clamps to MinPitch"), CameraPawn->GetPitch(), -45.f, 0.01f);

	// Zoom beyond the limits must clamp to Min/MaxDistance.
	CameraPawn->Zoom(-1000.f);
	TestEqual(TEXT("Zoom-out clamps to MaxDistance"), CameraPawn->GetDistance(), 500.f, 0.01f);

	CameraPawn->Zoom(1000.f);
	TestEqual(TEXT("Zoom-in clamps to MinDistance"), CameraPawn->GetDistance(), 100.f, 0.01f);

	// ResetToFraming(): drive the interpolation to completion with one large Tick, then check it reached the defaults.
	CameraPawn->ResetToFraming();
	TestTrue(TEXT("ResetToFraming starts an interpolation"), CameraPawn->IsInterpolating());
	CameraPawn->Tick(10.f); // InterpolationDuration defaults to 0.35s; a 10s delta finishes it in one step.
	TestFalse(TEXT("Interpolation finished after a large Tick"), CameraPawn->IsInterpolating());
	TestEqual(TEXT("Reset restores Distance"), CameraPawn->GetDistance(), 300.f, 0.5f);
	TestEqual(TEXT("Reset restores Pitch"), CameraPawn->GetPitch(), 0.f, 0.5f);
	TestEqual(TEXT("Reset restores Yaw"), CameraPawn->GetYaw(), 0.f, 0.5f);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterViewerProfileLookupTest,
	"CharacterShowcase.Viewer.ProfileLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterViewerProfileLookupTest::RunTest(const FString& Parameters)
{
	UCharacterProfileData* Profile = NewObject<UCharacterProfileData>();
	if (!TestNotNull(TEXT("Profile exists"), Profile))
	{
		return false;
	}

	FViewerCameraPreset FacePreset;
	FacePreset.Id = FName(TEXT("Face"));
	FacePreset.Framing.Distance = 80.f;
	Profile->CameraPresets.Add(FacePreset);

	FViewerCameraPreset FullPreset;
	FullPreset.Id = FName(TEXT("Full"));
	FullPreset.Framing.Distance = 350.f;
	Profile->CameraPresets.Add(FullPreset);

	Profile->DefaultPresetId = FName(TEXT("Full"));
	Profile->DefaultFraming.Distance = 999.f;

	FViewerAnimationEntry IdleEntry;
	IdleEntry.Id = FName(TEXT("Idle"));
	Profile->Animations.Add(IdleEntry);

	FViewerExpression SmileExpression;
	SmileExpression.Id = FName(TEXT("Smile"));
	Profile->Expressions.Add(SmileExpression);

	FViewerMaterialVariant AltVariant;
	AltVariant.Id = FName(TEXT("Alt"));
	Profile->MaterialVariants.Add(AltVariant);

	const FViewerCameraPreset* FoundPreset = Profile->FindPreset(FName(TEXT("Face")));
	if (TestNotNull(TEXT("FindPreset finds 'Face'"), FoundPreset))
	{
		TestEqual(TEXT("'Face' preset distance"), FoundPreset->Framing.Distance, 80.f, 0.01f);
	}
	TestNull(TEXT("FindPreset returns null for an unknown id"), Profile->FindPreset(FName(TEXT("Unknown"))));

	TestNotNull(TEXT("FindAnimation finds 'Idle'"), Profile->FindAnimation(FName(TEXT("Idle"))));
	TestNull(TEXT("FindAnimation returns null for an unknown id"), Profile->FindAnimation(FName(TEXT("Walk"))));

	TestNotNull(TEXT("FindExpression finds 'Smile'"), Profile->FindExpression(FName(TEXT("Smile"))));
	TestNull(TEXT("FindExpression returns null for an unknown id"), Profile->FindExpression(FName(TEXT("Angry"))));

	TestNotNull(TEXT("FindMaterialVariant finds 'Alt'"), Profile->FindMaterialVariant(FName(TEXT("Alt"))));
	TestNull(TEXT("FindMaterialVariant returns null for an unknown id"), Profile->FindMaterialVariant(FName(TEXT("Other"))));

	// GetResetFraming(): DefaultPresetId found -> that preset's framing, not DefaultFraming.
	const FViewerCameraFraming ResetFraming = Profile->GetResetFraming();
	TestEqual(TEXT("GetResetFraming picks the DefaultPresetId preset"), ResetFraming.Distance, 350.f, 0.01f);

	// DefaultPresetId not found -> falls back to DefaultFraming.
	Profile->DefaultPresetId = FName(TEXT("Missing"));
	const FViewerCameraFraming FallbackFraming = Profile->GetResetFraming();
	TestEqual(TEXT("GetResetFraming falls back to DefaultFraming"), FallbackFraming.Distance, 999.f, 0.01f);

	return true;
}

#endif
