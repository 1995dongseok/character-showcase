#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterProfileData.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UAnimInstance;
class UMaterialInterface;

// P0-3: camera framing values for one view (full body, face, etc.). Pure data,
// no runtime state (current yaw/pitch/interpolation live in
// ACharacterViewerCameraPawn, not here).
USTRUCT(BlueprintType)
struct FViewerCameraFraming
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector TargetOffset = FVector(0.f, 0.f, 90.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Distance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FOV = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinDistance = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MinPitch = -80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float MaxPitch = 80.f;
};

// P1-1: a named, selectable camera framing (Face / Upper / Full body, etc.).
USTRUCT(BlueprintType)
struct FViewerCameraPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FViewerCameraFraming Framing;
};

// P1-3: one selectable Animation Sequence or still pose.
USTRUCT(BlueprintType)
struct FViewerAnimationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> Sequence = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bLoop = true;

	// If true, playback is stopped and pinned at PoseTime instead of looping/playing once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bIsPose = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	float PoseTime = 0.f;
};

// P1-4: one Morph Target and its target weight, as used by an Expression.
USTRUCT(BlueprintType)
struct FViewerMorphWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
	FName MorphName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
	float Weight = 1.f;
};

// P1-4: a named facial expression. Neutral is represented by an empty Morphs array.
USTRUCT(BlueprintType)
struct FViewerExpression
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
	TArray<FViewerMorphWeight> Morphs;
};

// P1-5: one material slot override, resolved by SlotName first, then SlotIndex.
USTRUCT(BlueprintType)
struct FViewerMaterialSlotOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName SlotName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UMaterialInterface> Material = nullptr;
};

// P1-5: a named, selectable set of material slot overrides (partial overrides allowed).
USTRUCT(BlueprintType)
struct FViewerMaterialVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<FViewerMaterialSlotOverride> Slots;
};

// P2-1/P2-2: one selectable/inspectable part. Identified either by one or more
// bone names (a line-trace hit's BoneName, matched exactly here; the parent-
// bone walk that maps e.g. a finger bone to "Arm" is done by the Controller,
// not here -- see Docs/CHARACTER_VIEWER_SETUP.md section 13.11) or, for a
// structure with separate Components (not this placeholder), a ComponentTag.
// TriangleCount/TextureResolution are authored data measured once from the
// LOD0/material at content-creation time, never inferred per frame (section 7).
USTRUCT(BlueprintType)
struct FViewerPartInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FText PartType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part", meta = (MultiLine = "true"))
	FText Description;

	// Bone-based identification (this placeholder's method): a line-trace hit's
	// BoneName is matched against every part's BoneNames (exact match only;
	// the Controller walks up parent bones for an unmapped bone).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	TArray<FName> BoneNames;

	// Component-based identification, for a structure with separate Components
	// per part (e.g. Face/Hair/Jacket as distinct SkeletalMeshComponents). Not
	// used by this placeholder (a single SkeletalMeshComponent), but supported
	// by the schema so a future real-asset structure does not need a schema change.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FName ComponentTag;

	// Authored data: LOD0 triangle count for this part's mesh region, measured once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	int32 TriangleCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FText MaterialName;

	// Authored data, e.g. "2048x2048" or "N/A (no texture)"; measured once from the material's textures.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	FText TextureResolution;
};

// Character profile data asset. Configuration only: current selection /
// turntable / playback-time / camera runtime state must never be written
// back into this asset (see Docs/CHARACTER_VIEWER_SETUP.md section 8).
UCLASS(BlueprintType)
class CHARACTERSHOWCASE_API UCharacterProfileData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	// P0-3: default full-body framing. Used when CameraPresets is empty, and
	// as the GetResetFraming() fallback when DefaultPresetId is empty or not found.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FViewerCameraFraming DefaultFraming;

	// P1-1: Face / Upper / Full body, etc.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TArray<FViewerCameraPreset> CameraPresets;

	// The full-body preset id used for Reset (R). Empty or not-found falls back to DefaultFraming.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FName DefaultPresetId;

	// P1-3
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TArray<FViewerAnimationEntry> Animations;

	// Optional AnimBP class used as the default playback state instead of a single Sequence.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> DefaultAnimClass;

	// Used only when DefaultAnimClass is unset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	FName DefaultAnimationId;

	// P1-4. Include an entry with Id == "Neutral" (or similar) and an empty Morphs array for the neutral expression.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Expression")
	TArray<FViewerExpression> Expressions;

	// P1-5
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<FViewerMaterialVariant> MaterialVariants;

	// P1-2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turntable")
	float TurntableSpeedDegreesPerSecond = 20.f;

	// P2-0/P2-1/P2-2: inspectable parts. Empty means Inspection has nothing to select
	// (still safe: SetInspectionEnabled/clicks simply never resolve a part).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TArray<FViewerPartInfo> Parts;

	// P2-4: material applied to every slot while Wireframe is on. Null means
	// Wireframe is unavailable for this profile (the Widget disables the button).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TObjectPtr<UMaterialInterface> WireframeMaterial = nullptr;

	// Plain C++ helpers (not UFUNCTION: UHT does not support a raw pointer to a
	// USTRUCT as a Blueprint-exposed return type). Used from C++ only
	// (Actor/Controller/Widget/tests).
	const FViewerCameraPreset* FindPreset(FName Id) const;
	const FViewerAnimationEntry* FindAnimation(FName Id) const;
	const FViewerExpression* FindExpression(FName Id) const;
	const FViewerMaterialVariant* FindMaterialVariant(FName Id) const;

	// P2-2: finds a part by its own Id (exact match, no bone/parent lookup).
	const FViewerPartInfo* FindPart(FName Id) const;

	// P2-1: finds the part whose BoneNames contains Bone exactly (no parent
	// walk; that is the Controller's job -- see
	// Docs/CHARACTER_VIEWER_SETUP.md section 13.11).
	const FViewerPartInfo* FindPartByBone(FName Bone) const;

	// The framing used by Reset (R): the DefaultPresetId preset's framing if
	// found, otherwise DefaultFraming.
	UFUNCTION(BlueprintPure, Category = "Character")
	FViewerCameraFraming GetResetFraming() const;
};
