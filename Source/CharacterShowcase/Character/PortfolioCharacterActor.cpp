#include "Character/PortfolioCharacterActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Character/CharacterProfileData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APortfolioCharacterActor::APortfolioCharacterActor()
{
	// Tick is only needed while the turntable is running; it is enabled/disabled by SetTurntableEnabled().
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	SetRootComponent(Mesh);

	// P2-1: query-only collision on the Visibility channel is what the
	// Inspection line trace hits (per-bone, via the mesh's Physics Asset
	// bodies); physics stays off. See Docs/CHARACTER_VIEWER_SETUP.md section 13.11.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// P2-3: selection highlight overlay (section 13.11 -- component-level
	// Custom Depth alone is invisible without a project post-process
	// material we do not ship, so this OverlayMaterial tint is what actually
	// makes a selection visible). A missing asset (e.g. before
	// Scripts/CreatePortfolioAssets.py has run) leaves this null, which is
	// safe: SetSelectedPart() then only sets Custom Depth, no visible tint.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMaterialFinder(TEXT("/Game/Portfolio/Materials/M_ViewerHighlight.M_ViewerHighlight"));
	if (HighlightMaterialFinder.Succeeded())
	{
		HighlightOverlayMaterial = HighlightMaterialFinder.Object;
	}
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

		// P2-3/P2-4: restore selection highlight and wireframe to their
		// off/default state on every profile switch (Docs/CHARACTER_VIEWER_SETUP.md section 7).
		Mesh->SetOverlayMaterial(nullptr);
		Mesh->SetRenderCustomDepth(false);
	}

	AppliedMorphNames.Reset();
	CurrentAnimationId = NAME_None;
	CurrentExpressionId = NAME_None;
	CurrentVariantId = NAME_None;
	SelectedPartId = NAME_None;
	bWireframeEnabled = false;

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

	if (Id != NAME_None)
	{
		if (!Profile || !Profile->FindMaterialVariant(Id))
		{
			return false;
		}
	}

	CurrentVariantId = Id;

	// P2-4 precedence (Docs/CHARACTER_VIEWER_SETUP.md section 13.11): while
	// Wireframe is on, the wireframe material stays visually applied to every
	// slot; a variant selected in the meantime is only recorded (above) and
	// takes visual effect once Wireframe is turned back off.
	if (!bWireframeEnabled)
	{
		ApplyMaterialsForCurrentVariant();
	}

	return true;
}

void APortfolioCharacterActor::ApplyMaterialsForCurrentVariant()
{
	if (!Mesh)
	{
		return;
	}

	// Always restore defaults first so a partial override never leaves a stale slot from a previous variant.
	Mesh->EmptyOverrideMaterials();

	if (CurrentVariantId == NAME_None || !Profile)
	{
		return;
	}

	const FViewerMaterialVariant* Variant = Profile->FindMaterialVariant(CurrentVariantId);
	if (!Variant)
	{
		return;
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

		// Bound-check against the mesh's own material count when it is known
		// (a real SkeletalMesh is assigned). With no asset assigned
		// (GetNumMaterials() == 0, e.g. an editor test with no content
		// asset), there is nothing to validate the authored SlotIndex
		// against, so it is trusted as-is instead of being unconditionally rejected.
		const int32 NumMaterials = Mesh->GetNumMaterials();
		if (ResolvedIndex == INDEX_NONE || ResolvedIndex < 0 || (NumMaterials > 0 && ResolvedIndex >= NumMaterials))
		{
			UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::ApplyMaterialsForCurrentVariant: could not resolve a valid slot for variant '%s'."), *CurrentVariantId.ToString());
			continue;
		}

		Mesh->SetMaterial(ResolvedIndex, SlotOverride.Material);
	}
}

void APortfolioCharacterActor::SetSelectedPart(FName PartId)
{
	if (PartId != NAME_None && (!Profile || !Profile->FindPart(PartId)))
	{
		UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::SetSelectedPart: unknown part '%s' ignored."), *PartId.ToString());
		return;
	}

	SelectedPartId = PartId;
	ApplyHighlightState();
}

void APortfolioCharacterActor::SetHighlightVisible(bool bVisible)
{
	bHighlightVisible = bVisible;
	ApplyHighlightState();
}

void APortfolioCharacterActor::ApplyHighlightState()
{
	if (!Mesh)
	{
		return;
	}

	if (SelectedPartId == NAME_None || !bHighlightVisible)
	{
		Mesh->SetOverlayMaterial(nullptr);
		Mesh->SetRenderCustomDepth(false);
		return;
	}

	// P2-3: Custom Depth is per-component, so with this placeholder's single
	// SkeletalMeshComponent the whole mesh is flagged, not just the selected
	// part's region (documented limitation, section 13.11). The
	// OverlayMaterial tint is what actually makes the selection visible
	// without a project-supplied post-process material; it also covers the
	// whole mesh.
	Mesh->SetRenderCustomDepth(true);
	Mesh->SetCustomDepthStencilValue(1);
	Mesh->SetOverlayMaterial(HighlightOverlayMaterial);
}

void APortfolioCharacterActor::ClearSelectedPart()
{
	SetSelectedPart(NAME_None);
}

bool APortfolioCharacterActor::SetWireframeEnabled(bool bEnabled)
{
	if (!Mesh || !Profile || !Profile->WireframeMaterial)
	{
		return false;
	}

	bWireframeEnabled = bEnabled;

	if (bEnabled)
	{
		// Mesh->GetNumMaterials() is 0 without an assigned SkeletalMesh (e.g.
		// an editor test with no content asset); GetNumOverrideMaterials()
		// still reflects any slot ApplyMaterialsForCurrentVariant() has
		// already populated, so every slot that is actually in use gets wireframed.
		const int32 NumMaterials = FMath::Max(Mesh->GetNumMaterials(), Mesh->GetNumOverrideMaterials());
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			Mesh->SetMaterial(SlotIndex, Profile->WireframeMaterial);
		}
	}
	else
	{
		// Re-select the current variant by id instead of restoring a
		// snapshotted override array, so Variant -> Wireframe -> Variant
		// restores exactly (Docs/CHARACTER_VIEWER_SETUP.md section 7 pitfall).
		ApplyMaterialsForCurrentVariant();
	}

	// The selection highlight overlay (a separate OverlayMaterial slot) is
	// untouched here, so Wireframe and a part selection coexist.
	return true;
}
