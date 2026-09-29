#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterViewerEditorTools.generated.h"

// Distinguishes "nothing needed doing" from "did the work" from "could not do
// it" for BuildDefaultViewerWidgetLayout(), so a calling script (or a human)
// can tell a [keep] (existing designer tree, left alone) apart from an actual
// failure instead of collapsing both to the same `false`/exit-1 result.
UENUM(BlueprintType)
enum class ECharacterViewerWidgetLayoutResult : uint8
{
	Failed,
	Built,
	Kept,
};

// Editor-only helper (Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1): builds
// the minimal, designer-editable WBP_CharacterViewer widget tree
// (PanelRoot/NameText/ControlsBox/DescriptionScroll/DescriptionText/
// ListsScroll/ListsBox -- the exact 7 names/hierarchy in section 13.10.1's
// table) directly through the Editor C++ API. Exists because the Editor
// Python API cannot reach UWidgetBlueprint::WidgetTree in this engine
// version (see Scripts/CreateViewerWidgetLayout.py's history note):
// UBaseWidgetBlueprint::WidgetTree has no CPF_Edit-granting UPROPERTY
// specifier and UWidgetTree itself is not exposed to Python at all.
//
// NOTE on the WidgetBlueprint parameter's type: an earlier version of this
// class took `UWidgetBlueprint*` directly (matching the task spec verbatim)
// and the whole class/header was wrapped in `#if WITH_EDITOR`, with UMGEditor
// only added to CharacterShowcase.Build.cs when Target.bBuildEditor. That
// failed to build the Game (non-editor) target: UnrealHeaderTool parses this
// header (and resolves every UFUNCTION parameter type) for EVERY target,
// including Game, regardless of that target's own WITH_EDITOR value -- so it
// still needed to resolve `UWidgetBlueprint` even for the Game target parse,
// where UMGEditor (and therefore UWidgetBlueprint's declaration) is
// correctly absent from the include path, producing "Error: Unable to find
// 'class' ... with name 'UWidgetBlueprint'" (confirmed by an actual `Build.bat
// CharacterShowcase Win64 Development` run on this engine/PC). The class
// declaration and this UFUNCTION signature are therefore always compiled
// (both targets) and take a plain `UObject*`; the .cpp Cast<UWidgetBlueprint>()s
// it and #if WITH_EDITOR-guards the entire real implementation (falling back
// to a "not available" no-op outside the Editor target), so the *behavior*
// stays editor-only exactly as specified, only the parameter's static type
// had to change to keep UHT happy on every target.
UCLASS()
class CHARACTERSHOWCASE_API UCharacterViewerEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// WidgetBlueprint must be a UWidgetBlueprint (e.g. WBP_CharacterViewer);
	// passed as UObject* so this header needs no editor-only module
	// dependency on any target (see the class comment above). Outside the
	// Editor target (WITH_EDITOR==0) this always returns Failed and logs that
	// it is unavailable, without touching anything.
	//
	// Idempotent, unconditionally: if WidgetBlueprint->WidgetTree->RootWidget
	// is already set, this NEVER touches the asset (not even a dirty flag) --
	// there used to be a bOnlyIfEmpty=false "force" path that dropped and
	// rebuilt an existing tree, but nothing ever called it (Scripts/
	// CreateViewerWidgetLayout.py always passed bOnlyIfEmpty=true) and a
	// force-rebuild of a designer's hand-edited tree is exactly the kind of
	// surprise overwrite this whole class exists to avoid, so that parameter/
	// code path has been removed entirely. Instead it logs one "[keep] <name>
	// already has a designer tree; not modified." line, additionally
	// reporting (as part of that line) which of the 7 required widget names
	// are missing, the wrong type, or not marked "Is Variable" -- still
	// without modifying anything -- and returns Kept.
	//
	// Otherwise (empty tree) it constructs the 7-widget tree, marks each of
	// those 7 widgets bIsVariable=true (so UCharacterViewerWidget's
	// BindWidgetOptional members resolve them), compiles the Blueprint, and
	// saves the package -- but only if the compile did not leave the
	// Blueprint in an error state (WidgetBlueprint->Status == BS_Error); an
	// erroring compile is never saved. Returns Failed (and logs why) if
	// WidgetBlueprint is null/not a UWidgetBlueprint, has no WidgetTree, its
	// ParentClass is not UCharacterViewerWidget, the compile errored, or the
	// save failed. Returns Built only once the tree was actually constructed,
	// compiled cleanly, and saved.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "CharacterShowcase|Viewer")
	static ECharacterViewerWidgetLayoutResult BuildDefaultViewerWidgetLayout(UObject* WidgetBlueprint);
};
