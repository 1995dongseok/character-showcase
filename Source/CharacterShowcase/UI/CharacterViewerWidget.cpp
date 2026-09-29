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
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateTypes.h"

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
	case ECharacterViewerButtonKind::ToggleInspection:
		OwnerWidget->RequestToggleInspection();
		break;
	case ECharacterViewerButtonKind::ToggleWireframe:
		OwnerWidget->RequestToggleWireframe();
		break;
	}
}

bool UCharacterViewerWidget::IsUsingDesignerLayout() const
{
	return PanelRoot != nullptr && !bFallbackUIBuilt;
}

void UCharacterViewerWidget::BindToViewer(ACharacterViewerController* InController, APortfolioCharacterActor* InActor, ACharacterViewerCameraPawn* InCameraPawn)
{
	WeakController = InController;
	WeakActor = InActor;
	WeakCameraPawn = InCameraPawn;

	// Seed the VIEW section's current selection from the profile's own
	// DefaultPresetId (applied by ApplyFramingForCurrentActor()/GetResetFraming()
	// at bind time), not NAME_None -- otherwise the actually-applied default
	// preset (e.g. "Full") shows no "▶" until the user explicitly clicks a
	// preset button (section 13.10 "state display"). Runs on the initial bind
	// and again on every profile switch (SwitchProfile() calls BindToViewer()).
	const APortfolioCharacterActor* BoundActor = InActor;
	CurrentCameraPresetId = (BoundActor && BoundActor->Profile) ? BoundActor->Profile->DefaultPresetId : NAME_None;

	OnViewerDataChanged();
	RefreshUI();
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

FName UCharacterViewerWidget::GetCurrentCharacterProfileId() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return (Actor && Actor->Profile) ? Actor->Profile->GetFName() : NAME_None;
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

bool UCharacterViewerWidget::IsInspectionEnabled() const
{
	const ACharacterViewerController* Controller = WeakController.Get();
	return Controller && Controller->IsInspectionEnabled();
}

bool UCharacterViewerWidget::IsWireframeEnabled() const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	return Actor && Actor->IsWireframeEnabled();
}

bool UCharacterViewerWidget::GetSelectedPartInfo(FViewerPartInfo& OutInfo) const
{
	const APortfolioCharacterActor* Actor = WeakActor.Get();
	if (!Actor || !Actor->Profile || Actor->GetSelectedPartId() == NAME_None)
	{
		return false;
	}

	const FViewerPartInfo* Part = Actor->Profile->FindPart(Actor->GetSelectedPartId());
	if (!Part)
	{
		return false;
	}

	OutInfo = *Part;
	return true;
}

void UCharacterViewerWidget::NotifySelectionChanged()
{
	RefreshUI();
}

bool UCharacterViewerWidget::IsPointerOverPanel() const
{
	// PanelRoot is the same UBorder in both layout paths (designer-bound via
	// BindWidgetOptional, or built by BuildFallbackUI()); its own hover state
	// (not this outer UserWidget's, which would cover the whole, mostly-empty,
	// screen-filling canvas) is what must block Orbit/Zoom/Inspection.
	if (PanelRoot && PanelRoot->IsHovered())
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
	RefreshUI();
}

void UCharacterViewerWidget::RequestAnimation(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectAnimation(Id);
	}
	RefreshUI();
}

void UCharacterViewerWidget::RequestExpression(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectExpression(Id);
	}
	RefreshUI();
}

void UCharacterViewerWidget::RequestMaterialVariant(FName Id)
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->SelectMaterialVariant(Id);
	}
	RefreshUI();
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
	return NameText ? NameText->GetText() : FText::GetEmpty();
}

ESlateVisibility UCharacterViewerWidget::GetFallbackInspectionSectionVisibility() const
{
	return InspectionSectionBox ? InspectionSectionBox->GetVisibility() : ESlateVisibility::Collapsed;
}

FText UCharacterViewerWidget::GetFallbackInspectionBodyText() const
{
	return InspectionBodyText ? InspectionBodyText->GetText() : FText::GetEmpty();
}

FText UCharacterViewerWidget::GetGeneratedButtonText(ECharacterViewerButtonKind Kind, FName Id) const
{
	for (const TObjectPtr<UCharacterViewerButtonBinding>& Binding : ButtonBindings)
	{
		if (Binding && Binding->Kind == Kind && Binding->Id == Id)
		{
			if (UButton* Button = Binding->ButtonWidget.Get())
			{
				if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
				{
					return Label->GetText();
				}
			}
		}
	}
	return FText::GetEmpty();
}

void UCharacterViewerWidget::RequestToggleTurntable()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleTurntable();
	}
	RefreshUI();
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

void UCharacterViewerWidget::RequestToggleInspection()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleInspection();
	}
	RefreshUI();
}

void UCharacterViewerWidget::RequestToggleWireframe()
{
	if (ACharacterViewerController* Controller = WeakController.Get())
	{
		Controller->ToggleWireframe();
	}
	RefreshUI();
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

	// PanelRoot is populated by now on both paths: BuildFallbackUI() (called
	// from RebuildWidget(), before Super::RebuildWidget() converts the tree to
	// Slate) for the fallback, or UUserWidget's own BindWidgetOptional
	// resolution (which also runs before NativeConstruct) for a designer WBP.
	if (PanelRoot)
	{
		PanelRoot->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UCharacterViewerWidget, HandleFallbackPanelMouseButtonDown));
	}

	// A designer WBP tree that exists (so BuildFallbackUI() never ran) but is
	// missing, mis-typed, or not-a-variable on one of the 7 BindWidgetOptional
	// names leaves this UI silently empty by design (RefreshUI() below is a
	// no-op past whichever containers did not resolve) -- but "silently" is
	// exactly the problem an artist/CI run needs to know about, so name
	// what did not resolve, once, instead of leaving no trace at all.
	if (!bFallbackUIBuilt && WidgetTree && WidgetTree->RootWidget != nullptr)
	{
		TArray<FString> MissingOrWrongType;
		if (!PanelRoot) { MissingOrWrongType.Add(TEXT("PanelRoot")); }
		if (!NameText) { MissingOrWrongType.Add(TEXT("NameText")); }
		if (!ControlsBox) { MissingOrWrongType.Add(TEXT("ControlsBox")); }
		if (!DescriptionScroll) { MissingOrWrongType.Add(TEXT("DescriptionScroll")); }
		if (!DescriptionText) { MissingOrWrongType.Add(TEXT("DescriptionText")); }
		if (!ListsScroll) { MissingOrWrongType.Add(TEXT("ListsScroll")); }
		if (!ListsBox) { MissingOrWrongType.Add(TEXT("ListsBox")); }

		if (MissingOrWrongType.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CharacterViewerWidget] '%s' has a designer tree but %d of the 7 required BindWidgetOptional widgets did not resolve (missing, wrong type, or not marked 'Is Variable'): %s. The panel will stay empty for these until the WBP is fixed (see Docs/CHARACTER_VIEWER_SETUP.md section 13.10)."),
				*GetName(), MissingOrWrongType.Num(), *FString::Join(MissingOrWrongType, TEXT(", ")));
		}
	}

	RefreshUI();
}

void UCharacterViewerWidget::NativeDestruct()
{
	WeakController.Reset();
	WeakActor.Reset();
	WeakCameraPawn.Reset();

	Super::NativeDestruct();
}

// --- Layout (section 13.10) --------------------------------------------

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
	PanelRoot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelRoot"));
	if (!PanelRoot)
	{
		return;
	}
	PanelRoot->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
	PanelRoot->SetPadding(FMargin(16.f));

	if (UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(PanelRoot))
	{
		BorderSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		BorderSlot->SetAlignment(FVector2D(1.f, 0.f));
		BorderSlot->SetOffsets(FMargin(0.f, 0.f, 320.f, 0.f));
	}

	// Structural root inside the panel (not itself designer-bindable): holds
	// the fixed priority order name -> CHARACTER/VIEW/DISPLAY -> description
	// (limited-height scroll) -> lists (scroll), top to bottom (section 13.10).
	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
	if (!PanelContent)
	{
		return;
	}
	PanelRoot->SetContent(PanelContent);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	if (NameText)
	{
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 22;
		NameText->SetFont(NameFont);
		NameText->SetAutoWrapText(true);
		PanelContent->AddChildToVerticalBox(NameText);
	}

	// CHARACTER/VIEW/DISPLAY: primary controls, always visible, never scrolled away.
	ControlsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ControlsBox"));
	if (ControlsBox)
	{
		PanelContent->AddChildToVerticalBox(ControlsBox);
	}

	// Description: the one long-text field, confined to a small fixed-height
	// scroll area instead of pushing the CHARACTER/VIEW/DISPLAY controls or
	// the ANIMATION/EXPRESSION/APPEARANCE lists off screen.
	DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DescriptionScroll"));
	if (DescriptionScroll)
	{
		if (USizeBox* DescriptionSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DescriptionSizeBox")))
		{
			DescriptionSize->SetMaxDesiredHeight(110.f);
			DescriptionSize->SetContent(DescriptionScroll);
			PanelContent->AddChildToVerticalBox(DescriptionSize);
		}
		else
		{
			PanelContent->AddChildToVerticalBox(DescriptionScroll);
		}

		DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescriptionText"));
		if (DescriptionText)
		{
			DescriptionText->SetAutoWrapText(true);
			DescriptionScroll->AddChild(DescriptionText);
		}
	}

	// ANIMATION/EXPRESSION/APPEARANCE/INSPECTION: the long, data-driven lists,
	// in their own scroll area separate from the description.
	ListsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ListsScroll"));
	if (ListsScroll)
	{
		// Without an explicit Fill slot size, a VerticalBox only gives a child
		// its own desired (content) height, so ListsScroll never had a bounded
		// height to scroll within and just overflowed the screen instead of
		// scrolling. Fill makes it take the remaining space in PanelContent.
		if (UVerticalBoxSlot* ListsScrollSlot = Cast<UVerticalBoxSlot>(PanelContent->AddChildToVerticalBox(ListsScroll)))
		{
			ListsScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		ListsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListsBox"));
		if (ListsBox)
		{
			ListsScroll->AddChild(ListsBox);
		}
	}
}

FEventReply UCharacterViewerWidget::HandleFallbackPanelMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	// Absorb the press so it never reaches the game viewport / Enhanced Input
	// (see the header comment): the panel must not drive Orbit/Inspection.
	return FEventReply(true);
}

void UCharacterViewerWidget::RefreshUI()
{
	// Path-independent: PanelRoot/NameText/ControlsBox/... are the same
	// members whether they came from a designer WBP (BindWidgetOptional) or
	// BuildFallbackUI(). No-op until one of those has actually run.
	if (!PanelRoot)
	{
		return;
	}

	if (NameText)
	{
		NameText->SetText(GetDisplayName());
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(GetDescription());
	}

	// Input boundary (section 4/13.11.5): wheel over either scroll area must
	// always scroll the panel, even when content currently fits, so it never
	// leaks through to the Controller's Zoom handler.
	if (DescriptionScroll)
	{
		DescriptionScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	}
	if (ListsScroll)
	{
		ListsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	}

	// Every section below is rebuilt from scratch this call; drop the
	// previous call's button bindings up front instead of per-section.
	ButtonBindings.Reset();
	TurntableButtonText = nullptr;
	InspectionButtonText = nullptr;
	WireframeButton = nullptr;
	InspectionSectionBox = nullptr;
	InspectionBodyText = nullptr;

	if (ControlsBox)
	{
		ControlsBox->ClearChildren();
		AddListSection(ControlsBox, FText::FromString(TEXT("CHARACTER")), GetCharacterLibrary(), ECharacterViewerButtonKind::CharacterProfile, GetCurrentCharacterProfileId());
		AddListSection(ControlsBox, FText::FromString(TEXT("VIEW")), GetCameraPresets(), ECharacterViewerButtonKind::CameraPreset, CurrentCameraPresetId);
		BuildDisplaySection(ControlsBox);
	}

	if (ListsBox)
	{
		ListsBox->ClearChildren();
		AddListSection(ListsBox, FText::FromString(TEXT("ANIMATION")), GetAnimations(), ECharacterViewerButtonKind::Animation, GetCurrentAnimationId());
		AddListSection(ListsBox, FText::FromString(TEXT("EXPRESSION")), GetExpressions(), ECharacterViewerButtonKind::Expression, GetCurrentExpressionId());
		AddListSection(ListsBox, FText::FromString(TEXT("APPEARANCE")), GetMaterialVariants(), ECharacterViewerButtonKind::MaterialVariant, GetCurrentMaterialVariantId());
		BuildInspectionSection(ListsBox);
	}
}

void UCharacterViewerWidget::AddListSection(UVerticalBox* Container, const FText& HeaderLabel, const TArray<FViewerListItem>& Items, ECharacterViewerButtonKind Kind, FName CurrentSelectedId)
{
	// Empty data-driven sections (including their header) are omitted entirely.
	if (!Container || !WidgetTree || Items.Num() == 0)
	{
		return;
	}

	UVerticalBox* SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!SectionBox || !Header)
	{
		return;
	}
	Header->SetText(HeaderLabel);
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
	SectionBox->AddChildToVerticalBox(Header);

	for (const FViewerListItem& Item : Items)
	{
		const bool bSelected = Item.Id != NAME_None && Item.Id == CurrentSelectedId;
		AddButtonRow(SectionBox, Item.DisplayName, Item.bEnabled, bSelected, Item.Id, Kind);
	}

	Container->AddChildToVerticalBox(SectionBox);
}

void UCharacterViewerWidget::BuildDisplaySection(UVerticalBox* Container)
{
	if (!Container || !WidgetTree)
	{
		return;
	}

	UVerticalBox* SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!SectionBox || !Header)
	{
		return;
	}
	Header->SetText(FText::FromString(TEXT("DISPLAY")));
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
	SectionBox->AddChildToVerticalBox(Header);

	const bool bTurntableOn = IsTurntableEnabled();
	UTextBlock* TurntableLabel = nullptr;
	AddButtonRow(SectionBox, FText::FromString(bTurntableOn ? TEXT("Turntable: On (Space)") : TEXT("Turntable: Off (Space)")), true, bTurntableOn, NAME_None, ECharacterViewerButtonKind::ToggleTurntable, &TurntableLabel);
	TurntableButtonText = TurntableLabel;

	AddButtonRow(SectionBox, FText::FromString(TEXT("Reset Camera (R)")), true, false, NAME_None, ECharacterViewerButtonKind::ResetCamera);
	AddButtonRow(SectionBox, FText::FromString(TEXT("Clean View (H)")), true, false, NAME_None, ECharacterViewerButtonKind::ToggleCleanView);

	const bool bInspectionOn = IsInspectionEnabled();
	UTextBlock* InspectionLabel = nullptr;
	AddButtonRow(SectionBox, FText::FromString(bInspectionOn ? TEXT("Inspection: On (I)") : TEXT("Inspection: Off (I)")), true, bInspectionOn, NAME_None, ECharacterViewerButtonKind::ToggleInspection, &InspectionLabel);
	InspectionButtonText = InspectionLabel;

	const APortfolioCharacterActor* Actor = WeakActor.Get();
	const bool bWireframeAvailable = Actor && Actor->Profile && Actor->Profile->WireframeMaterial != nullptr;
	const bool bWireframeOn = IsWireframeEnabled();
	WireframeButton = AddButtonRow(SectionBox, FText::FromString(bWireframeOn ? TEXT("Wireframe: On (W)") : TEXT("Wireframe: Off (W)")), bWireframeAvailable, bWireframeOn, NAME_None, ECharacterViewerButtonKind::ToggleWireframe);

	Container->AddChildToVerticalBox(SectionBox);
}

void UCharacterViewerWidget::BuildInspectionSection(UVerticalBox* Container)
{
	if (!Container || !WidgetTree)
	{
		return;
	}

	UVerticalBox* SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UTextBlock* BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!SectionBox || !Header || !BodyText)
	{
		return;
	}
	Header->SetText(FText::FromString(TEXT("INSPECTION")));
	Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.f)));
	SectionBox->AddChildToVerticalBox(Header);

	BodyText->SetAutoWrapText(true);

	const bool bShowInspection = IsInspectionEnabled();
	if (bShowInspection)
	{
		FViewerPartInfo Info;
		if (GetSelectedPartInfo(Info))
		{
			BodyText->SetText(FText::FromString(FString::Printf(
				TEXT("%s (%s)\n%s\nTriangles: %d\nMaterial: %s\nTexture: %s"),
				*Info.DisplayName.ToString(),
				*Info.PartType.ToString(),
				*Info.Description.ToString(),
				Info.TriangleCount,
				*Info.MaterialName.ToString(),
				*Info.TextureResolution.ToString())));
		}
		else
		{
			BodyText->SetText(FText::FromString(TEXT("Click a part")));
		}
	}
	SectionBox->AddChildToVerticalBox(BodyText);
	SectionBox->SetVisibility(bShowInspection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	InspectionSectionBox = SectionBox;
	InspectionBodyText = BodyText;

	Container->AddChildToVerticalBox(SectionBox);
}

UButton* UCharacterViewerWidget::AddButtonRow(UVerticalBox* Container, const FText& Label, bool bEnabled, bool bSelected, FName Id, ECharacterViewerButtonKind Kind, UTextBlock** OutTextBlock)
{
	if (!Container || !WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!Button || !ButtonText)
	{
		return nullptr;
	}

	// A "▶ " (▶) prefix on top of any state text already in Label (e.g.
	// "Turntable: On (Space)") makes the current selection/toggle unmistakable
	// (section 13.10 "state display").
	ButtonText->SetText(bSelected ? FText::FromString(FString::Printf(TEXT("▶ %s"), *Label.ToString())) : Label);
	Button->AddChild(ButtonText);
	Button->SetIsEnabled(bEnabled);
	// A focusable UButton keeps keyboard focus after being clicked, so a
	// later Space press (ToggleTurntableAction) re-triggers THIS button
	// (UButton's own Space/Enter-activates-focused-widget behavior) instead
	// of reaching the Controller's Space binding -- e.g. clicking "Reset
	// Camera" then pressing Space re-clicked Reset instead of toggling the
	// turntable. UButton::InitIsFocusable() (the non-deprecated setter) is
	// `protected`, and this runs right after ConstructWidget() before this
	// Button's SWidget exists (same window InitIsFocusable() itself
	// documents as safe), so the direct field write below is equivalent and
	// deliberate, not an oversight.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Button->IsFocusable = false;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UCharacterViewerButtonBinding* Binding = NewObject<UCharacterViewerButtonBinding>(this);
	Binding->Widget = this;
	Binding->Id = Id;
	Binding->Kind = Kind;
	Binding->ButtonWidget = Button;
	ButtonBindings.Add(Binding);

	Button->OnClicked.AddDynamic(Binding, &UCharacterViewerButtonBinding::HandleClicked);

	Container->AddChildToVerticalBox(Button);

	if (OutTextBlock)
	{
		*OutTextBlock = ButtonText;
	}
	return Button;
}
