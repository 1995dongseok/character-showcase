#pragma once

#include "CoreMinimal.h"
#include "ViewerMeshStats.generated.h"

// Measured (not authored) technical info for the Skeletal Mesh currently shown
// by APortfolioCharacterActor (GetMeshStats()). Computed on demand from one
// LOD's render data (LOD0 unless a LOD is forced, see LODIndex) and the mesh
// asset, so it also works in cooked builds; when the render data is
// unavailable every count is 0 and bValid is false.
USTRUCT(BlueprintType)
struct FViewerMeshStats
{
	GENERATED_BODY()

	// False when there is no mesh or the measured LOD's render data is unavailable.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	bool bValid = false;

	// 0-based LOD that Triangles/Vertices were measured on (0 unless a LOD is
	// forced, APortfolioCharacterActor::SetForcedLOD()).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 LODIndex = 0;

	// Triangles of LODIndex, summed over every render section.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Triangles = 0;

	// Render vertices of LODIndex.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Vertices = 0;

	// Bones in the mesh's reference skeleton.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Bones = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 MaterialSlots = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 LODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 MorphTargets = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	FName SkeletonName;

	// NAME_None when the mesh has no Physics Asset (parts cannot be clicked then).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	FName PhysicsAssetName;

	// Height in cm of the mesh's imported bounds (reference pose, every LOD
	// alike) times the component's Z scale
	// (APortfolioCharacterActor::GetMeshHeightInfo()); 0 without a mesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	float HeightCm = 0.f;
};

// Measured info for one material slot of the current mesh (GetSlotStats()),
// or the sum over a part's MaterialSlotNames (GetPartMeasuredStats()).
// MaterialName/textures describe the mesh asset's own slot material (what the
// artist assigned), not a runtime Variant/Wireframe/highlight override.
USTRUCT(BlueprintType)
struct FViewerSlotStats
{
	GENERATED_BODY()

	// INDEX_NONE for a multi-slot sum (GetPartMeasuredStats with 2+ slots).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	FName SlotName;

	// Triangles of every render section that uses this slot, in the measured
	// LOD (LOD0 unless a LOD is forced).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 Triangles = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	FName MaterialName;

	// e.g. "3 tex, max 2048x2048" or "no texture".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	FString TextureSummary;

	// Largest texture dimension (max of width/height) used by the material, 0 if none.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 MaxTextureSize = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	int32 TextureCount = 0;
};
