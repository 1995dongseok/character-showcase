#include "UI/CharacterViewerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Character/CharacterProfileData.h"
#include "Character/PortfolioCharacterActor.h"
#include "CharacterViewer/CharacterViewerCameraPawn.h"
#include "CharacterViewer/CharacterViewerController.h"
#include "CharacterViewer/CharacterViewerGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UCharacterViewerButtonBinding::HandleClicked()
{
	UCharacterViewerWidget* OwnerWidget = Widget.Get();
	if (!OwnerWidget)
	{
		return;
	}

	switch (Kind)
	{
	case ECharacterViewerButtonKind::CameraPreset:
		OwnerWidget->RequestCameraPreset(Id);
		break;
	case ECharacterViewerButtonKind::Animation:
		OwnerWidget->RequestAnimation(Id);
		break;
	case ECharacterViewerButtonKind::Expression:
		OwnerWidget->RequestExpression(Id);
		break;
	case ECharacterViewerButtonKind::MaterialVariant:
		OwnerWidget->RequestMaterialVariant(Id);
		break;
	case ECharacterViewerButtonKind::ToggleTurntable:
		OwnerWidget->RequestToggleTurntable();
		break;
	case ECharacterViewerButtonKind::ResetCamera:
		OwnerWidget->RequestResetCamera();
		break;
	case ECharacterViewerButtonKind::ToggleCleanView:
		OwnerWidget->RequestToggleCleanView();
		break;
	case ECharacterViewerButtonKind::CharacterProfile:
		OwnerWidget->RequestCharacterProfile(Id);
		break;
	}
}

void UCharacterViewerWidget::BindToViewer(ACharacterViewerController* InController, APortfolioCharacterActor* InActor, ACharacterViewerCameraPawn* InCameraPawn)
{
	WeakController = InController;
	WeakActor = InActor;
	WeakCameraPawn = InCameraPawn;
	CurrentCameraPresetId = NAME_None;

	OnViewerDataChanged();
	RefreshFallbackUI();
}

FText UCharacterViewerWidget::GetDisplayName() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return (Actor && Actor->Profile) ? Actor->Profile->DisplayName : FText::GetEmpty();
}

FText UCharacterViewerWidget::GetDescription() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return (Actor && Actor->Profile) ? Actor->Profile->Description : FText::GetEmpty();
}

TArray<FViewerListItem> UCharacterViewerWidget::GetCameraPresets() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerCameraPreset& Preset : Actor->Profile->CameraPresets)
	{
		FViewerListItem Item;
		Item.Id = Preset.Id;
		Item.DisplayName = Preset.DisplayName;
		Item.bEnabled = Preset.Id != NAME_None;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetAnimations() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerAnimationEntry& Entry : Actor->Profile->Animations)
	{
		FViewerListItem Item;
		Item.Id = Entry.Id;
		Item.DisplayName = Entry.DisplayName;
		Item.bEnabled = Entry.Id != NAME_None && Entry.Sequence != nullptr;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetExpressions() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerExpression& Expression : Actor->Profile->Expressions)
	{
		FViewerListItem Item;
		Item.Id = Expression.Id;
		Item.DisplayName = Expression.DisplayName;
		// An empty Morphs array is a valid Neutral expression, so only an empty Id is invalid here.
		Item.bEnabled = Expression.Id != NAME_None;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetMaterialVariants() const
{
	TArray<FViewerListItem> Items;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile)
	{
		return Items;
	}

	for (const FViewerMaterialVariant& Variant : Actor->Profile->MaterialVariants)
	{
		FViewerListItem Item;
		Item.Id = Variant.Id;
		Item.DisplayName = Variant.DisplayName;

		bool bHasMaterial = Variant.Slots.Num() == 0; // "Default" (no overrides) is a valid, always-enabled variant.
		for (const FViewerMaterialSlotOverride& SlotOverride : Variant.Slots)
		{
			if (SlotOverride.Material)
			{
				bHasMaterial = true;
				break;
			}
		}

		Item.bEnabled = Variant.Id != NAME_None && bHasMaterial;
		Items.Add(Item);
	}

	return Items;
}

TArray<FViewerListItem> UCharacterViewerWidget::GetCharacterLibrary() const
{
	TArray<FViewerListItem> Items;

	const ACharacterViewerController* Controller = WeakController.Get();
	const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	const ACharacterViewerGameMode* GameMode = World ? World->GetAuthGameMode<ACharacterViewerGameMode>() : nullptr;
	if (!GameMode)
	{
		return Items;
	}

	for (UCharacterProfileData* LibraryProfile : GameMode->GetProfileLibrary())
	{
		if (!LibraryProfile)
		{
			continue;
		}

		FViewerListItem Item;
		Item.Id = LibraryProfile->GetFName();
		Item.DisplayName = LibraryProfile->DisplayName;
		Item.bEnabled = true;
		Items.Add(Item);
	}

	return Items;
}

bool UCharacterViewerWidget::IsTurntableEnabled() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor && Actor->IsTurntableEnabled();
}

FName UCharacterViewerWidget::GetCurrentAnimationId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentAnimationId() : NAME_None;
}

FName UCharacterViewerWidget::GetCurrentExpressionId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentExpressionId() : NAME_None;
}

FName UCharacterViewerWidget::GetCurrentMaterialVariantId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor ? Actor->GetCurrentVariantId() : NAME_None;
}

bool UCharacterViewerWidget::IsPointerOverPanel() const
{
	// The fallback panel's own hover state (not this outer UserWidget's,
	// which would cover the whole, mostly-empty, screen-filling canvas) is
	// what must block Orbit/Zoom; the empty canvas area must not.
	if (FallbackPanelBorder && FallbackPanelBorder->IsHovered())
	{
		return true;
	}

	return IsHovered();
}

void UCharacterViewerWidget::RequestCameraPreset(FName Id)
{
	CurrentCameraPresetId = Id;
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectCameraPreset(Id);
	}
}

void UCharacterViewerWidget::RequestAnimation(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectAnimation(Id);
	}
}

void UCharacterViewerWidget::RequestExpression(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectExpression(Id);
	}
}

void UCharacterViewerWidget::RequestMaterialVariant(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectMaterialVariant(Id);
	}
}

void UCharacterViewerWidget::RequestCharacterProfile(FName ProfileAssetName)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectCharacterProfile(ProfileAssetName);
	}
}

FText UCharacterViewerWidget::GetFallbackDisplayNameText() const
{
	return FallbackDisplayNameText ? FallbackDisplayNameText->GetText() : FText::GetEmpty();
}

void UCharacterViewerWidget::RequestToggleTurntable()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleTurntable();
	}
	RefreshFallbackUI();
}

void UCharacterViewerWidget::RequestToggleCleanView()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleCleanView();
	}
}

void UCharacterViewerWidget::RequestResetCamera()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ResetCamera();
	}
}

TSharedRef<SWidget> UCharacterViewerWidget::RebuildWidget()
{
	// A designer-authored WBP tree (RootWidget != nullptr) is left completely
	// alone; the fallback only fills in for a WBP (or this C++ class used
	// directly) with an empty tree. See section 13.10. It must be built here,
	// before Super::RebuildWidget() turns RootWidget into the Slate widget:
	// building it in NativeConstruct() (called after that) left only the
	// empty-tree SSpacer on screen, so the panel was never visible in -game.
	if (!IsDesignTime() && WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		BuildFallbackUI();
		bFallbackUIBuilt = true;
	}

	return Super::RebuildWidget();
}

void UCharacterViewerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bFallbackUIBuilt)
	{
		RefreshFallbackUI();
	}
}

void UCharacterViewerWidget::NativeDestruct()
{
	WeakController.Reset();
	WeakActor.Reset();
	WeakCameraPawn.Reset();

	Super::NativeDestruct();
}

// --- Fallback UI (section 13.10) --------------------------------------------

void UCharacterViewerWidget::BuildFallbackUI()
{
	if (!WidgetTree || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FallbackRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	if (!RootCanvas)
	{
		return;
	}

	// Right-edge, full-height, fixed-width (320px) dark panel. Anchors (1,0)-(1,1)
	// stretch vertically (Offsets.Top/Bottom = margins) and are point-anchored
	// horizontally to the right edge (Offsets.Left = X position from the anchor,
	// Offsets.Right = width); Alignment.X = 1 keeps the panel's right edge flush
	// with the screen's right edge instead of overflowing past it.
	FallbackPanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FallbackPanelBorder"));
	FallbackPanelBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
	FallbackPanelBorder->SetPadding(FMargin(16.f));

	if (UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(FallbackPanelBorder))
	{
		BorderSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		BorderSlot->SetAlignment(FVector2D(1.f, 0.f));
		BorderSlot->SetOffsets(FMargin(0.f, 0.f, 320.f, 0.f));
	}

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FallbackScrollBox"));
	FallbackPanelBorder->SetContent(ScrollBox);
	if (!ScrollBox)
	{
		return;
	}

	FallbackDisplayNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FallbackDisplayNameText"));
	{
		FSlateFontInfo NameFont = FallbackDisplayNameText->GetFont();
		NameFont.Size = 22;
		FallbackDisplayNameText->SetFont(NameFont);
	}
	FallbackDisplayNameText->SetAutoWrapText(true);
	ScrollBox->AddChild(FallbackDisplayNameText);

	FallbackDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FallbackDescriptionText"));
	FallbackDescriptionText->SetAutoWrapText(true);
	ScrollBox->AddChild(FallbackDescriptionText);

	auto MakeSection = [this, ScrollBox](const TCHAR* BoxName, const TCHAR* HeaderName, UVerticalBox*& OutBox, UTextBlock*& OutHeader)
	{
		OutBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), BoxName);
		OutHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), HeaderName);
		OutHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
		OutBox->AddChildToVerticalBox(OutHeader);
		ScrollBox->AddChild(OutBox);
	};

	// CHARACTER: top section (P1 completion evidence, section 6/13.6/13.10), one
	// button per ACharacterViewerGameMode::ProfileLibrary entry.
	UVerticalBox* CharacterBox = nullptr;
	UTextBlock* CharacterHeader = nullptr;
	MakeSection(TEXT("FallbackCharacterSectionBox"), TEXT("FallbackCharacterSectionHeader"), CharacterBox, CharacterHeader);
	FallbackCharacterSectionBox = CharacterBox;
	FallbackCharacterSectionHeader = CharacterHeader;

	UVerticalBox* ViewBox = nullptr;
	UTextBlock* ViewHeader = nullptr;
	MakeSection(TEXT("FallbackViewSectionBox"), TEXT("FallbackViewSectionHeader"), ViewBox, ViewHeader);
	FallbackViewSectionBox = ViewBox;
	FallbackViewSectionHeader = ViewHeader;

	UVerticalBox* AnimBox = nullptr;
	UTextBlock* AnimHeader = nullptr;
	MakeSection(TEXT("FallbackAnimationSectionBox"), TEXT("FallbackAnimationSectionHeader"), AnimBox, AnimHeader);
	FallbackAnimationSectionBox = AnimBox;
	FallbackAnimationSectionHeader = AnimHeader;

	UVerticalBox* ExprBox = nullptr;
	UTextBlock* ExprHeader = nullptr;
	MakeSection(TEXT("FallbackExpressionSectionBox"), TEXT("FallbackExpressionSectionHeader"), ExprBox, ExprHeader);
	FallbackExpressionSectionBox = ExprBox;
	FallbackExpressionSectionHeader = ExprHeader;

	UVerticalBox* AppearanceBox = nullptr;
	UTextBlock* AppearanceHeader = nullptr;
	MakeSection(TEXT("FallbackAppearanceSectionBox"), TEXT("FallbackAppearanceSectionHeader"), AppearanceBox, AppearanceHeader);
	FallbackAppearanceSectionBox = AppearanceBox;
	FallbackAppearanceSectionHeader = AppearanceHeader;

	// DISPLAY: fixed (non-data-driven) rows, built once here; never hidden.
	UVerticalBox* DisplayBox = nullptr;
	UTextBlock* DisplayHeader = nullptr;
	MakeSection(TEXT("FallbackDisplaySectionBox"), TEXT("FallbackDisplaySectionHeader"), DisplayBox, DisplayHeader);
	FallbackDisplaySectionBox = DisplayBox;
	if (DisplayHeader)
	{
		DisplayHeader->SetText(FText::FromString(TEXT("DISPLAY")));
	}

	UTextBlock* TurntableLabel = nullptr;
	AddFallbackButtonRow(FallbackDisplaySectionBox, FText::FromString(TEXT("Turntable (Space)")), true, NAME_None, ECharacterViewerButtonKind::ToggleTurntable, &TurntableLabel);
	FallbackTurntableButtonText = TurntableLabel;

	AddFallbackButtonRow(FallbackDisplaySectionBox, FText::FromString(TEXT("Reset Camera (R)")), true, NAME_None, ECharacterViewerButtonKind::ResetCamera);
	AddFallbackButtonRow(FallbackDisplaySectionBox, FText::FromString(TEXT("Clean View (H)")), true, NAME_None, ECharacterViewerButtonKind::ToggleCleanView);
}

void UCharacterViewerWidget::RefreshFallbackUI()
{
	if (!bFallbackUIBuilt)
	{
		return;
	}

	if (FallbackDisplayNameText)
	{
		FallbackDisplayNameText->SetText(GetDisplayName());
	}
	if (FallbackDescriptionText)
	{
		FallbackDescriptionText->SetText(GetDescription());
	}

	PopulateFallbackSection(FallbackCharacterSectionBox, FallbackCharacterSectionHeader, FText::FromString(TEXT("CHARACTER")), GetCharacterLibrary(), ECharacterViewerButtonKind::CharacterProfile);
	PopulateFallbackSection(FallbackViewSectionBox, FallbackViewSectionHeader, FText::FromString(TEXT("VIEW")), GetCameraPresets(), ECharacterViewerButtonKind::CameraPreset);
	PopulateFallbackSection(FallbackAnimationSectionBox, FallbackAnimationSectionHeader, FText::FromString(TEXT("ANIMATION")), GetAnimations(), ECharacterViewerButtonKind::Animation);
	PopulateFallbackSection(FallbackExpressionSectionBox, FallbackExpressionSectionHeader, FText::FromString(TEXT("EXPRESSION")), GetExpressions(), ECharacterViewerButtonKind::Expression);
	PopulateFallbackSection(FallbackAppearanceSectionBox, FallbackAppearanceSectionHeader, FText::FromString(TEXT("APPEARANCE")), GetMaterialVariants(), ECharacterViewerButtonKind::MaterialVariant);

	if (FallbackTurntableButtonText)
	{
		FallbackTurntableButtonText->SetText(FText::FromString(IsTurntableEnabled() ? TEXT("Turntable: On (Space)") : TEXT("Turntable: Off (Space)")));
	}
}

void UCharacterViewerWidget::PopulateFallbackSection(UVerticalBox* SectionBox, UTextBlock* HeaderText, const FText& HeaderLabel, const TArray<FViewerListItem>& Items, ECharacterViewerButtonKind Kind)
{
	if (!SectionBox)
	{
		return;
	}

	// ClearChildren() detaches (does not destroy) the header widget too;
	// re-add it below. Also drop this section's old button bindings so they
	// do not accumulate across every RefreshFallbackUI() call.
	SectionBox->ClearChildren();
	FallbackButtonBindings.RemoveAll([Kind](const TObjectPtr<UCharacterViewerButtonBinding>& Binding)
	{
		return !Binding || Binding->Kind == Kind;
	});

	if (HeaderText)
	{
		HeaderText->SetText(HeaderLabel);
		SectionBox->AddChildToVerticalBox(HeaderText);
	}

	for (const FViewerListItem& Item : Items)
	{
		AddFallbackButtonRow(SectionBox, Item.DisplayName, Item.bEnabled, Item.Id, Kind);
	}

	// Empty data-driven sections (including their header) are hidden entirely.
	SectionBox->SetVisibility(Items.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

UButton* UCharacterViewerWidget::AddFallbackButtonRow(UVerticalBox* Container, const FText& Label, bool bEnabled, FName Id, ECharacterViewerButtonKind Kind, UTextBlock** OutTextBlock)
{
	if (!Container || !WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(Label);
	Button->AddChild(ButtonText);
	Button->SetIsEnabled(bEnabled);

	UCharacterViewerButtonBinding* Binding = NewObject<UCharacterViewerButtonBinding>(this);
	Binding->Widget = this;
	Binding->Id = Id;
	Binding->Kind = Kind;
	FallbackButtonBindings.Add(Binding);

	Button->OnClicked.AddDynamic(Binding, &UCharacterViewerButtonBinding::HandleClicked);

	Container->AddChildToVerticalBox(Button);

	if (OutTextBlock)
	{
		*OutTextBlock = ButtonText;
	}
	return Button;
}
