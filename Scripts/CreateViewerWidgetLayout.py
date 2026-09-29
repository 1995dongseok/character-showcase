"""
Builds the minimal, designer-editable UMG widget tree for WBP_CharacterViewer
via the Editor C++ helper unreal.CharacterViewerEditorTools
.build_default_viewer_widget_layout() (Docs/CHARACTER_VIEWER_SETUP.md section
13.10.1, deliverable A).

Idempotent by design: if WBP_CharacterViewer already has a non-empty designer
tree (widget_tree.root_widget is not None), the C++ helper changes nothing,
logs one "[keep] ... already has a designer tree; not modified." line (plus,
since the 2026-09-29 review fix, which of the 7 required widgets -- if any --
are missing/wrong-type/not-a-variable in that existing tree), and this script
exits 0. The C++ helper returns a three-way
unreal.CharacterViewerWidgetLayoutResult (FAILED/BUILT/KEPT) rather than a
plain bool, so this script's exit code can tell "nothing needed doing" apart
from "actually failed" instead of collapsing both to the same result.

Run headlessly, e.g.:
    "C:\\Program Files\\Epic Games\\UE_5.6\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
        "<project>\\CharacterShowcase.uproject" ^
        -ExecutePythonScript="<project>\\Scripts\\CreateViewerWidgetLayout.py" ^
        -unattended -nosplash -nop4 -log

-----------------------------------------------------------------------------
HISTORY -- why this script no longer builds the tree in pure Python (recorded
2026-09-29, UE 5.6.1, this PC; superseded by the C++ route below on the same
date):

A first attempt tried to reach WBP_CharacterViewer's design-time widget tree
(UWidgetTree) straight from Python (WidgetTree.construct_widget(...) etc.).
It was BLOCKED for two independent, engine-source-confirmed reasons:
1. `unreal.WidgetBlueprint.get_editor_property('widget_tree')` raises
   Exception: "Failed to find property 'widget_tree' ..." -- Engine/Source/
   Editor/UnrealEd/Public/BaseWidgetBlueprint.h declares
   `UPROPERTY() TObjectPtr<UWidgetTree> WidgetTree;` with no CPF_Edit-granting
   specifier, so Python's generic get_editor_property can't find it by name.
2. Independently fatal: `unreal.WidgetTree` does not exist at all in this
   engine build's Python module (Engine/Source/Runtime/UMG/Public/Blueprint/
   WidgetTree.h's `UCLASS(MinimalAPI) class UWidgetTree` has no
   scripting-exposure specifier) -- so even if (1) were solved there would be
   no Python-side class to construct/attach widgets with.

Per the project's failure policy, that pure-Python route was stopped after
one honest attempt and the manual designer-editing fallback was documented
instead (Docs/CHARACTER_VIEWER_SETUP.md section 13.10.1's "남은 수동 작업").

THIS session added a small Editor-only C++ helper instead --
`UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout()`
(Source/CharacterShowcase/Editor/CharacterViewerEditorTools.h/.cpp,
`#if WITH_EDITOR`-guarded, only linked into the Editor target via
Target.bBuildEditor in CharacterShowcase.Build.cs) -- which reaches
UWidgetBlueprint::WidgetTree directly in C++ (where CPF_Edit is irrelevant;
it's a plain UPROPERTY() and C++ access only needs public/friend visibility,
both satisfied here) and constructs the 7-widget tree with
UWidgetTree::ConstructWidget<T>(). This script now simply calls that
BlueprintCallable static function; Python doesn't need to touch UWidgetTree
itself at all, sidestepping both blockers above.
-----------------------------------------------------------------------------
"""

import sys

import unreal

WBP_ASSET_PATH = "/Game/Portfolio/UI/WBP_CharacterViewer"

log = unreal.log
log_err = unreal.log_error


def main():
    log("[CreateViewerWidgetLayout] ==== START ====")

    if not unreal.EditorAssetLibrary.does_asset_exist(WBP_ASSET_PATH):
        log_err(f"[CreateViewerWidgetLayout] {WBP_ASSET_PATH} does not exist. "
                f"Run Scripts/CreatePortfolioAssets.py first to create it.")
        log("[CreateViewerWidgetLayout] ==== BLOCKED ====")
        return 1

    wbp = unreal.load_asset(WBP_ASSET_PATH)
    if wbp is None:
        log_err(f"[CreateViewerWidgetLayout] Could not load {WBP_ASSET_PATH}.")
        log("[CreateViewerWidgetLayout] ==== BLOCKED ====")
        return 1

    if not hasattr(unreal, "CharacterViewerEditorTools"):
        log_err("[CreateViewerWidgetLayout] unreal.CharacterViewerEditorTools is not "
                 "exposed to Python -- this Editor build was not compiled with the new "
                 "Source/CharacterShowcase/Editor/CharacterViewerEditorTools.h/.cpp helper "
                 "(or Target.bBuildEditor's UMGEditor/UnrealEd/Kismet deps did not build). "
                 "WBP_CharacterViewer was NOT modified.")
        log("[CreateViewerWidgetLayout] ==== BLOCKED ====")
        return 1

    # An existing designer tree is now ALWAYS left alone (the old
    # bOnlyIfEmpty=False "force rebuild" path was removed entirely -- see
    # UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout()'s header
    # comment), so this always calls with just the one argument.
    Result = unreal.CharacterViewerWidgetLayoutResult
    result = unreal.CharacterViewerEditorTools.build_default_viewer_widget_layout(wbp)

    if result == Result.BUILT:
        log(f"[CreateViewerWidgetLayout] [build] {WBP_ASSET_PATH}: designer tree built, compiled, and saved.")
        log("[CreateViewerWidgetLayout] ==== DONE ====")
        return 0
    elif result == Result.KEPT:
        log(f"[CreateViewerWidgetLayout] [keep] {WBP_ASSET_PATH}: existing designer tree left alone "
            f"(see the [keep] line logged just above, by UCharacterViewerEditorTools itself, for its "
            f"per-widget shape check).")
        log("[CreateViewerWidgetLayout] ==== DONE ====")
        return 0
    else:
        log_err(f"[CreateViewerWidgetLayout] build_default_viewer_widget_layout() returned FAILED for "
                f"{WBP_ASSET_PATH} -- see the error line logged just above (by "
                f"UCharacterViewerEditorTools itself) for why. WBP_CharacterViewer was NOT modified.")
        log("[CreateViewerWidgetLayout] ==== FAILED ====")
        return 1


_exit_code = main()
if _exit_code != 0:
    log_err(f"[CreateViewerWidgetLayout] Exiting with code {_exit_code}.")
sys.exit(_exit_code)
