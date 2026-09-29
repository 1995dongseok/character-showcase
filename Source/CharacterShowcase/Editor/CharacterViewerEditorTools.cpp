#include "Editor/CharacterViewerEditorTools.h"

#if WITH_EDITOR

#include "UI/CharacterViewerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/SlateTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

namespace
{
	// The 7 designer-bindable names UCharacterViewerWidget resolves via
	// BindWidgetOptional (see its header's "Two layout sources" comment) plus
	// the UWidget subclass each one must actually be. Used both to build the
	// tree below and to report, without modifying anything, which of the 7
	// are missing/wrong-type/not-a-variable on an EXISTING designer tree.
	// (Lives inside this file's WITH_EDITOR-guarded block: it names editor-only
	// widget types, same reason the includes above are guarded -- see the
	// header's class comment on why the UFUNCTION itself takes a plain
	// UObject* to stay parseable on every target regardless.)
	struct FRequiredViewerWidget
	{
		FName Name;
		UClass* ExpectedClass;
	};

	const TArray<FRequiredViewerWidget>& GetRequiredViewerWidgets()
	{
		static const TArray<FRequiredViewerWidget> Required = {
			{ TEXT("PanelRoot"), UBorder::StaticClass() },
			{ TEXT("NameText"), UTextBlock::StaticClass() },
			{ TEXT("ControlsBox"), UVerticalBox::StaticClass() },
			{ TEXT("DescriptionScroll"), UScrollBox::StaticClass() },
			{ TEXT("DescriptionText"), UTextBlock::StaticClass() },
			{ TEXT("ListsScroll"), UScrollBox::StaticClass() },
			{ TEXT("ListsBox"), UVerticalBox::StaticClass() },
		};
		return Required;
	}
}

#endif // WITH_EDITOR

ECharacterViewerWidgetLayoutResult UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout(UObject* WidgetBlueprintObject)
{
#if WITH_EDITOR

	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(WidgetBlueprintObject);
	if (!WidgetBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] BuildDefaultViewerWidgetLayout: WidgetBlueprint is null or not a UWidgetBlueprint (got '%s')."),
			WidgetBlueprintObject ? *WidgetBlueprintObject->GetClass()->GetName() : TEXT("null"));
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

	if (WidgetBlueprint->ParentClass != UCharacterViewerWidget::StaticClass())
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] '%s' ParentClass is '%s', expected UCharacterViewerWidget; not modified."),
			*WidgetBlueprint->GetName(),
			WidgetBlueprint->ParentClass ? *WidgetBlueprint->ParentClass->GetName() : TEXT("null"));
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (!WidgetTree)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] '%s' has no WidgetTree."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

	if (WidgetTree->RootWidget != nullptr)
	{
		// Always [keep]: an existing designer tree is never dropped/rebuilt
		// (see the header comment for why the old bOnlyIfEmpty=false force
		// path was removed). Report, without modifying anything, which of
		// the 7 required widgets are missing, the wrong type, or present but
		// not marked "Is Variable" (so BindWidgetOptional would not resolve
		// them either) -- this is the only way an artist/CI run finds out
		// their hand-edited tree silently drifted from the required shape.
		TArray<FString> Problems;
		for (const FRequiredViewerWidget& Req : GetRequiredViewerWidgets())
		{
			UWidget* Found = WidgetTree->FindWidget(Req.Name);
			if (!Found)
			{
				Problems.Add(FString::Printf(TEXT("%s: missing"), *Req.Name.ToString()));
			}
			else if (!Found->IsA(Req.ExpectedClass))
			{
				Problems.Add(FString::Printf(TEXT("%s: found as %s, expected %s"), *Req.Name.ToString(), *Found->GetClass()->GetName(), *Req.ExpectedClass->GetName()));
			}
			else if (!Found->bIsVariable)
			{
				Problems.Add(FString::Printf(TEXT("%s: not marked 'Is Variable'"), *Req.Name.ToString()));
			}
		}

		if (Problems.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[keep] %s already has a designer tree; not modified. %d of 7 required widgets have a problem: %s."),
				*WidgetBlueprint->GetName(), Problems.Num(), *FString::Join(Problems, TEXT("; ")));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[keep] %s already has a designer tree; not modified. All 7 required widgets present, correctly typed, and marked 'Is Variable'."),
				*WidgetBlueprint->GetName());
		}
		return ECharacterViewerWidgetLayoutResult::Kept;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	if (!RootCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct the root CanvasPanel for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	WidgetTree->RootWidget = RootCanvas;

	// Right-edge, full-height, 320px-wide dark panel -- same anchors/offsets as
	// the C++ fallback (UCharacterViewerWidget::BuildFallbackUI()), so the
	// designer layout and the fallback look the same until an artist tweaks it.
	UBorder* PanelRoot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelRoot"));
	if (!PanelRoot)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct PanelRoot for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	PanelRoot->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.85f));
	PanelRoot->SetPadding(FMargin(16.f));
	PanelRoot->SetVisibility(ESlateVisibility::Visible);
	PanelRoot->bIsVariable = true;

	if (UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(PanelRoot))
	{
		BorderSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		BorderSlot->SetAlignment(FVector2D(1.f, 0.f));
		BorderSlot->SetOffsets(FMargin(0.f, 0.f, 320.f, 0.f));
	}

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelContent"));
	if (!PanelContent)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct PanelContent for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	PanelRoot->SetContent(PanelContent);

	// Priority order (section 4/13.10.1): NameText -> ControlsBox (CHARACTER/
	// VIEW/DISPLAY) -> DescriptionScroll (limited-height) -> ListsScroll
	// (ANIMATION/EXPRESSION/APPEARANCE/INSPECTION).
	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	if (!NameText)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct NameText for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	{
		FSlateFontInfo NameFont = NameText->GetFont();
		NameFont.Size = 20;
		NameFont.TypefaceFontName = TEXT("Bold");
		NameText->SetFont(NameFont);
	}
	NameText->SetAutoWrapText(true);
	NameText->bIsVariable = true;
	PanelContent->AddChildToVerticalBox(NameText);

	UVerticalBox* ControlsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ControlsBox"));
	if (!ControlsBox)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct ControlsBox for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	ControlsBox->bIsVariable = true;
	PanelContent->AddChildToVerticalBox(ControlsBox);

	UScrollBox* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DescriptionScroll"));
	if (!DescriptionScroll)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct DescriptionScroll for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	DescriptionScroll->bIsVariable = true;
	DescriptionScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);

	if (USizeBox* DescriptionSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DescriptionSizeBox")))
	{
		DescriptionSizeBox->SetMaxDesiredHeight(110.f);
		DescriptionSizeBox->SetContent(DescriptionScroll);
		PanelContent->AddChildToVerticalBox(DescriptionSizeBox);
	}
	else
	{
		PanelContent->AddChildToVerticalBox(DescriptionScroll);
	}

	UTextBlock* DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescriptionText"));
	if (!DescriptionText)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct DescriptionText for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	DescriptionText->SetAutoWrapText(true);
	DescriptionText->bIsVariable = true;
	DescriptionScroll->AddChild(DescriptionText);

	UScrollBox* ListsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ListsScroll"));
	if (!ListsScroll)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct ListsScroll for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	ListsScroll->bIsVariable = true;
	ListsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	// Without an explicit Fill slot size, a VerticalBox gives every child only
	// its own desired (content) height, so ListsScroll never got a bounded
	// height to scroll within and just kept growing off the bottom of the
	// screen instead of scrolling (section 13.10/6.8). Fill makes it take the
	// remaining space in PanelContent, which is what actually makes it scroll.
	if (UVerticalBoxSlot* ListsScrollSlot = Cast<UVerticalBoxSlot>(PanelContent->AddChildToVerticalBox(ListsScroll)))
	{
		ListsScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* ListsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ListsBox"));
	if (!ListsBox)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct ListsBox for '%s'."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}
	ListsBox->bIsVariable = true;
	ListsScroll->AddChild(ListsBox);

	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	if (WidgetBlueprint->Status == EBlueprintStatus::BS_Error)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Built the widget tree for '%s' but the Blueprint compile ended in BS_Error; not saving."), *WidgetBlueprint->GetName());
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

	WidgetBlueprint->MarkPackageDirty();

	UPackage* Package = WidgetBlueprint->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(Package, WidgetBlueprint, *PackageFileName, SaveArgs);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Compiled '%s' but failed to save '%s'."), *WidgetBlueprint->GetName(), *PackageFileName);
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

	UE_LOG(LogTemp, Log, TEXT("[build] %s: built PanelRoot/NameText/ControlsBox/DescriptionScroll/DescriptionText/ListsScroll/ListsBox, compiled, and saved."), *WidgetBlueprint->GetName());
	return ECharacterViewerWidgetLayoutResult::Built;

#else // !WITH_EDITOR

	UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] BuildDefaultViewerWidgetLayout is editor-only; not available in this build."));
	return ECharacterViewerWidgetLayoutResult::Failed;

#endif // WITH_EDITOR
}
