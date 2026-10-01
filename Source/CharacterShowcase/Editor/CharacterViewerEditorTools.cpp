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

	// 8th, optional name: reported as a note (never a problem) when absent.
	static const FName OptionalStatusTextName(TEXT("StatusText"));
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

		const UWidget* StatusWidget = WidgetTree->FindWidget(OptionalStatusTextName);
		const bool bStatusOk = StatusWidget && StatusWidget->IsA(UTextBlock::StaticClass()) && StatusWidget->bIsVariable;
		const TCHAR* StatusNote = bStatusOk
			? TEXT("Optional StatusText present.")
			: TEXT("Optional StatusText missing/not a variable TextBlock (capture status line will not be shown; everything else works).");

		if (Problems.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[keep] %s already has a designer tree; not modified. %d of 7 required widgets have a problem: %s. %s"),
				*WidgetBlueprint->GetName(), Problems.Num(), *FString::Join(Problems, TEXT("; ")), StatusNote);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[keep] %s already has a designer tree; not modified. All 7 required widgets present, correctly typed, and marked 'Is Variable'. %s"),
				*WidgetBlueprint->GetName(), StatusNote);
		}
		return ECharacterViewerWidgetLayoutResult::Kept;
	}

	// Same builder as the runtime C++ fallback (UCharacterViewerWidget::
	// BuildFallbackUI()), so the generated designer tree and the fallback are
	// identical: right-anchored PanelRoot (auto width, see bAutoPanelWidth),
	// fonts, 6-line description, ListsScroll slot = Fill, the 7 required
	// names + optional StatusText, all marked "Is Variable".
	FCharacterViewerLayoutWidgets Built;
	if (!UCharacterViewerWidget::BuildDefaultLayoutTree(WidgetTree, Built))
	{
		UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] Failed to construct the default widget tree for '%s'."), *WidgetBlueprint->GetName());
		WidgetTree->RootWidget = nullptr;
		return ECharacterViewerWidgetLayoutResult::Failed;
	}

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

	UE_LOG(LogTemp, Log, TEXT("[build] %s: built PanelRoot/NameText/ControlsBox/StatusText/DescriptionScroll/DescriptionText/ListsScroll/ListsBox (panel width %.0f, auto), compiled, and saved."),
		*WidgetBlueprint->GetName(), UCharacterViewerWidget::DefaultPanelWidth);
	return ECharacterViewerWidgetLayoutResult::Built;

#else // !WITH_EDITOR

	UE_LOG(LogTemp, Error, TEXT("[CharacterViewerEditorTools] BuildDefaultViewerWidgetLayout is editor-only; not available in this build."));
	return ECharacterViewerWidgetLayoutResult::Failed;

#endif // WITH_EDITOR
}
