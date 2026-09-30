#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Character/CharacterProfileData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayDemo/DemoCharacter.h"

// D1 (Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md section 12): no content asset is
// needed; runs in the NullRHI Editor automation context like the Viewer tests.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoNullSafetyTest,
	"CharacterShowcase.Demo.NullSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDemoNullSafetyTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient world exists"), World))
	{
		return false;
	}
	World->InitializeActorsForPlay(FURL());

	ADemoCharacter* Character = World->SpawnActor<ADemoCharacter>();
	if (!TestNotNull(TEXT("Demo character exists"), Character))
	{
		World->DestroyWorld(false);
		return false;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	// --- Null profile / null mesh / null anim class: no crash, class-default speeds ---
	// Expected diagnostics: 1x null profile, 2x (no mesh + no anim class) for the empty profile,
	// 2x2 for the mesh-less speed profile applied twice.
	AddExpectedMessage(TEXT("ADemoCharacter::ApplyProfile"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 7);

	Character->ApplyProfile(nullptr);
	TestNull(TEXT("ApplyProfile(nullptr) leaves no mesh"), Character->GetMesh()->GetSkeletalMeshAsset());
	TestEqual(TEXT("Null profile keeps default WalkSpeed"), Character->WalkSpeed, 300.f);
	TestEqual(TEXT("Null profile keeps default RunSpeed"), Character->RunSpeed, 600.f);
	TestEqual(TEXT("Null profile MaxWalkSpeed == WalkSpeed"), Movement->MaxWalkSpeed, 300.f);

	UCharacterProfileData* EmptyProfile = NewObject<UCharacterProfileData>(World);
	Character->ApplyProfile(EmptyProfile);
	TestNull(TEXT("Profile without mesh leaves no mesh"), Character->GetMesh()->GetSkeletalMeshAsset());
	TestNull(TEXT("Profile without anim class leaves no anim class"), Character->GetMesh()->GetAnimClass());
	TestEqual(TEXT("Default profile WalkSpeed is 300"), Character->WalkSpeed, 300.f);
	TestEqual(TEXT("Default profile RunSpeed is 600"), Character->RunSpeed, 600.f);
	TestEqual(TEXT("Default profile MaxWalkSpeed is 300"), Movement->MaxWalkSpeed, 300.f);

	// --- Run toggles MaxWalkSpeed between the profile's RunSpeed and WalkSpeed ---
	UCharacterProfileData* SpeedProfile = NewObject<UCharacterProfileData>(World);
	SpeedProfile->WalkSpeed = 200.f;
	SpeedProfile->RunSpeed = 500.f;
	Character->ApplyProfile(SpeedProfile);
	TestEqual(TEXT("Profile WalkSpeed applied"), Movement->MaxWalkSpeed, 200.f);
	Character->SetRunning(true);
	TestTrue(TEXT("IsRunning after SetRunning(true)"), Character->IsRunning());
	TestEqual(TEXT("SetRunning(true) -> MaxWalkSpeed == RunSpeed"), Movement->MaxWalkSpeed, 500.f);
	Character->SetRunning(false);
	TestFalse(TEXT("Not running after SetRunning(false)"), Character->IsRunning());
	TestEqual(TEXT("SetRunning(false) -> MaxWalkSpeed == WalkSpeed"), Movement->MaxWalkSpeed, 200.f);

	// ApplyProfile while running keeps the run flag and picks up the new RunSpeed.
	Character->SetRunning(true);
	SpeedProfile->RunSpeed = 450.f;
	Character->ApplyProfile(SpeedProfile);
	TestEqual(TEXT("ApplyProfile while running uses the new RunSpeed"), Movement->MaxWalkSpeed, 450.f);
	Character->SetRunning(false);

	// --- Camera: zoom clamp, pitch clamp, reset (no controller: must not crash) ---
	Character->ZoomCamera(-1000.f);
	TestEqual(TEXT("Zoom-out clamps to MaxArmLength"), Character->GetArmLength(), Character->MaxArmLength);
	Character->ZoomCamera(1000.f);
	TestEqual(TEXT("Zoom-in clamps to MinArmLength"), Character->GetArmLength(), Character->MinArmLength);
	Character->ZoomCamera(-1.f);
	TestEqual(TEXT("One wheel notch changes the arm by ZoomStep"), Character->GetArmLength(), Character->MinArmLength + Character->ZoomStep);

	TestEqual(TEXT("Pitch above range clamps to MaxPitch"), Character->ClampCameraPitch(80.f), Character->MaxPitch);
	TestEqual(TEXT("Pitch below range clamps to MinPitch"), Character->ClampCameraPitch(-80.f), Character->MinPitch);
	TestEqual(TEXT("Pitch 350 (== -10) is inside the range"), Character->ClampCameraPitch(350.f), -10.f, 0.01f);

	Character->AddCameraYaw(10.f);
	Character->AddCameraPitch(10.f);
	Character->ResetCamera();
	TestEqual(TEXT("ResetCamera restores DefaultArmLength"), Character->GetArmLength(), Character->DefaultArmLength);

	// Move/stop/reset without a controller must be safe no-ops.
	Character->AddMoveInput2D(FVector2D(1.f, 1.f));
	Character->StopMoving();
	Character->ResetToStart();

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
