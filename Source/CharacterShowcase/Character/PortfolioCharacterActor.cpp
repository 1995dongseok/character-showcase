#include "Character/PortfolioCharacterActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Character/CharacterProfileData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "UObject/ConstructorHelpers.h"

namespace PortfolioCharacterActorPrivate
{
	// Mesh material slot used by render section SectionIndex of LODIndex
	// (LODMaterialMap remap first, as USkinnedMeshComponent does, else the
	// section's own index).
	int32 GetSectionMaterialIndex(const USkeletalMesh& MeshAsset, int32 LODIndex, int32 SectionIndex, const FSkelMeshRenderSection& Section)
	{
		if (const FSkeletalMeshLODInfo* LODInfo = MeshAsset.GetLODInfo(LODIndex))
		{
			if (LODInfo->LODMaterialMap.IsValidIndex(SectionIndex) && LODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
			{
				return LODInfo->LODMaterialMap[SectionIndex];
			}
		}
		return Section.MaterialIndex;
	}

	const FSkeletalMeshLODRenderData* GetLODRenderData(const USkeletalMesh* MeshAsset, int32 LODIndex)
	{
		const FSkeletalMeshRenderData* RenderData = MeshAsset ? MeshAsset->GetResourceForRendering() : nullptr;
		return (RenderData && RenderData->LODRenderData.IsValidIndex(LODIndex)) ? &RenderData->LODRenderData[LODIndex] : nullptr;
	}

	// Triangles of every render section of LODIndex that uses one of SlotIndices.
	int32 CountSlotTriangles(const USkeletalMesh& MeshAsset, const TArray<int32>& SlotIndices, int32 LODIndex)
	{
		int32 Triangles = 0;
		if (const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(&MeshAsset, LODIndex))
		{
			for (int32 SectionIndex = 0; SectionIndex < LODData->RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& Section = LODData->RenderSections[SectionIndex];
				if (SlotIndices.Contains(GetSectionMaterialIndex(MeshAsset, LODIndex, SectionIndex, Section)))
				{
					Triangles += static_cast<int32>(Section.NumTriangles);
				}
			}
		}
		return Triangles;
	}

	void CollectTextures(const UMaterialInterface* Material, TSet<UTexture*>& OutTextures)
	{
		if (!Material)
		{
			return;
		}
		TArray<UTexture*> Used;
		// All quality/feature levels the material has resources for (in a
		// cooked build that is just the cooked one), so this works in
		// Development and Shipping alike.
		Material->GetUsedTextures(Used, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
		Used.Remove(nullptr);
		if (Used.Num() > 0)
		{
			OutTextures.Append(Used);
			return;
		}

		// Fallback when no compiled material resource is available (e.g. an
		// editor run with -NullRHI): the textures referenced by the material
		// graph (cached expression data, also present in cooked builds), with
		// each Material Instance's texture parameter overrides applied from the
		// root-most instance outwards (an override replaces its parent's value).
		TSet<UTexture*> Textures;
		for (UObject* Referenced : Material->GetReferencedTextures())
		{
			if (UTexture* Texture = Cast<UTexture>(Referenced))
			{
				Textures.Add(Texture);
			}
		}
		TArray<const UMaterialInstance*> InstanceChain;
		for (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material); Instance && !InstanceChain.Contains(Instance); Instance = Cast<UMaterialInstance>(Instance->Parent))
		{
			InstanceChain.Add(Instance);
		}
		for (int32 Index = InstanceChain.Num() - 1; Index >= 0; --Index)
		{
			const UMaterialInstance* Instance = InstanceChain[Index];
			for (const FTextureParameterValue& Override : Instance->TextureParameterValues)
			{
				UTexture* ParentValue = nullptr;
				if (Instance->Parent && Instance->Parent->GetTextureParameterValue(Override.ParameterInfo, ParentValue) && ParentValue && ParentValue != Override.ParameterValue)
				{
					Textures.Remove(ParentValue);
				}
				if (Override.ParameterValue)
				{
					Textures.Add(Override.ParameterValue);
				}
			}
		}
		OutTextures.Append(Textures);
	}

	void FillTextureInfo(const TSet<UTexture*>& Textures, FViewerSlotStats& Out)
	{
		Out.TextureCount = Textures.Num();
		Out.MaxTextureSize = 0;
		int32 MaxWidth = 0;
		int32 MaxHeight = 0;
		for (const UTexture* Texture : Textures)
		{
			int32 Width = FMath::RoundToInt(Texture->GetSurfaceWidth());
			int32 Height = FMath::RoundToInt(Texture->GetSurfaceHeight());
#if WITH_EDITORONLY_DATA
			// In the editor the platform data may not be built yet (async
			// texture compilation, or a -NullRHI run), which reports 0x0; the
			// imported source size is the artist-facing resolution anyway.
			if ((Width <= 0 || Height <= 0) && Texture->Source.IsValid())
			{
				Width = Texture->Source.GetSizeX();
				Height = Texture->Source.GetSizeY();
			}
#endif
			if (FMath::Max(Width, Height) > Out.MaxTextureSize)
			{
				Out.MaxTextureSize = FMath::Max(Width, Height);
				MaxWidth = Width;
				MaxHeight = Height;
			}
		}
		Out.TextureSummary = Out.TextureCount == 0
			? FString(TEXT("no texture"))
			: FString::Printf(TEXT("%d tex, max %dx%d"), Out.TextureCount, MaxWidth, MaxHeight);
	}
}

APortfolioCharacterActor::APortfolioCharacterActor()
{
	// Tick is only needed while the turntable is running; it is enabled/disabled by SetTurntableEnabled().
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh"));
	SetRootComponent(Mesh);

	// P2-1: query-only collision on the Visibility channel is what the
	// Inspection line trace hits (per-bone, via the mesh's Physics Asset
	// bodies); physics stays off. See Docs/CHARACTER_VIEWER_SETUP.md section 2 (7).
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// P2-3: WholeMesh fallback highlight overlay (component-level Custom
	// Depth alone is invisible without a project post-process material we do
	// not ship). A missing asset (e.g. before Scripts/CreatePortfolioAssets.py
	// has run) leaves this null, which is safe.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightMaterialFinder(TEXT("/Game/Portfolio/Materials/M_ViewerHighlight.M_ViewerHighlight"));
	if (HighlightMaterialFinder.Succeeded())
	{
		HighlightOverlayMaterial = HighlightMaterialFinder.Object;
	}

	// Per-part highlight (MaterialSlots / BoneMarkers). Missing -> every
	// selection falls back to the WholeMesh tint above. Loaded here, so the
	// cooker picks both up as startup packages, like M_ViewerHighlight.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PartHighlightMaterialFinder(TEXT("/Game/Portfolio/Materials/M_ViewerPartHighlight.M_ViewerPartHighlight"));
	if (PartHighlightMaterialFinder.Succeeded())
	{
		PartHighlightMaterial = PartHighlightMaterialFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoneMarkerMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BoneMarkerMeshFinder.Succeeded())
	{
		BoneMarkerMesh = BoneMarkerMeshFinder.Object;
	}

	// Shaded wireframe overlay (EViewerWireframeMode::Overlay). Missing ->
	// Wireframe falls back to the profile's slot-replacing WireframeMaterial.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WireframeOverlayFinder(TEXT("/Game/Portfolio/Materials/M_WireframeOverlay.M_WireframeOverlay"));
	if (WireframeOverlayFinder.Succeeded())
	{
		WireframeOverlayMaterial = WireframeOverlayFinder.Object;
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
		// off/default state on every profile switch (Docs/CHARACTER_VIEWER_SETUP.md section 3).
		Mesh->SetOverlayMaterial(nullptr);
		Mesh->SetRenderCustomDepth(false);
	}
	DestroyBoneMarkers();

	AppliedMorphNames.Reset();
	CurrentAnimationId = NAME_None;
	CurrentExpressionId = NAME_None;
	CurrentVariantId = NAME_None;
	SelectedPartId = NAME_None;
	ActiveHighlightMode = EViewerHighlightMode::None;
	bWireframeEnabled = false;
	bLoggedTintSuppressedByWireframe = false;

	// Playback controls and the forced LOD belong to the previous profile.
	bAnimationPaused = false;
	AnimationPlayRate = 1.f;
	bCurrentAnimationIsPose = false;
	ForcedLODSetting = 0;
	if (Mesh)
	{
		Mesh->SetForcedLOD(0);
	}

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
		bCurrentAnimationIsPose = false;
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
	bCurrentAnimationIsPose = false;
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

bool APortfolioCharacterActor::IsAnimationSkeletonCompatible(const USkeleton* SequenceSkeleton, const USkeletalMesh* MeshAsset)
{
	if (!SequenceSkeleton || !MeshAsset)
	{
		return false;
	}

	const USkeleton* MeshSkeleton = MeshAsset->GetSkeleton();
	if (SequenceSkeleton == MeshSkeleton)
	{
		return true;
	}

	// Explicit "Compatible Skeletons" list (Skeleton editor > Asset Details),
	// either direction. GetCompatibleSkeletons() is runtime data, unlike the
	// editor-only USkeleton::IsCompatibleForEditor().
	auto IsListed = [](const USkeleton* OwnerSkeleton, const USkeleton* Other)
	{
		if (!OwnerSkeleton || !Other)
		{
			return false;
		}
		const FSoftObjectPath OtherPath(Other);
		for (const TSoftObjectPtr<USkeleton>& Compatible : OwnerSkeleton->GetCompatibleSkeletons())
		{
			if (Compatible.ToSoftObjectPath() == OtherPath)
			{
				return true;
			}
		}
		return false;
	};
	if (IsListed(MeshSkeleton, SequenceSkeleton) || IsListed(SequenceSkeleton, MeshSkeleton))
	{
		return true;
	}

	// Same bone hierarchy (every mesh bone, or its nearest ancestor, exists in
	// the sequence's skeleton with a matching parent chain): runtime-safe
	// engine check; a genuinely different rig (e.g. UE4 vs UE5 mannequin,
	// whose clavicles hang off different spine bones) fails it.
	return SequenceSkeleton->IsCompatibleMesh(MeshAsset);
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

	if (!IsAnimationSkeletonCompatible(Entry->Sequence->GetSkeleton(), SkeletalMeshAsset))
	{
		UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::SetAnimation: sequence skeleton is not compatible with the mesh for id '%s'."), *Id.ToString());
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
	bCurrentAnimationIsPose = Entry->bIsPose;
	ApplyPlaybackStateToNewAnimation();
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

	// P2-4 precedence: while Wireframe is on, the wireframe material stays
	// visually applied (and the selected part's highlight slots stay
	// highlighted); the variant takes visual effect once Wireframe is off.
	// ApplyMaterialState() derives all of that from the current state.
	ApplyMaterialState();
	return true;
}

void APortfolioCharacterActor::ApplyMaterialState()
{
	if (!Mesh)
	{
		return;
	}

	// 1. Defaults: never build on the previous override array.
	Mesh->EmptyOverrideMaterials();

	// 2. Current variant's slot overrides.
	ApplyVariantOverrides();

	// 3. Legacy wireframe (ReplaceSlots mode only; the default Overlay mode is
	// drawn by ApplyOverlayState() and leaves every slot alone) over every
	// slot. Mesh->GetNumMaterials() is 0 without an assigned SkeletalMesh
	// (e.g. an editor test with no content asset); GetNumOverrideMaterials()
	// still reflects any slot step 2 populated, so every slot that is
	// actually in use gets wireframed.
	if (GetActiveWireframeMode() == EViewerWireframeMode::ReplaceSlots)
	{
		const int32 NumMaterials = FMath::Max(Mesh->GetNumMaterials(), Mesh->GetNumOverrideMaterials());
		for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
		{
			Mesh->SetMaterial(SlotIndex, Profile->WireframeMaterial);
		}
	}

	// 4. Selected part's slots (MaterialSlots mode) on top of everything.
	if (bHighlightVisible && PartHighlightMaterial && Profile && SelectedPartId != NAME_None)
	{
		if (const FViewerPartInfo* Part = Profile->FindPart(SelectedPartId))
		{
			TArray<int32> SlotIndices;
			ResolvePartSlotIndices(*Part, SlotIndices);
			for (const int32 SlotIndex : SlotIndices)
			{
				Mesh->SetMaterial(SlotIndex, PartHighlightMaterial);
			}
		}
	}
}

void APortfolioCharacterActor::ApplyVariantOverrides()
{
	if (!Mesh || CurrentVariantId == NAME_None || !Profile)
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
			UE_LOG(LogTemp, Warning, TEXT("APortfolioCharacterActor::ApplyVariantOverrides: could not resolve a valid slot for variant '%s'."), *CurrentVariantId.ToString());
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

void APortfolioCharacterActor::ResolvePartSlotIndices(const FViewerPartInfo& Part, TArray<int32>& OutSlotIndices) const
{
	OutSlotIndices.Reset();
	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		return;
	}

	const int32 NumMaterials = Mesh->GetNumMaterials();
	for (const FName& SlotName : Part.MaterialSlotNames)
	{
		if (SlotName == NAME_None)
		{
			continue;
		}
		const int32 SlotIndex = Mesh->GetMaterialIndex(SlotName);
		if (SlotIndex >= 0 && SlotIndex < NumMaterials)
		{
			OutSlotIndices.AddUnique(SlotIndex);
		}
	}
}

EViewerHighlightMode APortfolioCharacterActor::ResolveHighlightMode(const FViewerPartInfo* Part, TArray<int32>& OutSlotIndices, TArray<FName>& OutMarkerBones) const
{
	OutSlotIndices.Reset();
	OutMarkerBones.Reset();
	if (!Part)
	{
		return EViewerHighlightMode::None;
	}

	// a. Separate material slots/sections per part.
	if (PartHighlightMaterial)
	{
		ResolvePartSlotIndices(*Part, OutSlotIndices);
		if (OutSlotIndices.Num() > 0)
		{
			return EViewerHighlightMode::MaterialSlots;
		}
	}

	// b. Bone markers: the part's bones plus their direct children, so e.g.
	// an arm shows shoulder -> elbow -> wrist (-> finger roots).
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (PartHighlightMaterial && BoneMarkerMesh && MeshAsset)
	{
		const FReferenceSkeleton& RefSkeleton = MeshAsset->GetRefSkeleton();
		TArray<int32> ChildBoneIndices;
		for (const FName& BoneName : Part->BoneNames)
		{
			const int32 BoneIndex = BoneName != NAME_None ? RefSkeleton.FindBoneIndex(BoneName) : INDEX_NONE;
			if (BoneIndex == INDEX_NONE)
			{
				continue;
			}
			OutMarkerBones.AddUnique(RefSkeleton.GetBoneName(BoneIndex));

			ChildBoneIndices.Reset();
			RefSkeleton.GetDirectChildBones(BoneIndex, ChildBoneIndices);
			for (const int32 ChildIndex : ChildBoneIndices)
			{
				OutMarkerBones.AddUnique(RefSkeleton.GetBoneName(ChildIndex));
			}
		}
		if (OutMarkerBones.Num() > 0)
		{
			return EViewerHighlightMode::BoneMarkers;
		}
	}

	// c. Nothing part-specific resolves: whole-mesh tint, so a selection is never invisible.
	return EViewerHighlightMode::WholeMesh;
}

void APortfolioCharacterActor::ApplyHighlightState()
{
	if (!Mesh)
	{
		ActiveHighlightMode = EViewerHighlightMode::None;
		return;
	}

	const FViewerPartInfo* Part = (Profile && SelectedPartId != NAME_None) ? Profile->FindPart(SelectedPartId) : nullptr;
	TArray<int32> SlotIndices;
	TArray<FName> MarkerBones;
	const EViewerHighlightMode Mode = ResolveHighlightMode(Part, SlotIndices, MarkerBones);
	ActiveHighlightMode = bHighlightVisible ? Mode : EViewerHighlightMode::None;

	// Slots (MaterialSlots mode, or restoring them when leaving it) are
	// recomputed from scratch together with Variant/Wireframe.
	ApplyMaterialState();

	// Markers: pooled while some part is selected (hidden in Clean View or for
	// a non-marker part), destroyed once the selection is cleared.
	if (ActiveHighlightMode == EViewerHighlightMode::BoneMarkers)
	{
		UpdateBoneMarkers(MarkerBones, true);
	}
	else if (Mode == EViewerHighlightMode::None)
	{
		DestroyBoneMarkers();
	}
	else
	{
		UpdateBoneMarkers(TArray<FName>(), false);
	}

	// The single overlay slot: Wireframe overlay or selection tint.
	ApplyOverlayState();

	// Custom Depth is per-component (whole mesh); harmless, kept for a
	// project-supplied stencil post-process.
	if (ActiveHighlightMode != EViewerHighlightMode::None)
	{
		Mesh->SetRenderCustomDepth(true);
		Mesh->SetCustomDepthStencilValue(1);
	}
	else
	{
		Mesh->SetRenderCustomDepth(false);
	}
}

void APortfolioCharacterActor::UpdateBoneMarkers(const TArray<FName>& MarkerBones, bool bVisible)
{
	if (!Mesh || !BoneMarkerMesh)
	{
		DestroyBoneMarkers();
		return;
	}

	const float MeshDiameter = FMath::Max(1.f, static_cast<float>(BoneMarkerMesh->GetBounds().BoxExtent.GetMax()) * 2.f);
	const float Scale = BoneMarkerDiameter / MeshDiameter;

	for (int32 Index = 0; Index < MarkerBones.Num(); ++Index)
	{
		const FName BoneName = MarkerBones[Index];
		UStaticMeshComponent* Marker = BoneMarkers.IsValidIndex(Index) ? BoneMarkers[Index].Get() : nullptr;
		if (!IsValid(Marker))
		{
			Marker = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			Marker->SetStaticMesh(BoneMarkerMesh);
			Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Marker->SetGenerateOverlapEvents(false);
			Marker->SetCanEverAffectNavigation(false);
			Marker->SetCastShadow(false);
			// Size in world centimetres regardless of the mesh/bone scale.
			Marker->SetUsingAbsoluteScale(true);
			Marker->SetupAttachment(Mesh, BoneName);
			Marker->RegisterComponent();

			if (BoneMarkers.IsValidIndex(Index))
			{
				BoneMarkers[Index] = Marker;
				BoneMarkerBones[Index] = BoneName;
			}
			else
			{
				BoneMarkers.Add(Marker);
				BoneMarkerBones.Add(BoneName);
			}
		}
		else if (BoneMarkerBones[Index] != BoneName || Marker->GetAttachParent() != Mesh)
		{
			Marker->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, BoneName);
			BoneMarkerBones[Index] = BoneName;
		}

		Marker->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Marker->SetRelativeScale3D(FVector(Scale));
		Marker->SetMaterial(0, PartHighlightMaterial);
		Marker->SetVisibility(bVisible);
	}

	// Pooled extras stay alive but hidden.
	for (int32 Index = MarkerBones.Num(); Index < BoneMarkers.Num(); ++Index)
	{
		if (UStaticMeshComponent* Marker = BoneMarkers[Index].Get())
		{
			Marker->SetVisibility(false);
		}
	}
}

void APortfolioCharacterActor::DestroyBoneMarkers()
{
	for (const TObjectPtr<UStaticMeshComponent>& Marker : BoneMarkers)
	{
		if (IsValid(Marker))
		{
			Marker->DestroyComponent();
		}
	}
	BoneMarkers.Reset();
	BoneMarkerBones.Reset();
}

int32 APortfolioCharacterActor::GetVisibleBoneMarkerCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UStaticMeshComponent>& Marker : BoneMarkers)
	{
		if (IsValid(Marker) && Marker->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

TArray<FName> APortfolioCharacterActor::GetBoneMarkerBoneNames() const
{
	TArray<FName> Names;
	for (int32 Index = 0; Index < BoneMarkers.Num(); ++Index)
	{
		const UStaticMeshComponent* Marker = BoneMarkers[Index].Get();
		if (IsValid(Marker) && Marker->IsVisible() && BoneMarkerBones.IsValidIndex(Index))
		{
			Names.Add(BoneMarkerBones[Index]);
		}
	}
	return Names;
}

void APortfolioCharacterActor::ClearSelectedPart()
{
	SetSelectedPart(NAME_None);
}

EViewerWireframeMode APortfolioCharacterActor::ResolveWireframeMode() const
{
	const bool bHasLegacyMaterial = Profile && Profile->WireframeMaterial;
	if (bWireframeReplacesSlots && bHasLegacyMaterial)
	{
		return EViewerWireframeMode::ReplaceSlots;
	}
	if (Profile && WireframeOverlayMaterial)
	{
		return EViewerWireframeMode::Overlay;
	}
	return bHasLegacyMaterial ? EViewerWireframeMode::ReplaceSlots : EViewerWireframeMode::None;
}

bool APortfolioCharacterActor::IsWireframeAvailable() const
{
	return Mesh && ResolveWireframeMode() != EViewerWireframeMode::None;
}

EViewerWireframeMode APortfolioCharacterActor::GetActiveWireframeMode() const
{
	return bWireframeEnabled ? ResolveWireframeMode() : EViewerWireframeMode::None;
}

bool APortfolioCharacterActor::SetWireframeEnabled(bool bEnabled)
{
	if (!Mesh || (bEnabled && !IsWireframeAvailable()))
	{
		return false;
	}

	bWireframeEnabled = bEnabled;

	// Off re-selects the current variant by id (never a snapshotted override
	// array) and re-applies the part highlight; on keeps the highlighted
	// slots highlighted. Slots, markers and the single overlay slot (wireframe
	// overlay vs. selection tint) are all recomputed from the current state.
	ApplyHighlightState();
	return true;
}

void APortfolioCharacterActor::ApplyOverlayState()
{
	if (!Mesh)
	{
		return;
	}

	const bool bWireframeOverlay = GetActiveWireframeMode() == EViewerWireframeMode::Overlay;
	const bool bWantsTint = ActiveHighlightMode == EViewerHighlightMode::WholeMesh
		|| (ActiveHighlightMode == EViewerHighlightMode::BoneMarkers && bWholeMeshTintWithBoneMarkers);

	if (bWireframeOverlay && bWantsTint)
	{
		// One overlay slot per component: the wireframe wins. The selection
		// keeps Custom Depth (stencil 1) and the INSPECTION text.
		if (!bLoggedTintSuppressedByWireframe)
		{
			UE_LOG(LogTemp, Log, TEXT("APortfolioCharacterActor: Wireframe overlay is on; the whole-mesh selection tint for part '%s' is not shown (Custom Depth only) until Wireframe is turned off."), *SelectedPartId.ToString());
			bLoggedTintSuppressedByWireframe = true;
		}
	}
	else
	{
		bLoggedTintSuppressedByWireframe = false;
	}

	UMaterialInterface* Overlay = nullptr;
	if (bWireframeOverlay)
	{
		Overlay = WireframeOverlayMaterial.Get();
	}
	else if (bWantsTint)
	{
		Overlay = HighlightOverlayMaterial.Get();
	}
	Mesh->SetOverlayMaterial(Overlay);
}

FViewerMeshStats APortfolioCharacterActor::GetMeshStats(int32 LODIndex) const
{
	using namespace PortfolioCharacterActorPrivate;

	FViewerMeshStats Stats;
	const int32 MeasuredLOD = LODIndex == INDEX_NONE ? GetDisplayedStatsLOD() : LODIndex;
	Stats.LODIndex = MeasuredLOD;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshLODRenderData* LODData = GetLODRenderData(MeshAsset, MeasuredLOD);
	if (!MeshAsset || !LODData)
	{
		// No mesh, render data unavailable, or no such LOD: all zeros, bValid false.
		return Stats;
	}

	for (const FSkelMeshRenderSection& Section : LODData->RenderSections)
	{
		Stats.Triangles += static_cast<int32>(Section.NumTriangles);
	}
	Stats.Vertices = static_cast<int32>(LODData->GetNumVertices());
	Stats.Bones = MeshAsset->GetRefSkeleton().GetNum();
	Stats.MaterialSlots = MeshAsset->GetMaterials().Num();
	Stats.LODs = MeshAsset->GetLODNum();
	Stats.MorphTargets = MeshAsset->GetMorphTargets().Num();
	Stats.SkeletonName = MeshAsset->GetSkeleton() ? MeshAsset->GetSkeleton()->GetFName() : NAME_None;
	Stats.PhysicsAssetName = MeshAsset->GetPhysicsAsset() ? MeshAsset->GetPhysicsAsset()->GetFName() : NAME_None;
	Stats.bValid = true;
	return Stats;
}

FViewerSlotStats APortfolioCharacterActor::ComputeSlotStats(int32 SlotIndex, int32 LODIndex) const
{
	using namespace PortfolioCharacterActorPrivate;

	FViewerSlotStats Stats;
	Stats.SlotIndex = SlotIndex;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset || !MeshAsset->GetMaterials().IsValidIndex(SlotIndex))
	{
		FillTextureInfo(TSet<UTexture*>(), Stats);
		return Stats;
	}

	const FSkeletalMaterial& SlotMaterial = MeshAsset->GetMaterials()[SlotIndex];
	Stats.SlotName = SlotMaterial.MaterialSlotName;
	Stats.MaterialName = SlotMaterial.MaterialInterface ? SlotMaterial.MaterialInterface->GetFName() : NAME_None;
	Stats.Triangles = CountSlotTriangles(*MeshAsset, TArray<int32>{ SlotIndex }, LODIndex);

	TSet<UTexture*> Textures;
	CollectTextures(SlotMaterial.MaterialInterface, Textures);
	FillTextureInfo(Textures, Stats);
	return Stats;
}

TArray<FViewerSlotStats> APortfolioCharacterActor::GetSlotStats(int32 LODIndex) const
{
	const int32 MeasuredLOD = LODIndex == INDEX_NONE ? GetDisplayedStatsLOD() : LODIndex;
	TArray<FViewerSlotStats> Result;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset)
	{
		return Result;
	}

	for (int32 SlotIndex = 0; SlotIndex < MeshAsset->GetMaterials().Num(); ++SlotIndex)
	{
		Result.Add(ComputeSlotStats(SlotIndex, MeasuredLOD));
	}
	return Result;
}

bool APortfolioCharacterActor::GetPartMeasuredStats(FName PartId, FViewerSlotStats& Out) const
{
	using namespace PortfolioCharacterActorPrivate;

	Out = FViewerSlotStats();
	const FViewerPartInfo* Part = Profile ? Profile->FindPart(PartId) : nullptr;
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Part || !MeshAsset)
	{
		return false;
	}

	TArray<int32> SlotIndices;
	ResolvePartSlotIndices(*Part, SlotIndices);
	if (SlotIndices.Num() == 0)
	{
		return false;
	}

	TArray<FString> SlotNames;
	TArray<FString> MaterialNames;
	TSet<UTexture*> Textures;
	for (const int32 SlotIndex : SlotIndices)
	{
		const FSkeletalMaterial& SlotMaterial = MeshAsset->GetMaterials()[SlotIndex];
		SlotNames.Add(SlotMaterial.MaterialSlotName.ToString());
		if (SlotMaterial.MaterialInterface)
		{
			MaterialNames.AddUnique(SlotMaterial.MaterialInterface->GetName());
		}
		CollectTextures(SlotMaterial.MaterialInterface, Textures);
	}

	Out.SlotIndex = SlotIndices.Num() == 1 ? SlotIndices[0] : INDEX_NONE;
	Out.SlotName = FName(*FString::Join(SlotNames, TEXT(", ")));
	Out.MaterialName = MaterialNames.Num() > 0 ? FName(*FString::Join(MaterialNames, TEXT(", "))) : NAME_None;
	Out.Triangles = CountSlotTriangles(*MeshAsset, SlotIndices, GetDisplayedStatsLOD());
	FillTextureInfo(Textures, Out);
	return true;
}

// --- Animation playback controls -------------------------------------------

float APortfolioCharacterActor::ClampAnimationPlayRate(float Rate)
{
	if (FMath::IsNaN(Rate))
	{
		return 1.f;
	}
	return FMath::Clamp(Rate, MinAnimationPlayRate, MaxAnimationPlayRate);
}

int32 APortfolioCharacterActor::WrapAnimationFrame(int32 Frame, int32 NumFrames)
{
	if (NumFrames <= 0)
	{
		return 0;
	}
	// Frames are 0..NumFrames inclusive (NumFrames + 1 sampled keys).
	const int32 Count = NumFrames + 1;
	return ((Frame % Count) + Count) % Count;
}

UAnimSingleNodeInstance* APortfolioCharacterActor::GetControllableSingleNode(UAnimSequenceBase** OutSequence) const
{
	if (OutSequence)
	{
		*OutSequence = nullptr;
	}
	if (!Mesh || Mesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		return nullptr;
	}
	UAnimSingleNodeInstance* SingleNode = Mesh->GetSingleNodeInstance();
	UAnimSequenceBase* Sequence = SingleNode ? Cast<UAnimSequenceBase>(SingleNode->GetAnimationAsset()) : nullptr;
	if (!Sequence)
	{
		return nullptr;
	}
	if (OutSequence)
	{
		*OutSequence = Sequence;
	}
	return SingleNode;
}

bool APortfolioCharacterActor::IsAnimationPlaybackControllable() const
{
	return GetControllableSingleNode() != nullptr;
}

void APortfolioCharacterActor::ApplyPlaybackStateToNewAnimation()
{
	UAnimSingleNodeInstance* SingleNode = GetControllableSingleNode();
	if (!SingleNode)
	{
		return;
	}
	// PlayAnimation() resets the instance's rate to 1.0; the user's rate persists.
	SingleNode->SetPlayRate(AnimationPlayRate);
	if (bAnimationPaused || bCurrentAnimationIsPose)
	{
		SingleNode->SetPlaying(false);
	}
}

bool APortfolioCharacterActor::SetAnimationPaused(bool bPaused)
{
	UAnimSequenceBase* Sequence = nullptr;
	UAnimSingleNodeInstance* SingleNode = GetControllableSingleNode(&Sequence);
	if (!SingleNode || (!bPaused && bCurrentAnimationIsPose))
	{
		return false;
	}

	bAnimationPaused = bPaused;
	if (!bPaused && !SingleNode->IsLooping() && SingleNode->GetCurrentTime() >= Sequence->GetPlayLength() - UE_KINDA_SMALL_NUMBER)
	{
		// A finished one-shot would otherwise "resume" without moving.
		SingleNode->SetPosition(0.f, false);
	}
	SingleNode->SetPlaying(!bPaused);
	return true;
}

bool APortfolioCharacterActor::IsAnimationPaused() const
{
	return bAnimationPaused || bCurrentAnimationIsPose;
}

bool APortfolioCharacterActor::StepAnimationFrames(int32 Frames)
{
	UAnimSequenceBase* Sequence = nullptr;
	UAnimSingleNodeInstance* SingleNode = GetControllableSingleNode(&Sequence);
	if (!SingleNode)
	{
		return false;
	}

	// Stepping only makes sense on a still pose: pause first (persists).
	bAnimationPaused = true;
	SingleNode->SetPlaying(false);

	float Time = 0.f;
	float Length = 0.f;
	int32 Frame = 0;
	int32 NumFrames = 0;
	GetAnimationTimeInfo(Time, Length, Frame, NumFrames);

	const int32 NewFrame = WrapAnimationFrame(Frame + Frames, NumFrames);
	const FFrameRate FrameRate = Sequence->GetSamplingFrameRate();
	const double FramesPerSecond = FrameRate.IsValid() && FrameRate.AsDecimal() > 0.0 ? FrameRate.AsDecimal() : 30.0;
	const float NewTime = FMath::Clamp(static_cast<float>(NewFrame / FramesPerSecond), 0.f, Length);
	SingleNode->SetPosition(NewTime, false);
	return true;
}

bool APortfolioCharacterActor::SetAnimationPlayRate(float Rate)
{
	UAnimSingleNodeInstance* SingleNode = GetControllableSingleNode();
	if (!SingleNode)
	{
		return false;
	}
	AnimationPlayRate = ClampAnimationPlayRate(Rate);
	SingleNode->SetPlayRate(AnimationPlayRate);
	return true;
}

bool APortfolioCharacterActor::GetAnimationTimeInfo(float& OutTime, float& OutLength, int32& OutFrame, int32& OutNumFrames) const
{
	OutTime = 0.f;
	OutLength = 0.f;
	OutFrame = 0;
	OutNumFrames = 0;

	UAnimSequenceBase* Sequence = nullptr;
	const UAnimSingleNodeInstance* SingleNode = GetControllableSingleNode(&Sequence);
	if (!SingleNode)
	{
		return false;
	}

	OutLength = Sequence->GetPlayLength();
	OutTime = FMath::Clamp(SingleNode->GetCurrentTime(), 0.f, OutLength);

	const FFrameRate FrameRate = Sequence->GetSamplingFrameRate();
	const double FramesPerSecond = FrameRate.IsValid() && FrameRate.AsDecimal() > 0.0 ? FrameRate.AsDecimal() : 30.0;
	const int32 NumKeys = Sequence->GetNumberOfSampledKeys();
	OutNumFrames = NumKeys > 1 ? NumKeys - 1 : FMath::Max(0, FMath::RoundToInt(OutLength * FramesPerSecond));
	OutFrame = FMath::Clamp(FMath::RoundToInt(OutTime * FramesPerSecond), 0, OutNumFrames);
	return true;
}

// --- LOD display -----------------------------------------------------------

int32 APortfolioCharacterActor::GetNumLODs() const
{
	const USkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return MeshAsset ? MeshAsset->GetLODNum() : 0;
}

bool APortfolioCharacterActor::SetForcedLOD(int32 ForcedLOD)
{
	if (!Mesh || ForcedLOD < 0 || ForcedLOD > GetNumLODs())
	{
		return false;
	}
	ForcedLODSetting = ForcedLOD;
	Mesh->SetForcedLOD(ForcedLOD);
	return true;
}
