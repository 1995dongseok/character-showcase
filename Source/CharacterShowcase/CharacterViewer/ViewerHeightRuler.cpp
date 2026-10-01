#include "CharacterViewer/ViewerHeightRuler.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

const FLinearColor AViewerHeightRuler::RulerColor(0.6f, 0.6f, 0.6f, 1.f);
const FLinearColor AViewerHeightRuler::HeightColor(1.f, 0.62f, 0.15f, 1.f);

namespace ViewerHeightRulerPrivate
{
	const FName ColorParameterName(TEXT("Color"));

	// Local layout (cm). +Y = screen-left (outer side, away from the character).
	constexpr float BarWidth = 1.2f;
	constexpr float MajorTickLength = 10.f;
	constexpr float MinorTickLength = 5.f;
	constexpr float MajorTickThickness = 0.8f;
	constexpr float MinorTickThickness = 0.35f;
	constexpr float TickDepth = 1.f;
	constexpr float LabelGap = 2.f;
	constexpr float ScaleLabelSize = 5.f;
	constexpr float HeightLabelSize = 6.f;
	// Height marker: from 20 cm toward the character to the major tick tip.
	constexpr float HeightMarkerInner = 20.f;
	constexpr float HeightMarkerThickness = 0.8f;
	// A scale label closer than this to the height label is hidden (overlap).
	constexpr float LabelOverlapCm = 7.f;
}

AViewerHeightRuler::AViewerHeightRuler()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("RulerRoot"));
	SetRootComponent(Root);

	// Loaded here (CDO), so the cooker packages them like the character
	// actor's highlight materials. M_ViewerRuler comes from
	// Scripts/CreatePortfolioAssets.py; if it is missing the opaque magenta
	// part highlight material is used instead (still unlit, still visible).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RulerMaterialFinder(TEXT("/Game/Portfolio/Materials/M_ViewerRuler.M_ViewerRuler"));
	if (RulerMaterialFinder.Succeeded())
	{
		RulerMaterial = RulerMaterialFinder.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> FallbackMaterialFinder(TEXT("/Game/Portfolio/Materials/M_ViewerPartHighlight.M_ViewerPartHighlight"));
		if (FallbackMaterialFinder.Succeeded())
		{
			RulerMaterial = FallbackMaterialFinder.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}
}

float AViewerHeightRuler::ComputeRulerTopCm(float HeightCm)
{
	if (!(HeightCm > DefaultRulerTopCm))
	{
		return DefaultRulerTopCm;
	}
	return FMath::Min(MaxRulerTopCm, FMath::CeilToFloat(HeightCm / LabelSpacingCm) * LabelSpacingCm);
}

FString AViewerHeightRuler::FormatHeightLabel(float HeightCm)
{
	return FString::Printf(TEXT("Height %d cm"), FMath::RoundToInt(HeightCm));
}

FString AViewerHeightRuler::GetHeightLabelText() const
{
	return HeightLabel ? HeightLabel->Text.ToString() : FString();
}

int32 AViewerHeightRuler::GetVisibleScaleLabelCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UTextRenderComponent>& Label : ScaleLabels)
	{
		if (Label && Label->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

void AViewerHeightRuler::SetMeasuredHeight(float HeightCm)
{
	const float Rounded = HeightCm > 0.f ? FMath::RoundToFloat(HeightCm * 10.f) / 10.f : -1.f;
	if (bBuilt && FMath::IsNearlyEqual(Rounded, MeasuredHeightCm))
	{
		return;
	}
	MeasuredHeightCm = Rounded;
	RulerTopCm = ComputeRulerTopCm(MeasuredHeightCm);
	Rebuild();
}

void AViewerHeightRuler::SetRulerTransform(const FVector& FeetLocation, float FacingYaw)
{
	SetActorLocationAndRotation(FeetLocation, FRotator(0.f, FacingYaw, 0.f));
}

void AViewerHeightRuler::SetRulerVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

void AViewerHeightRuler::DestroyParts()
{
	auto DestroyComponent = [](UPrimitiveComponent* Component)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	};
	DestroyComponent(Bar.Get());
	for (const TObjectPtr<UStaticMeshComponent>& Tick : Ticks)
	{
		DestroyComponent(Tick.Get());
	}
	for (const TObjectPtr<UTextRenderComponent>& Label : ScaleLabels)
	{
		DestroyComponent(Label.Get());
	}
	DestroyComponent(HeightMarker.Get());
	DestroyComponent(HeightLabel.Get());
	Bar = nullptr;
	Ticks.Reset();
	ScaleLabels.Reset();
	HeightMarker = nullptr;
	HeightLabel = nullptr;
}

UStaticMeshComponent* AViewerHeightRuler::AddBox(const FName& Name, const FVector& Center, const FVector& SizeCm, UMaterialInterface* Material)
{
	if (!CubeMesh || !Root)
	{
		return nullptr;
	}
	UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Name));
	Box->SetStaticMesh(CubeMesh);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCastShadow(false);
	Box->bReceivesDecals = false;
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetupAttachment(Root);
	// /Engine/BasicShapes/Cube is a 100 cm cube centred on its pivot.
	Box->SetRelativeLocation(Center);
	Box->SetRelativeScale3D(SizeCm / 100.f);
	if (Material)
	{
		Box->SetMaterial(0, Material);
	}
	Box->RegisterComponent();
	return Box;
}

UTextRenderComponent* AViewerHeightRuler::AddLabel(const FName& Name, const FVector& Location, const FString& Text, float WorldSize, FColor Color)
{
	if (!Root)
	{
		return nullptr;
	}
	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, MakeUniqueObjectName(this, UTextRenderComponent::StaticClass(), Name));
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetGenerateOverlapEvents(false);
	Label->SetCastShadow(false);
	Label->SetMobility(EComponentMobility::Movable);
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(Location);
	// The text faces the component's +X (= the camera, see SetRulerTransform);
	// right-aligned so it ends next to the tick and grows outwards (+Y = screen-left).
	Label->SetHorizontalAlignment(EHTA_Right);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(WorldSize);
	Label->SetTextRenderColor(Color);
	Label->SetText(FText::FromString(Text));
	Label->RegisterComponent();
	return Label;
}

void AViewerHeightRuler::Rebuild()
{
	using namespace ViewerHeightRulerPrivate;

	DestroyParts();
	bBuilt = true;

	if (RulerMaterial && !RulerMID)
	{
		RulerMID = UMaterialInstanceDynamic::Create(RulerMaterial, this);
		HeightMID = UMaterialInstanceDynamic::Create(RulerMaterial, this);
		if (RulerMID)
		{
			RulerMID->SetVectorParameterValue(ColorParameterName, RulerColor);
		}
		if (HeightMID)
		{
			HeightMID->SetVectorParameterValue(ColorParameterName, HeightColor);
		}
	}
	UMaterialInterface* BarMaterial = RulerMID ? RulerMID.Get() : RulerMaterial.Get();
	UMaterialInterface* MarkerMaterial = HeightMID ? HeightMID.Get() : RulerMaterial.Get();

	const FColor ScaleTextColor = RulerColor.ToFColor(true);
	const FColor HeightTextColor = HeightColor.ToFColor(true);
	const bool bHasHeight = MeasuredHeightCm > 0.f;

	Bar = AddBox(TEXT("RulerBar"), FVector(0.f, 0.f, RulerTopCm * 0.5f), FVector(BarWidth, BarWidth, RulerTopCm), BarMaterial);

	const int32 NumTicks = FMath::RoundToInt(RulerTopCm / TickSpacingCm) + 1;
	for (int32 Index = 0; Index < NumTicks; ++Index)
	{
		const float Z = Index * TickSpacingCm;
		const bool bMajor = Index % FMath::RoundToInt(LabelSpacingCm / TickSpacingCm) == 0;
		const float Length = bMajor ? MajorTickLength : MinorTickLength;
		const float Thickness = bMajor ? MajorTickThickness : MinorTickThickness;
		if (UStaticMeshComponent* Tick = AddBox(TEXT("RulerTick"), FVector(0.f, Length * 0.5f, Z), FVector(TickDepth, Length, Thickness), BarMaterial))
		{
			Ticks.Add(Tick);
		}

		if (bMajor && Index > 0)
		{
			if (UTextRenderComponent* Label = AddLabel(TEXT("RulerLabel"), FVector(0.f, MajorTickLength + LabelGap, Z),
				FString::Printf(TEXT("%d cm"), FMath::RoundToInt(Z)), ScaleLabelSize, ScaleTextColor))
			{
				// The height label wins where the two would overlap.
				Label->SetVisibility(!(bHasHeight && FMath::Abs(Z - MeasuredHeightCm) < LabelOverlapCm));
				ScaleLabels.Add(Label);
			}
		}
	}

	if (bHasHeight)
	{
		const float MarkerLength = HeightMarkerInner + MajorTickLength;
		HeightMarker = AddBox(TEXT("RulerHeightMarker"), FVector(0.f, (MajorTickLength - HeightMarkerInner) * 0.5f, MeasuredHeightCm),
			FVector(TickDepth * 1.2f, MarkerLength, HeightMarkerThickness), MarkerMaterial);
		HeightLabel = AddLabel(TEXT("RulerHeightLabel"), FVector(0.f, MajorTickLength + LabelGap, MeasuredHeightCm),
			FormatHeightLabel(MeasuredHeightCm), HeightLabelSize, HeightTextColor);
	}
}
