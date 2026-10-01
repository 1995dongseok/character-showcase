#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ViewerHeightRuler.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Height reference ruler for proportion checks (G key, DISPLAY row
// "Ruler: Off (G)", Docs/CHARACTER_VIEWER_SETUP.md sections 1.7 / 6.20).
//
// A transient actor spawned by ACharacterViewerController on first use (never
// placed in or saved with a level). Local space: Z = 0 is the character's
// feet (bottom of the mesh bounds), +X faces the camera, +Y is screen-left.
// Built from:
//   - a vertical bar (/Engine/BasicShapes/Cube) from 0 to GetRulerTopCm()
//     (200 cm, or the next 50 cm step above a taller character),
//   - a tick every 10 cm (longer/thicker at every 50 cm),
//   - "50 cm" / "100 cm" / ... labels (UTextRenderComponent, engine default
//     text material/font -- Latin only, hence "Height", not Hangul),
//   - an amber marker line + "Height 182 cm" label at the measured height.
// Every part is unlit (M_ViewerRuler, or M_ViewerPartHighlight if that asset
// is missing), with no collision (never blocks the Inspection trace) and no
// shadow. The controller positions/rotates the whole actor every tick while
// it is visible (SetRulerTransform), so nothing here ticks.
UCLASS(NotPlaceable, Transient)
class CHARACTERSHOWCASE_API AViewerHeightRuler : public AActor
{
	GENERATED_BODY()

public:
	AViewerHeightRuler();

	// Rebuilds the ticks/labels for a character HeightCm tall (no-op if the
	// height, rounded to 0.1 cm, did not change). HeightCm <= 0 shows the bare
	// 200 cm scale without the height marker.
	void SetMeasuredHeight(float HeightCm);

	UFUNCTION(BlueprintPure, Category = "Viewer|Ruler")
	float GetMeasuredHeight() const { return MeasuredHeightCm; }

	// Top of the scale: 200 cm, or the next multiple of 50 cm above the
	// measured height for a taller character (capped at MaxRulerTopCm).
	UFUNCTION(BlueprintPure, Category = "Viewer|Ruler")
	float GetRulerTopCm() const { return RulerTopCm; }

	// Pure (unit-tested): max(200, ceil(HeightCm / 50) * 50), capped at MaxRulerTopCm.
	static float ComputeRulerTopCm(float HeightCm);

	// Feet point (world) and the yaw that makes the ruler face the camera.
	void SetRulerTransform(const FVector& FeetLocation, float FacingYaw);

	void SetRulerVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Viewer|Ruler")
	bool IsRulerVisible() const { return !IsHidden(); }

	// Component counts (tests): ticks = RulerTop / 10 + 1, scale labels =
	// RulerTop / 50 minus any hidden because they would overlap the height
	// label, plus 1 height label and 1 height marker when a height is set.
	int32 GetTickCount() const { return Ticks.Num(); }
	int32 GetScaleLabelCount() const { return ScaleLabels.Num(); }
	int32 GetVisibleScaleLabelCount() const;
	bool HasHeightMarker() const { return HeightMarker != nullptr && HeightLabel != nullptr; }

	// "Height 182 cm" (empty without a height).
	FString GetHeightLabelText() const;

	// "Height 182 cm" for HeightCm (rounded to whole cm).
	static FString FormatHeightLabel(float HeightCm);

	static constexpr float DefaultRulerTopCm = 200.f;
	static constexpr float MaxRulerTopCm = 1000.f;
	static constexpr float TickSpacingCm = 10.f;
	static constexpr float LabelSpacingCm = 50.f;

	// Material of the bar/ticks (M_ViewerRuler: unlit, Color parameter).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Viewer|Ruler")
	TObjectPtr<UMaterialInterface> RulerMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Viewer|Ruler")
	TObjectPtr<UStaticMesh> CubeMesh;

	// Bar/tick colour (M_ViewerRuler "Color" parameter) and the height marker colour.
	static const FLinearColor RulerColor;
	static const FLinearColor HeightColor;

private:
	UPROPERTY(VisibleAnywhere, Category = "Viewer|Ruler")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Bar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Ticks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> ScaleLabels;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> HeightMarker;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> HeightLabel;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RulerMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeightMID;

	float MeasuredHeightCm = -1.f;
	float RulerTopCm = DefaultRulerTopCm;
	bool bBuilt = false;

	void Rebuild();
	void DestroyParts();
	UStaticMeshComponent* AddBox(const FName& Name, const FVector& Center, const FVector& SizeCm, UMaterialInterface* Material);
	UTextRenderComponent* AddLabel(const FName& Name, const FVector& Location, const FString& Text, float WorldSize, FColor Color);
};
