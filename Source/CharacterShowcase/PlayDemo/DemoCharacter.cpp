#include "PlayDemo/DemoCharacter.h"

#include "Camera/CameraComponent.h"
#include "Character/CharacterProfileData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

ADemoCharacter::ADemoCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Camera rotation must not turn the body; the movement component turns it toward the movement direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 500.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;

	// The engine mannequin faces +Y, so it is turned -90 to face the actor's +X (same as the third-person template).
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator(0.f, -90.f, 0.f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ADemoCharacter::BeginPlay()
{
	Super::BeginPlay();

	StartTransform = GetActorTransform();
	ApplySpeed();
	CameraBoom->TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
}

void ADemoCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The world's own KillZ is usually unset, so the fall safety net is checked here too.
	if (GetActorLocation().Z < StartTransform.GetLocation().Z + KillZOffset)
	{
		ResetToStart();
	}
}

void ADemoCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	ResetToStart();
}

void ADemoCharacter::ApplyProfile(UCharacterProfileData* InProfile)
{
	Profile = InProfile;

	const ADemoCharacter* Defaults = GetClass()->GetDefaultObject<ADemoCharacter>();
	WalkSpeed = Defaults->WalkSpeed;
	RunSpeed = Defaults->RunSpeed;

	USkeletalMeshComponent* MeshComp = GetMesh();

	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("ADemoCharacter::ApplyProfile: profile is null; the character has no mesh (invisible capsule)."));
		MeshComp->SetSkeletalMeshAsset(nullptr);
		ApplySpeed();
		return;
	}

	WalkSpeed = FMath::Max(1.f, Profile->WalkSpeed);
	RunSpeed = FMath::Max(WalkSpeed, Profile->RunSpeed);

	if (Profile->SkeletalMesh)
	{
		MeshComp->SetSkeletalMeshAsset(Profile->SkeletalMesh);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ADemoCharacter::ApplyProfile: profile '%s' has no SkeletalMesh; the character is an invisible capsule."), *GetNameSafe(Profile));
		MeshComp->SetSkeletalMeshAsset(nullptr);
	}

	if (Profile->DefaultAnimClass)
	{
		MeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		MeshComp->SetAnimInstanceClass(Profile->DefaultAnimClass);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ADemoCharacter::ApplyProfile: profile '%s' has no DefaultAnimClass; the mesh will not animate."), *GetNameSafe(Profile));
		MeshComp->SetAnimInstanceClass(nullptr);
	}

	ApplySpeed();
}

void ADemoCharacter::ApplySpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = bRunning ? RunSpeed : WalkSpeed;
}

void ADemoCharacter::SetRunning(bool bInRunning)
{
	bRunning = bInRunning;
	ApplySpeed();
}

void ADemoCharacter::AddMoveInput2D(FVector2D Input)
{
	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	const FVector Direction = (Forward * Input.Y + Right * Input.X).GetClampedToMaxSize(1.f);
	if (!Direction.IsNearlyZero())
	{
		AddMovementInput(Direction, 1.f);
	}
}

void ADemoCharacter::StopMoving()
{
	ConsumeMovementInputVector();
	GetCharacterMovement()->StopMovementImmediately();
}

void ADemoCharacter::ResetToStart()
{
	SetActorLocationAndRotation(StartTransform.GetLocation(), StartTransform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	StopMoving();
	ResetCamera();
}

float ADemoCharacter::ClampCameraPitch(float Pitch) const
{
	return FMath::Clamp(FRotator::NormalizeAxis(Pitch), MinPitch, MaxPitch);
}

float ADemoCharacter::GetCameraPitch() const
{
	return Controller ? FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch) : 0.f;
}

float ADemoCharacter::GetCameraYaw() const
{
	return Controller ? FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw) : 0.f;
}

float ADemoCharacter::GetArmLength() const
{
	return CameraBoom->TargetArmLength;
}

void ADemoCharacter::AddCameraYaw(float Degrees)
{
	if (Controller)
	{
		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + Degrees);
		Controller->SetControlRotation(Rotation);
	}
}

void ADemoCharacter::AddCameraPitch(float Degrees)
{
	if (Controller)
	{
		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Pitch = ClampCameraPitch(FRotator::NormalizeAxis(Rotation.Pitch) + Degrees);
		Controller->SetControlRotation(Rotation);
	}
}

void ADemoCharacter::ZoomCamera(float Axis)
{
	CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength - Axis * ZoomStep, MinArmLength, MaxArmLength);
}

void ADemoCharacter::ResetCamera()
{
	CameraBoom->TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);

	if (Controller)
	{
		Controller->SetControlRotation(FRotator(ClampCameraPitch(DefaultPitch), GetActorRotation().Yaw, 0.f));
	}
}
