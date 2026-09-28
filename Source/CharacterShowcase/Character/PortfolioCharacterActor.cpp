#include "Character/PortfolioCharacterActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Character/CharacterProfileData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

APortfolioCharacterActor::APortfolioCharacterActor()
{
	// Tick is only needed while the turntable is running; it is enabled/disabled by SetTurntableEnabled().
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APortfolioCharacterActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	InitialRotation = GetActorRotation();
	bInitialRotationCaptured = true;
}

void APortfolioCharacterActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyProfile(Profile.Get());
}

void APortfolioCharacterActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceTurntable(DeltaSeconds);
}

void APortfolioCharacterActor::ApplyProfile(UCharacterProfileData* NewProfile)
{
	// Clear everything the previous profile/selection left behind before switching.
	ClearRuntimeState();

	Profile = IsValid(NewProfile) ? NewProfile : nullptr;
	if (Mesh)
	{
		Mesh->SetSkeletalMesh(Profile ? Profile->SkeletalMesh.Get() : nullptr, true);
	}

	RestoreDefaultAnimationState();
}

void APortfolioCharacterActor::ClearRuntimeState()
{
	if (Mesh)
	{
		for (const FName& MorphName : AppliedMorphNames)
		{
			Mesh->SetMorphTarget(MorphName, 0.f);
		}
		Mesh->EmptyOverrideMaterials();
	}

	AppliedMorphNames.Reset();
	CurrentAnimationId = NAME_None;
	CurrentExpressionId = NAME_None;
	CurrentVariantId = NAME_None;

	// Turntable enabled/disabled state intentionally persists across a profile
	// switch (see Docs/CHARACTER_VIEWER_SETUP.md section 4); only rotation resets.
	// Skip the reset until PostInitializeComponents() has actually captured
	// the placed rotation, otherwise this would snap it to identity.
	if (bInitialRotationCaptured)
	{
		SetActorRotation(InitialRotation);
	}
}

void APortfolioCharacterActor::RestoreDefaultAnimationState()
{
	if (!Mesh)
	{
		return;
	}

	if (Profile && Profile->DefaultAnimClass)
	{
		Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Mesh->SetAnimInstanceClass(Profile->DefaultAnimClass);
		CurrentAnimationId = NAME_None;
		return;
	}

	if (Profile && Profile->DefaultAnimationId != NAME_None)
	{
		if (SetAnimation(Profile->DefaultAnimationId))
		{
			return;
		}
	}

	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(nullptr, false);
	CurrentAnimationId = NAME_None;
}

void APortfolioCharacterActor::SetTurntableEnabled(bool bEnabled)
{
	bTurntableEnabled = bEnabled;
	// Tick is only needed while turntable is actually running.
	SetActorTickEnabled(bTurntableEnabled);
}

void APortfolioCharacterActor::AdvanceTurntable(float DeltaSeconds)
{
	if (!bTurntableEnabled)
	{
		return;
	}

	const float SpeedDegreesPerSecond = Profile ? Profile->TurntableSpeedDegreesPerSecond : 20.f;

	FRotator NewRotation = GetActorRotation();
	NewRotation.Yaw += SpeedDegreesPerSecond * DeltaSeconds;
	SetActorRotation(NewRotation);
}

bool APortfolioCharacterActor::SetAnimation(FName Id)
{
	if (!Mesh)
	{
		return false;
	}

	if (Id == NAME_None)
	{
		RestoreDefaultAnimationState();
		return true;
	}

	if (!Profile)
	{
		return false;
	}

	const FViewerAnimationEntry* Entry = Profile->FindAnimation(Id);
	if (!Entry || !Entry->Sequence)
	{
		return false;
	}

	USkeletalMesh* SkeletalMeshAsset = Mesh->GetSkeletalMeshAsset();
	if (!SkeletalMeshAsset)
	{
		return false;
	}

	// Simplest safe compatibility check: the Sequence and the Mesh must share the same Skeleton.
	if (Entry->Sequence->GetSkeleton() != SkeletalMeshAsset->GetSkeleton())
	{
		UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::SetAnimation: sequence skeleton does not match mesh skeleton for id '%s'."), *Id.ToString());
		return false;
	}

	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Mesh->PlayAnimation(Entry->Sequence, Entry->bLoop);

	if (Entry->bIsPose)
	{
		Mesh->Stop();
		Mesh->SetPosition(Entry->PoseTime, false);
	}

	CurrentAnimationId = Id;
	return true;
}

bool APortfolioCharacterActor::SetExpression(FName Id)
{
	if (!Mesh)
	{
		return false;
	}

	// Only reset the morphs the previously-applied expression actually set.
	for (const FName& MorphName : AppliedMorphNames)
	{
		Mesh->SetMorphTarget(MorphName, 0.f);
	}
	AppliedMorphNames.Reset();

	if (Id == NAME_None)
	{
		CurrentExpressionId = NAME_None;
		return true;
	}

	if (!Profile)
	{
		return false;
	}

	const FViewerExpression* Expression = Profile->FindExpression(Id);
	if (!Expression)
	{
		return false;
	}

	USkeletalMesh* SkeletalMeshAsset = Mesh->GetSkeletalMeshAsset();

	for (const FViewerMorphWeight& MorphWeight : Expression->Morphs)
	{
		if (SkeletalMeshAsset && SkeletalMeshAsset->FindMorphTarget(MorphWeight.MorphName) == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::SetExpression: unknown morph target '%s' for expression '%s'."), *MorphWeight.MorphName.ToString(), *Id.ToString());
			continue;
		}

		Mesh->SetMorphTarget(MorphWeight.MorphName, MorphWeight.Weight);
		AppliedMorphNames.Add(MorphWeight.MorphName);
	}

	CurrentExpressionId = Id;
	return true;
}

bool APortfolioCharacterActor::SetMaterialVariant(FName Id)
{
	if (!Mesh)
	{
		return false;
	}

	// Always restore defaults first so a partial override never leaves a stale slot from a previous variant.
	Mesh->EmptyOverrideMaterials();

	if (Id == NAME_None)
	{
		CurrentVariantId = NAME_None;
		return true;
	}

	if (!Profile)
	{
		return false;
	}

	const FViewerMaterialVariant* Variant = Profile->FindMaterialVariant(Id);
	if (!Variant)
	{
		return false;
	}

	for (const FViewerMaterialSlotOverride& SlotOverride : Variant->Slots)
	{
		if (!SlotOverride.Material)
		{
			continue;
		}

		int32 ResolvedIndex = INDEX_NONE;
		if (SlotOverride.SlotName != NAME_None)
		{
			ResolvedIndex = Mesh->GetMaterialIndex(SlotOverride.SlotName);
		}
		if (ResolvedIndex == INDEX_NONE)
		{
			ResolvedIndex = SlotOverride.SlotIndex;
		}

		if (ResolvedIndex == INDEX_NONE || ResolvedIndex < 0 || ResolvedIndex >= Mesh->GetNumMaterials())
		{
			UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::SetMaterialVariant: could not resolve a valid slot for variant '%s'."), *Id.ToString());
			continue;
		}

		Mesh->SetMaterial(ResolvedIndex, SlotOverride.Material);
	}

	CurrentVariantId = Id;
	return true;
}
