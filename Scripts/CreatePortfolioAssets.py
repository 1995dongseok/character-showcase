"""
Creates the Editor-only assets needed by section 13.6 of
Docs/CHARACTER_VIEWER_SETUP.md, using the Unreal Editor Python API instead of
a human clicking through the Editor.

Run headlessly, e.g.:
    "C:\\Program Files\\Epic Games\\UE_5.6\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
        "<project>\\CharacterShowcase.uproject" ^
        -ExecutePythonScript="<project>\\Scripts\\CreatePortfolioAssets.py" ^
        -unattended -nosplash -nop4 -log

Or from within a running Editor: Window > Developer Tools > Python Console,
then `exec(open(r"<project>\\Scripts\\CreatePortfolioAssets.py").read())`, or
Output Log's Cmd combo box does not run Python -- use the Python console.

Default behavior is CREATE-MISSING-ONLY, PRESERVE-EXISTING:
  - If an asset does not exist yet, it is created exactly as before.
  - If an asset already exists, this script does NOT modify or re-save it
    (no lighting/placement/Profile/WBP-layout/Material overwrite). Instead it
    loads the asset read-only and validates it against a minimal expected
    shape, printing one line per asset:
        [keep] <path> OK
        [keep] <path> DIFFERS: <what>
    An existing asset that is incomplete or configured differently is
    reported and skipped -- it is never "fixed" automatically. To pick up a
    script change to an asset's generated content, delete that specific
    asset in the Editor and re-run the script so it is recreated.
  - Config/DefaultEngine.ini is only written if the GlobalDefaultGameMode key
    is missing or different from the expected value; what happened is logged
    either way.
  - Re-running this script is always safe: with everything already present
    it makes no changes at all (see Docs/CHARACTER_VIEWER_SETUP.md section
    13.6 for the preservation test that checks this).

Uses ONLY engine-provided placeholder content (no project art exists yet):
  - Skeletal mesh:  /Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP
  - Anim sequences: .../Tutorial_Idle, .../Tutorial_Walk_Fwd
  - Material:       .../TutorialTPP_Mat
  - Variant material: /Engine/EngineMaterials/WorldGridMaterial
  - Platform mesh:  /Engine/BasicShapes/Cylinder
  - Ambient light:  /Engine/MapTemplates/Sky/DaylightAmbientCubemap (SkyLight cubemap)
This mesh has NO morph targets, so Expressions is limited to a single
"Neutral" entry with an empty Morphs list (valid per
Docs/CHARACTER_VIEWER_SETUP.md section 8/13.2) -- expression verification is
out of scope until a real rigged/morphed character asset is imported.
"""

import sys

import unreal

# ---------------------------------------------------------------------------
# Paths / constants
# ---------------------------------------------------------------------------

DATA_PACKAGE = "/Game/Portfolio/Data"
DATA_ASSET_NAME = "DA_Character"
DATA_ASSET_PATH = f"{DATA_PACKAGE}/{DATA_ASSET_NAME}"

# P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): a second
# profile, so ACharacterViewerGameMode.ProfileLibrary can offer a code-free
# runtime profile switch. Uses another engine-shipped placeholder (a simple
# skeletal cube) instead of a second real character, since no second real
# character asset exists yet -- see Scripts/CreatePortfolioAssets.py module
# docstring for the same rationale applied to DA_Character/TutorialTPP.
DATA_ASSET_CUBE_NAME = "DA_Character_Cube"
DATA_ASSET_CUBE_PATH = f"{DATA_PACKAGE}/{DATA_ASSET_CUBE_NAME}"

BP_PACKAGE = "/Game/Portfolio/Blueprints"
BP_ASSET_NAME = "BP_CharacterViewerGameMode"
BP_ASSET_PATH = f"{BP_PACKAGE}/{BP_ASSET_NAME}"

WBP_PACKAGE = "/Game/Portfolio/UI"
WBP_ASSET_NAME = "WBP_CharacterViewer"
WBP_ASSET_PATH = f"{WBP_PACKAGE}/{WBP_ASSET_NAME}"

MAP_PACKAGE = "/Game/Portfolio/Maps"
MAP_ASSET_NAME = "LV_Portfolio"
MAP_ASSET_PATH = f"{MAP_PACKAGE}/{MAP_ASSET_NAME}"

# P2-4/P2-3: wireframe display material and selection-highlight overlay
# material (Docs/CHARACTER_VIEWER_SETUP.md section 13.11).
MATERIALS_PACKAGE = "/Game/Portfolio/Materials"
WIREFRAME_MAT_NAME = "M_Wireframe"
WIREFRAME_MAT_PATH = f"{MATERIALS_PACKAGE}/{WIREFRAME_MAT_NAME}"
HIGHLIGHT_MAT_NAME = "M_ViewerHighlight"
HIGHLIGHT_MAT_PATH = f"{MATERIALS_PACKAGE}/{HIGHLIGHT_MAT_NAME}"
# Per-part highlight (Docs/CHARACTER_VIEWER_SETUP.md section 6.11): opaque
# material put on a selected part's material slots and on the bone marker
# spheres by APortfolioCharacterActor.
PART_HIGHLIGHT_MAT_NAME = "M_ViewerPartHighlight"
PART_HIGHLIGHT_MAT_PATH = f"{MATERIALS_PACKAGE}/{PART_HIGHLIGHT_MAT_NAME}"

TUTORIAL_MESH = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"
TUTORIAL_IDLE = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"
TUTORIAL_WALK = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"
TUTORIAL_MAT = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Mat.TutorialTPP_Mat"
GRID_MAT = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
SKELETAL_CUBE_MESH = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"
CYLINDER_MESH = "/Engine/BasicShapes/Cylinder.Cylinder"
AMBIENT_CUBEMAP = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"
DARK_MAT = "/Engine/EngineMaterials/T_Default_Material.T_Default_Material"  # fallback if BasicShapeMaterial unavailable

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

errors = []
log = unreal.log
log_warn = unreal.log_warning
log_err = unreal.log_error


def report_exception(context, exc):
    msg = f"[CreatePortfolioAssets] FAILED at: {context}: {exc!r}"
    log_err(msg)
    errors.append(msg)


# ---------------------------------------------------------------------------
# Generic helpers
# ---------------------------------------------------------------------------

def ensure_directory(package_path):
    if not EAL.does_directory_exist(package_path):
        EAL.make_directory(package_path)


def load_or_none(path):
    if not path:
        return None
    obj = unreal.load_asset(path)
    if obj is None:
        log_warn(f"[CreatePortfolioAssets] Could not load '{path}'.")
    return obj


def save(path):
    """Saves a just-created asset. Every call site here is a fresh asset (an
    existing one is validated read-only and never re-saved), so a failed
    save here means the asset this script just built was never actually
    written to disk -- a real failure, not a warning: it is appended to
    `errors` so the script exits non-zero instead of silently reporting
    success at the end."""
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    if not ok:
        msg = f"[CreatePortfolioAssets] FAILED: save_asset returned False for '{path}'."
        log_err(msg)
        errors.append(msg)
    return ok


def report_keep(path, diffs):
    """Prints the one-line [keep] verdict for an existing asset that was
    validated read-only (never modified/re-saved by this script)."""
    if diffs:
        log(f"[CreatePortfolioAssets] [keep] {path} DIFFERS: {'; '.join(diffs)}")
    else:
        log(f"[CreatePortfolioAssets] [keep] {path} OK")


# ---------------------------------------------------------------------------
# 1. DA_Character (CharacterProfileData data asset)
# ---------------------------------------------------------------------------

def validate_character_profile(profile, path):
    """Minimal read-only shape check for an existing CharacterProfileData:
    SkeletalMesh set, DefaultPresetId found in CameraPresets, WireframeMaterial
    set, Parts non-empty. Never writes to `profile`."""
    diffs = []
    try:
        if profile.get_editor_property("skeletal_mesh") is None:
            diffs.append("skeletal_mesh is not set")
    except Exception as exc:
        diffs.append(f"could not read skeletal_mesh ({exc!r})")

    try:
        default_preset_id = profile.get_editor_property("default_preset_id")
        presets = profile.get_editor_property("camera_presets") or []
        preset_ids = [p.get_editor_property("id") for p in presets]
        if default_preset_id not in preset_ids:
            diffs.append(f"default_preset_id '{default_preset_id}' not found in camera_presets {preset_ids}")
    except Exception as exc:
        diffs.append(f"could not read default_preset_id/camera_presets ({exc!r})")

    try:
        if profile.get_editor_property("wireframe_material") is None:
            diffs.append("wireframe_material is not set")
    except Exception as exc:
        diffs.append(f"could not read wireframe_material ({exc!r})")

    try:
        if not profile.get_editor_property("parts"):
            diffs.append("parts is empty")
    except Exception as exc:
        diffs.append(f"could not read parts ({exc!r})")

    report_keep(path, diffs)


def create_or_update_character_profile():
    if EAL.does_asset_exist(DATA_ASSET_PATH):
        profile = EAL.load_asset(DATA_ASSET_PATH)
        log(f"[CreatePortfolioAssets] DA_Character already exists, preserving (read-only): {DATA_ASSET_PATH}")
        validate_character_profile(profile, DATA_ASSET_PATH)
        return profile

    ensure_directory(DATA_PACKAGE)
    data_asset_class = unreal.CharacterProfileData
    factory = unreal.DataAssetFactory()
    try:
        factory.set_editor_property("data_asset_class", data_asset_class)
    except Exception as exc:
        report_exception("DataAssetFactory.data_asset_class", exc)
    profile = asset_tools.create_asset(DATA_ASSET_NAME, DATA_PACKAGE, data_asset_class, factory)
    if profile is None:
        raise RuntimeError("asset_tools.create_asset returned None for DA_Character")
    log(f"[CreatePortfolioAssets] Created DA_Character at {DATA_ASSET_PATH}")

    skel_mesh = load_or_none(TUTORIAL_MESH)
    idle_seq = load_or_none(TUTORIAL_IDLE)
    walk_seq = load_or_none(TUTORIAL_WALK)
    grid_mat = load_or_none(GRID_MAT)

    profile.set_editor_property("display_name", unreal.Text("Tutorial Mannequin (placeholder)"))
    profile.set_editor_property(
        "description",
        unreal.Text(
            "Engine tutorial third-person mannequin used as a placeholder character. "
            "No real portfolio art has been imported yet; this profile exists to prove "
            "the Character Portfolio Viewer end to end with content already shipped in "
            "the engine (Engine/Tutorial/SubEditors/TutorialAssets/Character)."
        ),
    )
    profile.set_editor_property("skeletal_mesh", skel_mesh)

    # --- Default (full body) framing ---
    # TutorialTPP is ~192 cm tall (imported bounds Z 0..192). At FOV 60
    # (horizontal, 16:9 => ~36 deg vertical) a 300 cm / Z 90 framing only
    # covers Z -7..187 and cut the head off; 380 cm around Z 95 covers
    # roughly Z -28..218, i.e. the whole body with some headroom.
    default_framing = unreal.ViewerCameraFraming()
    default_framing.set_editor_property("target_offset", unreal.Vector(0.0, 0.0, 95.0))
    default_framing.set_editor_property("distance", 380.0)
    default_framing.set_editor_property("fov", 60.0)
    default_framing.set_editor_property("min_distance", 120.0)
    default_framing.set_editor_property("max_distance", 700.0)
    default_framing.set_editor_property("min_pitch", -80.0)
    default_framing.set_editor_property("max_pitch", 80.0)
    profile.set_editor_property("default_framing", default_framing)

    # --- Camera presets: Face / Upper / Full ---
    face_framing = unreal.ViewerCameraFraming()
    face_framing.set_editor_property("target_offset", unreal.Vector(0.0, 0.0, 165.0))
    face_framing.set_editor_property("distance", 90.0)
    face_framing.set_editor_property("fov", 45.0)
    face_framing.set_editor_property("min_distance", 40.0)
    face_framing.set_editor_property("max_distance", 250.0)
    face_framing.set_editor_property("min_pitch", -60.0)
    face_framing.set_editor_property("max_pitch", 60.0)

    upper_framing = unreal.ViewerCameraFraming()
    upper_framing.set_editor_property("target_offset", unreal.Vector(0.0, 0.0, 130.0))
    upper_framing.set_editor_property("distance", 170.0)
    upper_framing.set_editor_property("fov", 55.0)
    upper_framing.set_editor_property("min_distance", 80.0)
    upper_framing.set_editor_property("max_distance", 400.0)
    upper_framing.set_editor_property("min_pitch", -70.0)
    upper_framing.set_editor_property("max_pitch", 70.0)

    full_framing = unreal.ViewerCameraFraming()
    full_framing.set_editor_property("target_offset", unreal.Vector(0.0, 0.0, 95.0))
    full_framing.set_editor_property("distance", 380.0)
    full_framing.set_editor_property("fov", 60.0)
    full_framing.set_editor_property("min_distance", 120.0)
    full_framing.set_editor_property("max_distance", 700.0)
    full_framing.set_editor_property("min_pitch", -80.0)
    full_framing.set_editor_property("max_pitch", 80.0)

    def make_preset(id_name, display, framing):
        preset = unreal.ViewerCameraPreset()
        preset.set_editor_property("id", id_name)
        preset.set_editor_property("display_name", unreal.Text(display))
        preset.set_editor_property("framing", framing)
        return preset

    presets = [
        make_preset("Face", "Face", face_framing),
        make_preset("Upper", "Upper Body", upper_framing),
        make_preset("Full", "Full Body", full_framing),
    ]
    profile.set_editor_property("camera_presets", presets)
    profile.set_editor_property("default_preset_id", "Full")

    # --- Animations: Idle (loop), Walk (loop), Pose (Tutorial_Idle, still) ---
    def make_anim(id_name, display, sequence, loop, is_pose, pose_time):
        entry = unreal.ViewerAnimationEntry()
        entry.set_editor_property("id", id_name)
        entry.set_editor_property("display_name", unreal.Text(display))
        entry.set_editor_property("sequence", sequence)
        entry.set_editor_property("loop", loop)
        entry.set_editor_property("is_pose", is_pose)
        entry.set_editor_property("pose_time", pose_time)
        return entry

    animations = [
        make_anim("Idle", "Idle", idle_seq, True, False, 0.0),
        make_anim("Walk", "Walk", walk_seq, True, False, 0.0),
        make_anim("Pose", "Pose", idle_seq, False, True, 0.5),
    ]
    profile.set_editor_property("animations", animations)
    profile.set_editor_property("default_animation_id", "Idle")

    # --- Expressions: Neutral only (mesh has no morph targets) ---
    neutral = unreal.ViewerExpression()
    neutral.set_editor_property("id", "Neutral")
    neutral.set_editor_property("display_name", unreal.Text("Neutral"))
    neutral.set_editor_property("morphs", [])
    profile.set_editor_property("expressions", [neutral])

    # --- Material variants: Default (no overrides) + Grid ---
    slot_name = unreal.Name()
    try:
        if skel_mesh is not None:
            materials = skel_mesh.get_editor_property("materials")
            if materials:
                slot_name = materials[0].get_editor_property("material_slot_name")
                log(f"[CreatePortfolioAssets] TutorialTPP slot 0 name = '{slot_name}'")
    except Exception as exc:
        report_exception("reading TutorialTPP material slot name", exc)

    default_variant = unreal.ViewerMaterialVariant()
    default_variant.set_editor_property("id", "Default")
    default_variant.set_editor_property("display_name", unreal.Text("Default"))
    default_variant.set_editor_property("slots", [])

    grid_slot = unreal.ViewerMaterialSlotOverride()
    grid_slot.set_editor_property("slot_name", slot_name if slot_name else unreal.Name())
    grid_slot.set_editor_property("slot_index", 0)
    grid_slot.set_editor_property("material", grid_mat)

    grid_variant = unreal.ViewerMaterialVariant()
    grid_variant.set_editor_property("id", "Grid")
    grid_variant.set_editor_property("display_name", unreal.Text("Grid"))
    grid_variant.set_editor_property("slots", [grid_slot])

    profile.set_editor_property("material_variants", [default_variant, grid_variant])

    profile.set_editor_property("turntable_speed_degrees_per_second", 20.0)

    # --- P2-0/P2-1/P2-2 Parts: bone-based, from the physics asset's actual
    # constraint tree (Docs/CHARACTER_VIEWER_SETUP.md section 6.9 -- 22
    # bodies across TutorialTPP_PhysicsAsset's 68-bone skeleton).
    # material_slot_names stays EMPTY on purpose: TutorialTPP has a single
    # material slot, so a part cannot be a slot here; the selection highlight
    # therefore uses bone markers on these BoneNames (EViewerHighlightMode::
    # BoneMarkers). TriangleCount/MaterialName/TextureResolution are optional
    # authored notes (whole LOD0/slot 0, shared by every part); for a real
    # character with per-part slots, fill material_slot_names instead and the
    # Viewer measures them (APortfolioCharacterActor::GetPartMeasuredStats).
    tri_count = 6118  # LOD0 total (AssetRegistry "Triangles" tag), measured via Python.
    mat_name = "TutorialTPP_Mat"
    tex_res = "N/A (no texture; TutorialTPP_Mat BaseColor is a solid Constant3Vector color)"

    def make_part(id_name, display, part_type, description, bone_names):
        part = unreal.ViewerPartInfo()
        part.set_editor_property("id", id_name)
        part.set_editor_property("display_name", unreal.Text(display))
        part.set_editor_property("part_type", unreal.Text(part_type))
        part.set_editor_property("description", unreal.Text(description))
        part.set_editor_property("bone_names", [unreal.Name(b) for b in bone_names])
        part.set_editor_property("component_tag", unreal.Name())
        part.set_editor_property("material_slot_names", [])  # single-slot placeholder: bone markers, see above
        part.set_editor_property("triangle_count", tri_count)
        part.set_editor_property("material_name", unreal.Text(mat_name))
        part.set_editor_property("texture_resolution", unreal.Text(tex_res))
        return part

    parts = [
        make_part("Head", "Head", "Body Part", "Head and neck.", ["head", "neck_01"]),
        make_part("Torso", "Torso", "Body Part", "Pelvis and spine.", ["pelvis", "spine_01", "spine_02", "spine_03"]),
        make_part("LeftArm", "Left Arm", "Body Part", "Left clavicle through hand.", ["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"]),
        make_part("RightArm", "Right Arm", "Body Part", "Right clavicle through hand.", ["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"]),
        make_part("LeftLeg", "Left Leg", "Body Part", "Left thigh through ball of foot.", ["thigh_l", "calf_l", "foot_l", "ball_l"]),
        make_part("RightLeg", "Right Leg", "Body Part", "Right thigh through ball of foot.", ["thigh_r", "calf_r", "foot_r", "ball_r"]),
    ]
    profile.set_editor_property("parts", parts)

    wireframe_mat = load_or_none(WIREFRAME_MAT_PATH)
    profile.set_editor_property("wireframe_material", wireframe_mat)

    save(DATA_ASSET_PATH)
    return profile


def measure_skeletal_mesh_extent(mesh):
    """Returns (origin: unreal.Vector, box_extent: unreal.Vector) for `mesh`,
    trying get_bounds() then get_imported_bounds() (both confirmed present on
    unreal.SkeletalMesh in this UE 5.6 Python API; imported bounds are the
    un-transformed import-time bounds, render bounds are get_bounds()).
    Falls back to a conservative guess (50cm half-extent cube) if neither
    call succeeds, so profile creation never hard-fails on this.
    """
    for method_name in ("get_bounds", "get_imported_bounds"):
        method = getattr(mesh, method_name, None)
        if method is None:
            continue
        try:
            bounds = method()
            origin = bounds.origin
            extent = bounds.box_extent
            log(f"[CreatePortfolioAssets] {mesh.get_name()}.{method_name}() -> origin={origin} extent={extent}")
            return origin, extent
        except Exception as exc:
            report_exception(f"measure_skeletal_mesh_extent via {mesh.get_name()}.{method_name}()", exc)
    log_warn(f"[CreatePortfolioAssets] Could not measure bounds for '{mesh.get_name()}', using a 50cm half-extent guess.")
    return unreal.Vector(0.0, 0.0, 0.0), unreal.Vector(50.0, 50.0, 50.0)


# ---------------------------------------------------------------------------
# 1a. M_Wireframe / M_ViewerHighlight / M_ViewerPartHighlight (P2-3/P2-4 materials)
# ---------------------------------------------------------------------------

def validate_material(mat, path, require_wireframe):
    """Minimal read-only shape check for an existing material: must be
    usable on a skeletal mesh (`used_with_skeletal_mesh`), and M_Wireframe
    must additionally have `wireframe` on. Never writes to `mat`."""
    diffs = []
    try:
        if mat.get_editor_property("used_with_skeletal_mesh") is not True:
            diffs.append("used_with_skeletal_mesh is not True")
    except Exception as exc:
        diffs.append(f"could not read used_with_skeletal_mesh ({exc!r})")

    if require_wireframe:
        try:
            if mat.get_editor_property("wireframe") is not True:
                diffs.append("wireframe is not True")
        except Exception as exc:
            diffs.append(f"could not read wireframe ({exc!r})")

    report_keep(path, diffs)


def create_or_update_wireframe_material():
    """M_Wireframe: unlit, opaque, two-sided, Wireframe=True, constant cyan
    emissive. Applied to every material slot while Wireframe is on
    (APortfolioCharacterActor::SetWireframeEnabled); Wireframe=True makes the
    engine render only the mesh's edges, so the constant color only needs to
    be visible/bright, not textured (section 7: no viewmode-wireframe dependency).

    Only ever built once, on first creation -- an existing M_Wireframe is
    validated read-only and never touched (see module docstring; this also
    means the historical `Assertion failed: !IsRooted()` crash from deleting
    and rebuilding an already-referenced material's expression graph on a
    re-run, documented in Docs/CHARACTER_VIEWER_SETUP.md section 13.11.6, can
    no longer happen here -- the expression-graph-rebuild code path for an
    existing asset has been removed entirely, not just guarded)."""
    if EAL.does_asset_exist(WIREFRAME_MAT_PATH):
        mat = EAL.load_asset(WIREFRAME_MAT_PATH)
        log(f"[CreatePortfolioAssets] M_Wireframe already exists, preserving (read-only): {WIREFRAME_MAT_PATH}")
        validate_material(mat, WIREFRAME_MAT_PATH, require_wireframe=True)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset(WIREFRAME_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, factory)
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {WIREFRAME_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created M_Wireframe at {WIREFRAME_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("wireframe", True)
    # Without this, UE falls back to the default material at runtime for any
    # skeletal mesh this is applied to (LogMaterial: "missing
    # bUsedWithSkeletalMesh=True! Default Material will be used in game."),
    # which is exactly how this material is used (SetMaterial on
    # USkeletalMeshComponent slots) -- caught by the first -game smoke run
    # (Docs/CHARACTER_VIEWER_SETUP.md section 13.11).
    mat.set_editor_property("used_with_skeletal_mesh", True)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)
    color_node = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    color_node.set_editor_property("constant", unreal.LinearColor(0.0, 1.0, 1.0, 1.0))  # cyan
    MEL.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)

    save(WIREFRAME_MAT_PATH)
    return mat


HIGHLIGHT_COLOR = unreal.LinearColor(1.0, 0.0, 0.8, 1.0)  # magenta (clearly distinct from the orange-ish placeholder mannequin)
HIGHLIGHT_OPACITY = 0.55


def create_or_update_highlight_material():
    """M_ViewerHighlight: unlit, translucent, two-sided, constant magenta
    emissive (HIGHLIGHT_COLOR) at HIGHLIGHT_OPACITY. Applied via
    USkeletalMeshComponent::SetOverlayMaterial() while a part is selected
    (APortfolioCharacterActor::SetSelectedPart) so the selection is visible
    without a project post-process material reading CustomStencil.

    Only ever built once, on first creation -- same rationale/crash history
    as create_or_update_wireframe_material() above. An existing
    M_ViewerHighlight is validated read-only and never touched."""
    if EAL.does_asset_exist(HIGHLIGHT_MAT_PATH):
        mat = EAL.load_asset(HIGHLIGHT_MAT_PATH)
        log(f"[CreatePortfolioAssets] M_ViewerHighlight already exists, preserving (read-only): {HIGHLIGHT_MAT_PATH}")
        validate_material(mat, HIGHLIGHT_MAT_PATH, require_wireframe=False)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset(HIGHLIGHT_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, factory)
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {HIGHLIGHT_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created M_ViewerHighlight at {HIGHLIGHT_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("wireframe", False)
    # See create_or_update_wireframe_material(): required for
    # SetOverlayMaterial()/SetMaterial() on a USkeletalMeshComponent to
    # actually use this material at runtime instead of silently falling back
    # to the default material.
    mat.set_editor_property("used_with_skeletal_mesh", True)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)
    color_node = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, -50)
    color_node.set_editor_property("constant", HIGHLIGHT_COLOR)
    MEL.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opacity_node = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 100)
    opacity_node.set_editor_property("r", HIGHLIGHT_OPACITY)
    MEL.connect_material_property(opacity_node, "", unreal.MaterialProperty.MP_OPACITY)

    MEL.recompile_material(mat)

    save(HIGHLIGHT_MAT_PATH)
    return mat


PART_HIGHLIGHT_COLOR = unreal.LinearColor(1.0, 0.0, 0.8, 1.0)  # same magenta as M_ViewerHighlight, but opaque


def validate_part_highlight_material(mat, path):
    """Minimal read-only shape check for an existing M_ViewerPartHighlight:
    usable on a skeletal mesh, opaque (it replaces a slot's material, so a
    translucent one would make the part see-through) and unlit. Never writes
    to `mat`."""
    diffs = []
    try:
        if mat.get_editor_property("used_with_skeletal_mesh") is not True:
            diffs.append("used_with_skeletal_mesh is not True")
    except Exception as exc:
        diffs.append(f"could not read used_with_skeletal_mesh ({exc!r})")

    try:
        if mat.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE:
            diffs.append(f"blend_mode is {mat.get_editor_property('blend_mode')}, expected BLEND_OPAQUE")
    except Exception as exc:
        diffs.append(f"could not read blend_mode ({exc!r})")

    try:
        if mat.get_editor_property("shading_model") != unreal.MaterialShadingModel.MSM_UNLIT:
            diffs.append(f"shading_model is {mat.get_editor_property('shading_model')}, expected MSM_UNLIT")
    except Exception as exc:
        diffs.append(f"could not read shading_model ({exc!r})")

    report_keep(path, diffs)


def create_or_update_part_highlight_material():
    """M_ViewerPartHighlight: unlit, OPAQUE, two-sided, constant magenta
    emissive. Used by APortfolioCharacterActor for the per-part selection
    highlight: SetMaterial() on the selected part's material slots
    (MaterialSlots mode) and on the bone marker spheres (BoneMarkers mode).
    Opaque so a highlighted slot reads as a solid colour block (not a tint
    over the original surface) and so it renders identically on the marker
    spheres.

    CREATE-MISSING-ONLY like the materials above: an existing
    M_ViewerPartHighlight is validated read-only and never touched."""
    if EAL.does_asset_exist(PART_HIGHLIGHT_MAT_PATH):
        mat = EAL.load_asset(PART_HIGHLIGHT_MAT_PATH)
        log(f"[CreatePortfolioAssets] M_ViewerPartHighlight already exists, preserving (read-only): {PART_HIGHLIGHT_MAT_PATH}")
        validate_part_highlight_material(mat, PART_HIGHLIGHT_MAT_PATH)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset(PART_HIGHLIGHT_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, factory)
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {PART_HIGHLIGHT_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created M_ViewerPartHighlight at {PART_HIGHLIGHT_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("wireframe", False)
    # Required for SetMaterial() on a USkeletalMeshComponent slot (see
    # create_or_update_wireframe_material()). Static meshes (the marker
    # spheres) need no usage flag.
    mat.set_editor_property("used_with_skeletal_mesh", True)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)
    color_node = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
    color_node.set_editor_property("constant", PART_HIGHLIGHT_COLOR)
    MEL.connect_material_property(color_node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(mat)

    save(PART_HIGHLIGHT_MAT_PATH)
    return mat


# ---------------------------------------------------------------------------
# 1b. DA_Character_Cube (second CharacterProfileData, P1 completion evidence:
#     a code-free runtime profile switch needs a second profile to switch to)
# ---------------------------------------------------------------------------

def create_or_update_character_profile_cube():
    if EAL.does_asset_exist(DATA_ASSET_CUBE_PATH):
        profile = EAL.load_asset(DATA_ASSET_CUBE_PATH)
        log(f"[CreatePortfolioAssets] DA_Character_Cube already exists, preserving (read-only): {DATA_ASSET_CUBE_PATH}")
        validate_character_profile(profile, DATA_ASSET_CUBE_PATH)
        return profile

    ensure_directory(DATA_PACKAGE)
    data_asset_class = unreal.CharacterProfileData
    factory = unreal.DataAssetFactory()
    try:
        factory.set_editor_property("data_asset_class", data_asset_class)
    except Exception as exc:
        report_exception("DataAssetFactory.data_asset_class (cube)", exc)
    profile = asset_tools.create_asset(DATA_ASSET_CUBE_NAME, DATA_PACKAGE, data_asset_class, factory)
    if profile is None:
        raise RuntimeError("asset_tools.create_asset returned None for DA_Character_Cube")
    log(f"[CreatePortfolioAssets] Created DA_Character_Cube at {DATA_ASSET_CUBE_PATH}")

    skel_mesh = load_or_none(SKELETAL_CUBE_MESH)
    grid_mat = load_or_none(GRID_MAT)

    profile.set_editor_property("display_name", unreal.Text("Skeletal Cube (placeholder)"))
    profile.set_editor_property(
        "description",
        unreal.Text(
            "Engine skeletal-mesh cube (/Engine/EngineMeshes/SkeletalCube) used as a second, "
            "clearly different placeholder profile. Its only purpose is to prove that switching "
            "the active character in the viewer (ACharacterViewerController::SwitchProfile / "
            "SelectCharacterProfile) requires no C++/Blueprint code change -- just adding an "
            "entry to ACharacterViewerGameMode.ProfileLibrary. It has no animations or morph "
            "targets, so Animations/Expressions are intentionally empty."
        ),
    )
    profile.set_editor_property("skeletal_mesh", skel_mesh)

    # --- Default framing: measured from the mesh's own bounds so it fills the frame ---
    if skel_mesh is not None:
        origin, extent = measure_skeletal_mesh_extent(skel_mesh)
    else:
        origin, extent = unreal.Vector(0.0, 0.0, 0.0), unreal.Vector(50.0, 50.0, 50.0)

    max_extent = max(extent.x, extent.y, extent.z, 1.0)
    # SkeletalCube measures ~12.6cm half-extent (a ~25cm cube). A naive
    # "fit-the-frustum" minimum (half_extent / tan(fov/2)) leaves ~zero
    # headroom and, for a small object this close to the lens, the first
    # -game smoke run at that minimum showed the cube filling/overflowing
    # the whole frame (see Docs/CHARACTER_VIEWER_SETUP.md section 13.7.2) --
    # so this uses a much larger multiplier (~6x half-extent) for generous
    # headroom on every side, matching DA_Character's own Full framing ratio
    # (distance 380 / TutorialTPP half-height ~96 = ~4x) plus extra margin
    # for how much smaller this object is in absolute (cm) terms.
    cube_distance = max_extent * 6.0
    cube_framing = unreal.ViewerCameraFraming()
    cube_framing.set_editor_property("target_offset", unreal.Vector(origin.x, origin.y, origin.z))
    cube_framing.set_editor_property("distance", cube_distance)
    cube_framing.set_editor_property("fov", 50.0)
    cube_framing.set_editor_property("min_distance", max(max_extent * 2.0, 10.0))
    cube_framing.set_editor_property("max_distance", max_extent * 15.0)
    cube_framing.set_editor_property("min_pitch", -80.0)
    cube_framing.set_editor_property("max_pitch", 80.0)
    profile.set_editor_property("default_framing", cube_framing)

    # A single "Full" preset mirroring DefaultFraming, so DefaultPresetId
    # resolves to a real preset (GetResetFraming() would fall back to
    # DefaultFraming anyway if this were left empty, but this matches
    # DA_Character's shape and keeps the VIEW section non-empty).
    full_preset = unreal.ViewerCameraPreset()
    full_preset.set_editor_property("id", "Full")
    full_preset.set_editor_property("display_name", unreal.Text("Full"))
    full_preset.set_editor_property("framing", cube_framing)
    profile.set_editor_property("camera_presets", [full_preset])
    profile.set_editor_property("default_preset_id", "Full")

    # --- No animations or morph targets on this mesh: leave both empty ---
    profile.set_editor_property("animations", [])
    profile.set_editor_property("default_animation_id", unreal.Name())
    profile.set_editor_property("expressions", [])

    # --- Material variants: Default (no overrides) + one engine-material variant ---
    slot_name = unreal.Name()
    try:
        if skel_mesh is not None:
            materials = skel_mesh.get_editor_property("materials")
            if materials:
                slot_name = materials[0].get_editor_property("material_slot_name")
                log(f"[CreatePortfolioAssets] SkeletalCube slot 0 name = '{slot_name}'")
    except Exception as exc:
        report_exception("reading SkeletalCube material slot name", exc)

    default_variant = unreal.ViewerMaterialVariant()
    default_variant.set_editor_property("id", "Default")
    default_variant.set_editor_property("display_name", unreal.Text("Default"))
    default_variant.set_editor_property("slots", [])

    grid_slot = unreal.ViewerMaterialSlotOverride()
    grid_slot.set_editor_property("slot_name", slot_name if slot_name else unreal.Name())
    grid_slot.set_editor_property("slot_index", 0)
    grid_slot.set_editor_property("material", grid_mat)

    grid_variant = unreal.ViewerMaterialVariant()
    grid_variant.set_editor_property("id", "Grid")
    grid_variant.set_editor_property("display_name", unreal.Text("Grid"))
    grid_variant.set_editor_property("slots", [grid_slot])

    profile.set_editor_property("material_variants", [default_variant, grid_variant])

    profile.set_editor_property("turntable_speed_degrees_per_second", 45.0)

    # P2 completion evidence: one Part so a profile switch away from a
    # selection on DA_Character can be tested against a second profile that
    # also has Parts data (SkeletalCube has no PhysicsAsset -- see
    # Docs/CHARACTER_VIEWER_SETUP.md section 13.11 -- so this Part exists for
    # schema/profile-switch coverage, not for a working click on the cube itself).
    cube_part = unreal.ViewerPartInfo()
    cube_part.set_editor_property("id", "Cube")
    cube_part.set_editor_property("display_name", unreal.Text("Cube"))
    cube_part.set_editor_property("part_type", unreal.Text("Body Part"))
    cube_part.set_editor_property("description", unreal.Text("The entire skeletal cube placeholder (no PhysicsAsset, so no per-bone split)."))
    cube_part.set_editor_property("bone_names", [unreal.Name("Bone01"), unreal.Name("Bone02")])
    cube_part.set_editor_property("component_tag", unreal.Name())
    cube_part.set_editor_property("triangle_count", 12)
    cube_part.set_editor_property("material_name", unreal.Text("Default (engine, unnamed)"))
    cube_part.set_editor_property("texture_resolution", unreal.Text("N/A (no texture)"))
    profile.set_editor_property("parts", [cube_part])

    wireframe_mat = load_or_none(WIREFRAME_MAT_PATH)
    profile.set_editor_property("wireframe_material", wireframe_mat)

    save(DATA_ASSET_CUBE_PATH)
    return profile


# ---------------------------------------------------------------------------
# 2. WBP_CharacterViewer (Widget Blueprint, parent = UCharacterViewerWidget)
#    Created here with an empty designer tree; Scripts/CreateViewerWidgetLayout.py
#    (UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout(), run
#    separately/afterward) fills that tree with the 7-widget designer layout.
#    Only if that tree ends up empty (or that script has not been run yet)
#    does the C++ fallback panel (CharacterViewerWidget::RebuildWidget())
#    build the UI instead, when WidgetTree->RootWidget is null.
# ---------------------------------------------------------------------------

def validate_widget_blueprint(wbp, path):
    """Minimal read-only shape check for an existing WBP_CharacterViewer:
    parent class must be UCharacterViewerWidget. Never writes to `wbp` (in
    particular, never recompiles it)."""
    diffs = []
    try:
        generated_class = wbp.generated_class()
        if generated_class is None:
            diffs.append("generated_class() is None (blueprint not compiled)")
        else:
            cdo = unreal.get_default_object(generated_class)
            if not isinstance(cdo, unreal.CharacterViewerWidget):
                diffs.append(f"parent class is not CharacterViewerWidget (CDO class = {cdo.get_class().get_name()})")
    except Exception as exc:
        diffs.append(f"could not validate parent class ({exc!r})")

    report_keep(path, diffs)


def create_or_update_widget_blueprint():
    if EAL.does_asset_exist(WBP_ASSET_PATH):
        wbp = EAL.load_asset(WBP_ASSET_PATH)
        log(f"[CreatePortfolioAssets] WBP_CharacterViewer already exists, preserving (read-only): {WBP_ASSET_PATH}")
        validate_widget_blueprint(wbp, WBP_ASSET_PATH)
        return wbp

    ensure_directory(WBP_PACKAGE)
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CharacterViewerWidget)
    wbp = asset_tools.create_asset(WBP_ASSET_NAME, WBP_PACKAGE, unreal.WidgetBlueprint, factory)
    if wbp is None:
        raise RuntimeError("asset_tools.create_asset returned None for WBP_CharacterViewer")
    log(f"[CreatePortfolioAssets] Created WBP_CharacterViewer at {WBP_ASSET_PATH}")

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    except Exception as exc:
        report_exception("compile WBP_CharacterViewer", exc)

    save(WBP_ASSET_PATH)
    return wbp


# ---------------------------------------------------------------------------
# 3. BP_CharacterViewerGameMode (Blueprint, parent = ACharacterViewerGameMode)
# ---------------------------------------------------------------------------

def validate_gamemode_blueprint(bp, path):
    """Minimal read-only shape check for an existing BP_CharacterViewerGameMode:
    parent class correct, DefaultProfile set, ProfileLibrary non-empty,
    ViewerWidgetClass set. Never writes to `bp` (in particular, never
    recompiles it or touches its CDO)."""
    diffs = []
    try:
        generated_class = bp.generated_class()
        if generated_class is None:
            diffs.append("generated_class() is None (blueprint not compiled)")
            report_keep(path, diffs)
            return

        cdo = unreal.get_default_object(generated_class)
        if not isinstance(cdo, unreal.CharacterViewerGameMode):
            diffs.append(f"parent class is not CharacterViewerGameMode (CDO class = {cdo.get_class().get_name()})")

        if cdo.get_editor_property("default_profile") is None:
            diffs.append("default_profile is not set")

        if not cdo.get_editor_property("profile_library"):
            diffs.append("profile_library is empty")

        if cdo.get_editor_property("viewer_widget_class") is None:
            diffs.append("viewer_widget_class is not set")
    except Exception as exc:
        diffs.append(f"could not validate ({exc!r})")

    report_keep(path, diffs)


def create_or_update_gamemode_blueprint(profile, cube_profile, wbp):
    if EAL.does_asset_exist(BP_ASSET_PATH):
        bp = EAL.load_asset(BP_ASSET_PATH)
        log(f"[CreatePortfolioAssets] BP_CharacterViewerGameMode already exists, preserving (read-only): {BP_ASSET_PATH}")
        validate_gamemode_blueprint(bp, BP_ASSET_PATH)
        return bp

    ensure_directory(BP_PACKAGE)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.CharacterViewerGameMode)
    bp = asset_tools.create_asset(BP_ASSET_NAME, BP_PACKAGE, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError("asset_tools.create_asset returned None for BP_CharacterViewerGameMode")
    log(f"[CreatePortfolioAssets] Created BP_CharacterViewerGameMode at {BP_ASSET_PATH}")

    generated_class = bp.generated_class()
    if generated_class is None:
        # Freshly created blueprints need a compile before generated_class() is valid.
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as exc:
            report_exception("compile BP_CharacterViewerGameMode (pre)", exc)
        generated_class = bp.generated_class()

    if generated_class is None:
        raise RuntimeError("BP_CharacterViewerGameMode has no generated_class() even after compiling")

    cdo = unreal.get_default_object(generated_class)
    cdo.set_editor_property("default_profile", profile)
    wbp_generated_class = wbp.generated_class() if wbp else None
    cdo.set_editor_property("viewer_widget_class", wbp_generated_class)
    # P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): the
    # runtime CHARACTER UI section offers every entry here (default profile
    # first) via ACharacterViewerController::SelectCharacterProfile() -- no
    # C++/Blueprint change needed to add DA_Character_Cube as a second choice.
    cdo.set_editor_property("profile_library", [profile, cube_profile])

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        report_exception("compile BP_CharacterViewerGameMode (post)", exc)

    save(BP_ASSET_PATH)
    return bp


# ---------------------------------------------------------------------------
# 4. LV_Portfolio (level)
# ---------------------------------------------------------------------------

def validate_level(path):
    """Minimal read-only shape check for an existing LV_Portfolio: exactly
    one APortfolioCharacterActor with Profile set, at least one light,
    World Settings GameModeOverride (DefaultGameMode) set.

    LevelEditorSubsystem.load_level(path) silently discards any unsaved
    edits of the level CURRENTLY open in the Editor, with no prompt -- fine
    for a headless run, but a real correctness bug when this script is run
    from the in-Editor Python console while an artist has unsaved changes
    open (whether in LV_Portfolio itself or in a different level). So:
      - If LV_Portfolio is already the open level, it is validated in place
        (no load_level() call at all -- nothing to discard).
      - Otherwise, in an interactive (non-unattended) session, the
        currently open level's dirty state is checked first
        (EditorLoadingAndSavingUtils.get_dirty_map_packages()); if it is
        dirty, this is skipped entirely with a [keep] line instead of
        risking a silent discard.
      - Only when the open map is clean and different (or the session is
        headless/unattended, where there is no one to lose unsaved work)
        is load_level() actually called.
    This function never calls new_level(), delete_asset(), or
    save_current_level()/save_asset() on the level."""
    diffs = []
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal_editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    current_world = unreal_editor_subsystem.get_editor_world() if unreal_editor_subsystem else None
    current_package = current_world.get_outer() if current_world else None
    current_package_name = current_package.get_name() if current_package else None

    if current_package_name != path:
        is_unattended = unreal.SystemLibrary.is_unattended()
        if not is_unattended:
            try:
                dirty_map_packages = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or []
            except Exception as exc:
                dirty_map_packages = []
                report_exception("EditorLoadingAndSavingUtils.get_dirty_map_packages()", exc)
            dirty_package_names = {p.get_name() for p in dirty_map_packages if p}
            if current_package_name in dirty_package_names:
                log(f"[CreatePortfolioAssets] [keep] {path} not validated (another unsaved level is open)")
                return

        if not level_subsystem.load_level(path):
            diffs.append("LevelEditorSubsystem.load_level() returned False; could not validate contents")
            report_keep(path, diffs)
            return

    try:
        actors = actor_subsystem.get_all_level_actors()

        character_actors = [a for a in actors if isinstance(a, unreal.PortfolioCharacterActor)]
        if len(character_actors) != 1:
            diffs.append(f"expected exactly 1 PortfolioCharacterActor, found {len(character_actors)}")
        elif character_actors[0].get_editor_property("profile") is None:
            diffs.append("PortfolioCharacterActor.profile is not set")

        light_types = (unreal.DirectionalLight, unreal.PointLight, unreal.SpotLight, unreal.RectLight, unreal.SkyLight)
        light_count = sum(1 for a in actors if isinstance(a, light_types))
        if light_count < 1:
            diffs.append("no light actors found")

        world = unreal_editor_subsystem.get_editor_world() if unreal_editor_subsystem else None
        if world is None:
            world = level_subsystem.get_current_level().get_outer()
        world_settings = world.get_world_settings()
        if world_settings.get_editor_property("default_game_mode") is None:
            diffs.append("World Settings DefaultGameMode is not set")
    except Exception as exc:
        diffs.append(f"could not validate level contents ({exc!r})")

    report_keep(path, diffs)


def create_or_update_level(profile, gamemode_bp):
    """Creates LV_Portfolio only if it does not exist yet. An existing
    LV_Portfolio is validated read-only via validate_level() and is never
    deleted, recreated, or re-saved.

    (Root-cause note, Docs/CHARACTER_VIEWER_SETUP.md sections 13.9/13.11.8:
    the previous version of this function deleted and recreated LV_Portfolio
    on every re-run -- first switching to a `/Temp/...` scratch level, then
    EditorAssetLibrary.delete_asset() on LV_Portfolio, then new_level() on
    the same path again. That scratch-level switch's return value was never
    checked, and it always failed in practice ("Failed to validate the
    destination ... There's already an asset at the destination", seen in
    every run's log, e.g. Saved/Crashes/UECC-Windows-40B8CFD74992167DEFFD7B95
    0E878C7C_0000/CreateAssets_P2d.log). Because LV_Portfolio is also the
    Editor's auto-loaded startup map in this project (same log: `Cmd: MAP
    LOAD FILE=".../LV_Portfolio.umap"` right after Editor init), the silent
    scratch-switch failure meant delete_asset() ran on the package backing
    the CURRENTLY LOADED world, and was immediately followed by new_level()
    creating a fresh world at that same path. Two of five observed runs then
    crashed with EXCEPTION_ACCESS_VIOLATION reading address 0xffffffffffffffff
    (a dangling-pointer read) right after the log's last line, "Creating
    Chaos Debug Draw Scene for world LV_Portfolio" -- i.e. inside that
    immediate re-create of the just-deleted, still-referenced world. This
    matches CrashContext.runtime-xml's GameThread call stack, which is
    entirely inside UnrealEditor-CoreUObject with no PythonScriptPlugin
    frames, consistent with the crash happening during the engine-internal
    world/package teardown-and-recreate rather than inside a Python call
    itself. This entire delete+recreate call sequence, including the
    `/Temp/...` scratch step, has been removed below -- an existing
    LV_Portfolio is never deleted or recreated by this script anymore, so
    this call sequence cannot recur. A second, unrelated crash
    (Assertion failed: !IsRooted(), Saved/Crashes/UECC-Windows-4EC0D0634C08C
    B787087C792EADF35E7_0000/CreateAssets_P2b.log) was already root-caused
    and fixed in section 13.11.6 -- rebuilding a material's expression graph
    on an already-loaded, already-referenced existing Material -- and is now
    additionally impossible here because existing materials are validated
    read-only and their expression graphs are never touched at all, see
    create_or_update_wireframe_material()/create_or_update_highlight_material().)
    """
    ensure_directory(MAP_PACKAGE)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if EAL.does_asset_exist(MAP_ASSET_PATH):
        log(f"[CreatePortfolioAssets] LV_Portfolio already exists, preserving (read-only): {MAP_ASSET_PATH}")
        validate_level(MAP_ASSET_PATH)
        return True

    log(f"[CreatePortfolioAssets] Creating new level at {MAP_ASSET_PATH}")
    if not level_subsystem.new_level(MAP_ASSET_PATH):
        msg = f"[CreatePortfolioAssets] FAILED: new_level() returned False for {MAP_ASSET_PATH}; aborting level population."
        log_err(msg)
        errors.append(msg)
        return False

    # NOTE: unreal.Rotator's POSITIONAL order is (roll, pitch, yaw), not
    # (pitch, yaw, roll). Always pass keywords here. The first version of this
    # script used unreal.Rotator(-45.0, 45.0, 0.0) for the key light, which is
    # roll=-45/pitch=+45: the light pointed UP from under the floor, the
    # platform shadowed the whole character and the -game screenshots were
    # black except the head top and one hand (Docs/CHARACTER_VIEWER_SETUP.md
    # section 13.7.1).
    #
    # The viewer camera orbits with yaw 0 = camera on the -X side looking
    # toward +X (ACharacterViewerCameraPawn::UpdateCameraTransform). The
    # engine mannequin mesh faces +Y, so the actor is yawed +90 to face -X,
    # i.e. the default camera sees the character's front, not its side.

    # --- Character actor ---
    character_actor = actor_subsystem.spawn_actor_from_class(
        unreal.PortfolioCharacterActor, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0)
    )
    character_actor.set_editor_property("profile", profile)
    character_actor.set_actor_label("PortfolioCharacter")

    # Exposure is fixed (Config/DefaultEngine.ini r.DefaultFeature.AutoExposure=False
    # pins scene luminance 1 => EV100 3), so the lux values below map to a
    # predictable brightness: a handful of lux on a mid-albedo surface is mid-bright.

    # --- Key light (Directional, movable, from the camera's front-left, 40 deg down) ---
    key_light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 400.0), unreal.Rotator(roll=0.0, pitch=-40.0, yaw=30.0)
    )
    key_light.set_actor_label("KeyLight")
    key_light_comp = key_light.get_component_by_class(unreal.DirectionalLightComponent)
    if key_light_comp:
        key_light_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        # 10 lux looked slightly blown out on the (saturated yellow) mannequin material.
        key_light_comp.set_editor_property("intensity", 7.0)
        # With two directional lights UE shows an on-screen "Multiple directional
        # lights are competing ... ForwardShadingPriority" warning (visible in the
        # screenshots); make the key light the explicit winner.
        try:
            key_light_comp.set_editor_property("forward_shading_priority", 1)
        except Exception as exc:
            report_exception("KeyLight forward_shading_priority", exc)

    # --- Fill light (Directional, movable, from the camera's front-right, no shadows) ---
    fill_light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 450.0), unreal.Rotator(roll=0.0, pitch=-15.0, yaw=-50.0)
    )
    fill_light.set_actor_label("FillLight")
    fill_light_comp = fill_light.get_component_by_class(unreal.DirectionalLightComponent)
    if fill_light_comp:
        fill_light_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        fill_light_comp.set_editor_property("intensity", 2.0)
        fill_light_comp.set_editor_property("cast_shadows", False)
        # Only one directional light may be the atmosphere sun; keep the key as index 0.
        fill_light_comp.set_editor_property("atmosphere_sun_light_index", 1)

    # --- Ambient (SkyLight, movable, engine daylight cubemap) ---
    # A captured-scene SkyLight in a level with no sky captures black and adds
    # no light at all, so use an engine-shipped ambient cubemap instead.
    sky_light = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    )
    sky_light.set_actor_label("AmbientSkyLight")
    sky_light_comp = sky_light.get_component_by_class(unreal.SkyLightComponent)
    if sky_light_comp:
        sky_light_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky_light_comp.set_editor_property("intensity", 1.0)
        try:
            ambient_cubemap = load_or_none(AMBIENT_CUBEMAP)
            if ambient_cubemap:
                sky_light_comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
                sky_light_comp.set_editor_property("cubemap", ambient_cubemap)
            sky_light_comp.recapture_sky()
        except Exception as exc:
            report_exception("SkyLight cubemap", exc)

    # --- Platform (dark cylinder under the character) ---
    platform = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, -10.0), unreal.Rotator(0.0, 0.0, 0.0)
    )
    platform.set_actor_label("PlatformCylinder")
    platform_mesh_comp = platform.get_component_by_class(unreal.StaticMeshComponent)
    cylinder_mesh = load_or_none(CYLINDER_MESH)
    dark_mat = load_or_none(TUTORIAL_MAT)  # reuse an engine-shipped material as a stand-in neutral material
    if platform_mesh_comp:
        if cylinder_mesh:
            platform_mesh_comp.set_static_mesh(cylinder_mesh)
        platform_mesh_comp.set_world_scale3d(unreal.Vector(4.0, 4.0, 0.2))
        platform_mesh_comp.set_mobility(unreal.ComponentMobility.MOVABLE)

    # --- World Settings: GameMode override ---
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        # Fallback for API variants without EditorLevelLibrary.
        world = level_subsystem.get_current_level().get_outer()
    world_settings = world.get_world_settings()
    generated_class = gamemode_bp.generated_class() if gamemode_bp else None
    world_settings.set_editor_property("default_game_mode", generated_class)

    if not level_subsystem.save_current_level():
        msg = f"[CreatePortfolioAssets] FAILED: save_current_level() returned False for {MAP_ASSET_PATH}; not proceeding further."
        log_err(msg)
        errors.append(msg)
        return False

    return True


# ---------------------------------------------------------------------------
# 5. Config/DefaultEngine.ini: GlobalDefaultGameMode -> BP_CharacterViewerGameMode_C
# ---------------------------------------------------------------------------

def update_default_engine_ini():
    """Only writes Config/DefaultEngine.ini's GlobalDefaultGameMode key when
    it is missing entirely or still set to the known C++ default
    (/Script/CharacterShowcase.CharacterViewerGameMode); no other line in the
    file is touched. If the key is present with any OTHER value (e.g. an
    artist/designer deliberately pointed it at a different GameMode), that
    is left completely alone -- only reported as
    "[keep] ... GlobalDefaultGameMode DIFFERS: <value>" -- instead of being
    silently overwritten or duplicated with a second, conflicting line.
    Always logs what it did (or that nothing needed to change)."""
    import os
    import re

    project_dir = unreal.Paths.project_dir()
    ini_path = os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(project_dir), "Config", "DefaultEngine.ini"))
    if not os.path.exists(ini_path):
        log_warn(f"[CreatePortfolioAssets] {ini_path} does not exist, skipping ini update.")
        return

    with open(ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    key = "GlobalDefaultGameMode"
    known_default_value = "/Script/CharacterShowcase.CharacterViewerGameMode"
    expected_value = f"{BP_ASSET_PATH}.{BP_ASSET_NAME}_C"
    new_line = f"{key}={expected_value}"

    match = re.search(rf"^{re.escape(key)}=(.*)$", content, re.MULTILINE)
    current_value = match.group(1).strip() if match else None

    if current_value == expected_value:
        log(f"[CreatePortfolioAssets] [keep] {ini_path} OK (GlobalDefaultGameMode already set to the Blueprint path, not rewritten)")
        return

    if current_value is not None and current_value != known_default_value:
        # Some other value (not missing, not the known C++ default): never
        # overwrite or duplicate it -- just report and move on.
        log(f"[CreatePortfolioAssets] [keep] {ini_path} GlobalDefaultGameMode DIFFERS: {current_value}")
        return

    if match:
        content = content[:match.start()] + new_line + content[match.end():]
        write_reason = f"replaced default C++ GlobalDefaultGameMode line with {new_line}"
    else:
        content += f"\n[/Script/EngineSettings.GameMapsSettings]\n{new_line}\n"
        write_reason = f"appended explicit override {new_line} (GlobalDefaultGameMode key not present)"

    with open(ini_path, "w", encoding="utf-8") as f:
        f.write(content)
    log(f"[CreatePortfolioAssets] Updated {ini_path}: {write_reason}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    log("[CreatePortfolioAssets] ==== START ====")
    create_or_update_wireframe_material()
    create_or_update_highlight_material()
    create_or_update_part_highlight_material()
    profile =create_or_update_character_profile()
    cube_profile = create_or_update_character_profile_cube()
    wbp = create_or_update_widget_blueprint()
    gamemode_bp = create_or_update_gamemode_blueprint(profile, cube_profile, wbp)
    create_or_update_level(profile, gamemode_bp)
    update_default_engine_ini()

    if errors:
        log_err(f"[CreatePortfolioAssets] ==== DONE WITH {len(errors)} ERROR(S) ====")
        for e in errors:
            log_err(e)
        sys.exit(1)
    else:
        log("[CreatePortfolioAssets] ==== DONE, NO ERRORS ====")


main()
