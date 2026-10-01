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

Added 2026-10-01 (Docs/CHARACTER_VIEWER_SETUP.md 6.13), same create-missing-only rule:
  - DA_Character_Manny: the DEFAULT profile, on the engine third-person
    mannequin copied unchanged into /Game/Characters/Mannequins (SKM_Manny_Simple,
    MM_Idle, MF_Unarmed_Walk_Fwd/Jog_Fwd; referenced read-only, never re-saved).
    Also has no morph targets.
  - M_StudioBackdrop / MI_StudioBackdrop and M_StudioFloor / MI_StudioFloor:
    the LV_Portfolio studio backdrop gradient and neutral floor.
  - apply_studio_setup(): the studio lights/backdrop/post-process actors; run
    by this script only when it creates a NEW LV_Portfolio.

Added 2026-10-01 (Docs/CHARACTER_VIEWER_SETUP.md 6.14), same rule:
  - M_WireframeOverlay: the shaded-wireframe overlay material
    (APortfolioCharacterActor::WireframeOverlayMaterial).
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
# Shaded wireframe (2026-10-01, Docs/CHARACTER_VIEWER_SETUP.md 6.14): set as the
# mesh's OVERLAY material by APortfolioCharacterActor while Wireframe is on.
WIREFRAME_OVERLAY_MAT_NAME = "M_WireframeOverlay"
WIREFRAME_OVERLAY_MAT_PATH = f"{MATERIALS_PACKAGE}/{WIREFRAME_OVERLAY_MAT_NAME}"

TUTORIAL_MESH = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"
TUTORIAL_IDLE = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"
TUTORIAL_WALK = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"
TUTORIAL_MAT = "/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP_Mat.TutorialTPP_Mat"
GRID_MAT = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
SKELETAL_CUBE_MESH = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"
CYLINDER_MESH = "/Engine/BasicShapes/Cylinder.Cylinder"
AMBIENT_CUBEMAP = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"
DARK_MAT = "/Engine/EngineMaterials/T_Default_Material.T_Default_Material"  # fallback if BasicShapeMaterial unavailable
SPHERE_MESH = "/Engine/BasicShapes/Sphere.Sphere"

# 2026-10-01: third profile on the engine third-person mannequin (copied
# unchanged into /Game/Characters/Mannequins by the playable-demo D0 commit;
# read-only here -- referenced, never re-saved). It is the DEFAULT profile
# from now on (realistic proportions, 2 material slots, textures, physics
# asset), DA_Character/DA_Character_Cube stay in the ProfileLibrary.
DATA_ASSET_MANNY_NAME = "DA_Character_Manny"
DATA_ASSET_MANNY_PATH = f"{DATA_PACKAGE}/{DATA_ASSET_MANNY_NAME}"
MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"
MANNY_IDLE = "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"
MANNY_WALK = "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"
MANNY_JOG = "/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"

# LOD0 triangle split of SKM_Manny_Simple, measured once (2026-10-01) because
# the 5.6 Python API exposes per-LOD vertex counts and section->slot mapping
# but no per-section/per-bone triangle count: the mesh was exported read-only
# to an ASCII FBX (unreal.Exporter.run_asset_export_task, LOD0 only), each
# triangle was assigned to the Part of its vertices' dominant skin-weight bone
# (majority of the 3 vertices, walking bone parents up to a Part bone) and
# counted per material slot. Sum = 92,178 = the asset's AssetRegistry
# "Triangles" tag (checked again at creation time below). Authored data like
# every TriangleCount: re-measure if the mesh changes.
MANNY_LOD0_TRIANGLES = 92178
MANNY_PART_TRIANGLES = {
    "Head": {"M_HeadLegs": 9206},
    "Torso": {"M_Torso": 22148, "M_HeadLegs": 3532},
    "LeftArm": {"M_Torso": 15928, "M_HeadLegs": 3752},
    "RightArm": {"M_Torso": 15928, "M_HeadLegs": 3752},
    "LeftLeg": {"M_HeadLegs": 8962, "M_Torso": 4},
    "RightLeg": {"M_HeadLegs": 8962, "M_Torso": 4},
}

# Studio look for LV_Portfolio (2026-10-01): gradient backdrop + neutral floor
# materials (parameters exposed through Material Instances so an artist changes
# colours in the MI Details panel without touching the graph).
STUDIO_BACKDROP_MAT_NAME = "M_StudioBackdrop"
STUDIO_BACKDROP_MAT_PATH = f"{MATERIALS_PACKAGE}/{STUDIO_BACKDROP_MAT_NAME}"
STUDIO_BACKDROP_MI_NAME = "MI_StudioBackdrop"
STUDIO_BACKDROP_MI_PATH = f"{MATERIALS_PACKAGE}/{STUDIO_BACKDROP_MI_NAME}"
STUDIO_FLOOR_MAT_NAME = "M_StudioFloor"
STUDIO_FLOOR_MAT_PATH = f"{MATERIALS_PACKAGE}/{STUDIO_FLOOR_MAT_NAME}"
STUDIO_FLOOR_MI_NAME = "MI_StudioFloor"
STUDIO_FLOOR_MI_PATH = f"{MATERIALS_PACKAGE}/{STUDIO_FLOOR_MI_NAME}"

# Linear colours. Backdrop is UNLIT (emissive), so these are the on-screen
# values before the tonemapper at the level's fixed exposure (see STUDIO_*).
STUDIO_BACKDROP_VECTORS = {
    "BottomColor": (0.060, 0.063, 0.068),  # at the floor horizon (Z = GradientBottomZ)
    "TopColor": (0.006, 0.0065, 0.008),    # GradientHeight cm above it and up
}
STUDIO_BACKDROP_SCALARS = {
    "GradientBottomZ": 0.0,
    "GradientHeight": 1200.0,
    "Brightness": 1.0,
}
STUDIO_FLOOR_VECTORS = {
    "BaseColor": (0.18, 0.18, 0.18),       # 18% mid-grey albedo around the character
    "EdgeColor": (0.035, 0.035, 0.038),    # albedo the floor fades to far away (close to the backdrop horizon)
}
STUDIO_FLOOR_SCALARS = {
    "Roughness": 0.7,
    "Specular": 0.3,
    "FadeStartRadius": 350.0,              # cm from the floor actor's centre
    "FadeEndRadius": 2400.0,
}

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


WIREFRAME_OVERLAY_COLOR = (0.0, 1.0, 1.0)  # same cyan as M_Wireframe
# Each vertex of the overlay pass is moved toward the camera by this fraction
# of its distance to the camera (World Position Offset = (CameraPosition -
# WorldPosition) * fraction). Moving along the view ray does not change where
# the line lands on screen, only its depth, so the lines win the depth test
# against the identical shaded surface instead of z-fighting into dashes.
# 0.002 = 0.9 cm at the 4.6 m Full Body distance, 0.36 cm at the 1.8 m Face distance.
WIREFRAME_OVERLAY_SCALARS = {"DepthBiasFraction": 0.002}
WIREFRAME_OVERLAY_VECTORS = {"LineColor": WIREFRAME_OVERLAY_COLOR}


def validate_wireframe_overlay_material(mat, path):
    """Read-only check for an existing M_WireframeOverlay: skeletal-mesh
    usage, Wireframe on, unlit, opaque, and the two exposed parameters.
    Never writes to `mat`."""
    diffs = []
    expected = (
        ("used_with_skeletal_mesh", True, "used_with_skeletal_mesh is not True"),
        ("wireframe", True, "wireframe is not True"),
        ("two_sided", True, "two_sided is not True"),
    )
    for prop, value, message in expected:
        try:
            if mat.get_editor_property(prop) is not value:
                diffs.append(message)
        except Exception as exc:
            diffs.append(f"could not read {prop} ({exc!r})")
    try:
        if mat.get_editor_property("shading_model") != unreal.MaterialShadingModel.MSM_UNLIT:
            diffs.append(f"shading_model is {mat.get_editor_property('shading_model')}, expected MSM_UNLIT")
        if mat.get_editor_property("blend_mode") != unreal.BlendMode.BLEND_OPAQUE:
            diffs.append(f"blend_mode is {mat.get_editor_property('blend_mode')}, expected BLEND_OPAQUE")
    except Exception as exc:
        diffs.append(f"could not read shading_model/blend_mode ({exc!r})")
    try:
        MEL = unreal.MaterialEditingLibrary
        have_scalars = {str(n) for n in MEL.get_scalar_parameter_names(mat)}
        have_vectors = {str(n) for n in MEL.get_vector_parameter_names(mat)}
        missing = [n for n in WIREFRAME_OVERLAY_SCALARS if n not in have_scalars] + \
                  [n for n in WIREFRAME_OVERLAY_VECTORS if n not in have_vectors]
        if missing:
            diffs.append(f"missing parameters {missing}")
    except Exception as exc:
        diffs.append(f"could not read parameter names ({exc!r})")
    report_keep(path, diffs)


def create_or_update_wireframe_overlay_material():
    """M_WireframeOverlay: unlit, OPAQUE, two-sided, Wireframe=True, cyan
    emissive (`LineColor`), plus a camera-ward World Position Offset
    (`DepthBiasFraction`, see WIREFRAME_OVERLAY_SCALARS). Set with
    USkeletalMeshComponent::SetOverlayMaterial() while Wireframe is on
    (APortfolioCharacterActor, EViewerWireframeMode::Overlay): the engine
    draws the overlay as an extra mesh pass of the same sections (any blend
    mode is accepted; only the skeletal-mesh usage flag is checked), so the
    shaded surface stays visible with cyan lines on top -- unlike M_Wireframe,
    which replaces every slot and turns a 92k-triangle mesh into a solid cyan
    silhouette. Opaque so the lines are written like normal geometry (no
    translucency sorting).

    CREATE-MISSING-ONLY like the materials above: an existing
    M_WireframeOverlay is validated read-only and never touched."""
    if EAL.does_asset_exist(WIREFRAME_OVERLAY_MAT_PATH):
        mat = EAL.load_asset(WIREFRAME_OVERLAY_MAT_PATH)
        log(f"[CreatePortfolioAssets] {WIREFRAME_OVERLAY_MAT_NAME} already exists, preserving (read-only): {WIREFRAME_OVERLAY_MAT_PATH}")
        validate_wireframe_overlay_material(mat, WIREFRAME_OVERLAY_MAT_PATH)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    mat = asset_tools.create_asset(WIREFRAME_OVERLAY_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {WIREFRAME_OVERLAY_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created {WIREFRAME_OVERLAY_MAT_NAME} at {WIREFRAME_OVERLAY_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("wireframe", True)
    # Required: the skeletal mesh scene proxy drops an overlay material
    # without this usage flag ("Overlay material with missing usage flag").
    mat.set_editor_property("used_with_skeletal_mesh", True)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)

    color = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
    color.set_editor_property("parameter_name", "LineColor")
    rgb = WIREFRAME_OVERLAY_VECTORS["LineColor"]
    color.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    color.set_editor_property("group", "Wireframe")
    if not MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        errors.append(f"[CreatePortfolioAssets] FAILED: connect emissive on {WIREFRAME_OVERLAY_MAT_NAME}")

    camera_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -800, 200)
    world_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -800, 320)
    to_camera = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -600, 250)
    bias = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 400)
    bias.set_editor_property("parameter_name", "DepthBiasFraction")
    bias.set_editor_property("default_value", WIREFRAME_OVERLAY_SCALARS["DepthBiasFraction"])
    bias.set_editor_property("group", "Wireframe")
    offset = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 300)
    _connect(camera_pos, "", to_camera, "A")
    _connect(world_pos, "", to_camera, "B")
    _connect(to_camera, "", offset, "A")
    _connect(bias, "", offset, "B")
    if not MEL.connect_material_property(offset, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        errors.append(f"[CreatePortfolioAssets] FAILED: connect world position offset on {WIREFRAME_OVERLAY_MAT_NAME}")

    MEL.recompile_material(mat)
    save(WIREFRAME_OVERLAY_MAT_PATH)
    return mat


# ---------------------------------------------------------------------------
# 1a-2. Studio backdrop / floor materials + instances (LV_Portfolio studio look)
# ---------------------------------------------------------------------------

def validate_studio_material(mat, path, shading_model, scalar_names, vector_names):
    """Read-only check for an existing studio material: shading model and the
    exposed parameter names the Material Instance relies on. Never writes."""
    diffs = []
    try:
        if mat.get_editor_property("shading_model") != shading_model:
            diffs.append(f"shading_model is {mat.get_editor_property('shading_model')}, expected {shading_model}")
    except Exception as exc:
        diffs.append(f"could not read shading_model ({exc!r})")
    try:
        MEL = unreal.MaterialEditingLibrary
        have_scalars = {str(n) for n in MEL.get_scalar_parameter_names(mat)}
        have_vectors = {str(n) for n in MEL.get_vector_parameter_names(mat)}
        missing = [n for n in scalar_names if n not in have_scalars] + [n for n in vector_names if n not in have_vectors]
        if missing:
            diffs.append(f"missing parameters {missing}")
    except Exception as exc:
        diffs.append(f"could not read parameter names ({exc!r})")
    report_keep(path, diffs)


def validate_studio_material_instance(mi, path, parent_path):
    """Read-only check for an existing studio Material Instance: parent must be
    the matching studio material. Parameter VALUES are artist-owned and are
    deliberately not compared. Never writes."""
    diffs = []
    try:
        parent = mi.get_editor_property("parent")
        if parent is None or parent.get_path_name().split(".")[0] != parent_path:
            diffs.append(f"parent is {parent.get_path_name() if parent else None}, expected {parent_path}")
    except Exception as exc:
        diffs.append(f"could not read parent ({exc!r})")
    report_keep(path, diffs)


def _connect(from_node, from_output, to_node, to_input):
    """MaterialEditingLibrary.connect_material_expressions with a hard failure
    (appended to `errors`) instead of a silently half-wired graph."""
    ok = unreal.MaterialEditingLibrary.connect_material_expressions(from_node, from_output, to_node, to_input)
    if not ok:
        msg = f"[CreatePortfolioAssets] FAILED: connect {from_node.get_name()}.{from_output!r} -> {to_node.get_name()}.{to_input!r}"
        log_err(msg)
        errors.append(msg)
    return ok


def _scalar_param(mat, name, default, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    node.set_editor_property("group", "Studio")
    return node


def _vector_param(mat, name, rgb, x, y):
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    node.set_editor_property("group", "Studio")
    return node


def create_or_update_studio_backdrop_material():
    """M_StudioBackdrop: unlit, opaque, two-sided (seen from INSIDE a large
    engine sphere), vertical gradient on absolute world Z:
        Emissive = lerp(BottomColor, TopColor,
                        saturate((WorldPosition.Z - GradientBottomZ) / GradientHeight)) * Brightness
    Unlit so the backdrop colour is exactly what the artist picks (no light
    or shadow on it). Built once on creation only; an existing asset is
    validated read-only (same rule as M_Wireframe)."""
    scalar_names = list(STUDIO_BACKDROP_SCALARS)
    vector_names = list(STUDIO_BACKDROP_VECTORS)
    if EAL.does_asset_exist(STUDIO_BACKDROP_MAT_PATH):
        mat = EAL.load_asset(STUDIO_BACKDROP_MAT_PATH)
        log(f"[CreatePortfolioAssets] {STUDIO_BACKDROP_MAT_NAME} already exists, preserving (read-only): {STUDIO_BACKDROP_MAT_PATH}")
        validate_studio_material(mat, STUDIO_BACKDROP_MAT_PATH, unreal.MaterialShadingModel.MSM_UNLIT, scalar_names, vector_names)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    mat = asset_tools.create_asset(STUDIO_BACKDROP_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {STUDIO_BACKDROP_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created {STUDIO_BACKDROP_MAT_NAME} at {STUDIO_BACKDROP_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)
    world_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    mask_z = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1200, 0)
    for channel, on in (("r", False), ("g", False), ("b", True), ("a", False)):
        mask_z.set_editor_property(channel, on)
    bottom_z = _scalar_param(mat, "GradientBottomZ", STUDIO_BACKDROP_SCALARS["GradientBottomZ"], -1200, 150)
    height = _scalar_param(mat, "GradientHeight", STUDIO_BACKDROP_SCALARS["GradientHeight"], -1000, 250)
    sub = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1000, 50)
    div = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -800, 100)
    sat = MEL.create_material_expression(mat, unreal.MaterialExpressionSaturate, -600, 100)
    bottom_color = _vector_param(mat, "BottomColor", STUDIO_BACKDROP_VECTORS["BottomColor"], -600, -250)
    top_color = _vector_param(mat, "TopColor", STUDIO_BACKDROP_VECTORS["TopColor"], -600, -50)
    lerp = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -50)
    brightness = _scalar_param(mat, "Brightness", STUDIO_BACKDROP_SCALARS["Brightness"], -350, 150)
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 0)

    _connect(world_pos, "", mask_z, "")
    _connect(mask_z, "", sub, "A")
    _connect(bottom_z, "", sub, "B")
    _connect(sub, "", div, "A")
    _connect(height, "", div, "B")
    _connect(div, "", sat, "")
    _connect(bottom_color, "", lerp, "A")
    _connect(top_color, "", lerp, "B")
    _connect(sat, "", lerp, "Alpha")
    _connect(lerp, "", mul, "A")
    _connect(brightness, "", mul, "B")
    if not MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        errors.append(f"[CreatePortfolioAssets] FAILED: connect emissive on {STUDIO_BACKDROP_MAT_NAME}")
    MEL.recompile_material(mat)

    save(STUDIO_BACKDROP_MAT_PATH)
    return mat


def create_or_update_studio_floor_material():
    """M_StudioFloor: default lit, opaque. BaseColor fades radially (distance
    in XY from the floor actor's own pivot, so it follows the platform if it is
    moved) from BaseColor to EdgeColor between FadeStartRadius and
    FadeEndRadius, so the far floor sinks into the dark backdrop and its edge
    does not read as a hard line. Specular fades to 0 with the same radial
    alpha: without that, grazing-angle reflections of the SkyLight cubemap made
    the far floor a bright band right under the horizon (calibration render
    2026-10-01). Roughness/Specular exposed. No texture, no checker. Built once on creation only; an existing asset is validated
    read-only."""
    scalar_names = list(STUDIO_FLOOR_SCALARS)
    vector_names = list(STUDIO_FLOOR_VECTORS)
    if EAL.does_asset_exist(STUDIO_FLOOR_MAT_PATH):
        mat = EAL.load_asset(STUDIO_FLOOR_MAT_PATH)
        log(f"[CreatePortfolioAssets] {STUDIO_FLOOR_MAT_NAME} already exists, preserving (read-only): {STUDIO_FLOOR_MAT_PATH}")
        validate_studio_material(mat, STUDIO_FLOOR_MAT_PATH, unreal.MaterialShadingModel.MSM_DEFAULT_LIT, scalar_names, vector_names)
        return mat

    ensure_directory(MATERIALS_PACKAGE)
    mat = asset_tools.create_asset(STUDIO_FLOOR_MAT_NAME, MATERIALS_PACKAGE, unreal.Material, unreal.MaterialFactoryNew())
    if mat is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {STUDIO_FLOOR_MAT_NAME}")
    log(f"[CreatePortfolioAssets] Created {STUDIO_FLOOR_MAT_NAME} at {STUDIO_FLOOR_MAT_PATH}")

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)

    MEL = unreal.MaterialEditingLibrary
    MEL.delete_all_material_expressions(mat)
    world_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1600, 0)
    mask_wp = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1400, 0)
    obj_pos = MEL.create_material_expression(mat, unreal.MaterialExpressionObjectPositionWS, -1600, 150)
    mask_op = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -1400, 150)
    for mask in (mask_wp, mask_op):
        for channel, on in (("r", True), ("g", True), ("b", False), ("a", False)):
            mask.set_editor_property(channel, on)
    dist = MEL.create_material_expression(mat, unreal.MaterialExpressionDistance, -1200, 50)
    fade_start = _scalar_param(mat, "FadeStartRadius", STUDIO_FLOOR_SCALARS["FadeStartRadius"], -1200, 250)
    fade_end = _scalar_param(mat, "FadeEndRadius", STUDIO_FLOOR_SCALARS["FadeEndRadius"], -1200, 400)
    sub_dist = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1000, 100)
    sub_range = MEL.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1000, 300)
    div = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -800, 150)
    sat = MEL.create_material_expression(mat, unreal.MaterialExpressionSaturate, -600, 150)
    base_color = _vector_param(mat, "BaseColor", STUDIO_FLOOR_VECTORS["BaseColor"], -600, -250)
    edge_color = _vector_param(mat, "EdgeColor", STUDIO_FLOOR_VECTORS["EdgeColor"], -600, -50)
    lerp = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -350, -50)
    roughness = _scalar_param(mat, "Roughness", STUDIO_FLOOR_SCALARS["Roughness"], -350, 200)
    specular = _scalar_param(mat, "Specular", STUDIO_FLOOR_SCALARS["Specular"], -350, 320)
    one_minus = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -350, 420)
    spec_fade = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 350)

    _connect(world_pos, "", mask_wp, "")
    _connect(obj_pos, "", mask_op, "")
    _connect(mask_wp, "", dist, "A")
    _connect(mask_op, "", dist, "B")
    _connect(dist, "", sub_dist, "A")
    _connect(fade_start, "", sub_dist, "B")
    _connect(fade_end, "", sub_range, "A")
    _connect(fade_start, "", sub_range, "B")
    _connect(sub_dist, "", div, "A")
    _connect(sub_range, "", div, "B")
    _connect(div, "", sat, "")
    _connect(base_color, "", lerp, "A")
    _connect(edge_color, "", lerp, "B")
    _connect(sat, "", lerp, "Alpha")
    _connect(sat, "", one_minus, "")
    _connect(specular, "", spec_fade, "A")
    _connect(one_minus, "", spec_fade, "B")
    for node, prop in ((lerp, unreal.MaterialProperty.MP_BASE_COLOR),
                       (roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                       (spec_fade, unreal.MaterialProperty.MP_SPECULAR)):
        if not MEL.connect_material_property(node, "", prop):
            errors.append(f"[CreatePortfolioAssets] FAILED: connect {prop} on {STUDIO_FLOOR_MAT_NAME}")
    MEL.recompile_material(mat)

    save(STUDIO_FLOOR_MAT_PATH)
    return mat


def create_or_update_studio_material_instance(mi_name, mi_path, parent, parent_path, vectors, scalars):
    """MI_StudioBackdrop / MI_StudioFloor: Material Instance of the matching
    studio material with every exposed parameter set explicitly (so the
    artist sees and edits them in the MI's Details panel). Created once;
    an existing MI is validated read-only (its values are artist-owned)."""
    if EAL.does_asset_exist(mi_path):
        mi = EAL.load_asset(mi_path)
        log(f"[CreatePortfolioAssets] {mi_name} already exists, preserving (read-only): {mi_path}")
        validate_studio_material_instance(mi, mi_path, parent_path)
        return mi

    if parent is None:
        msg = f"[CreatePortfolioAssets] FAILED: parent material for {mi_name} is missing"
        log_err(msg)
        errors.append(msg)
        return None

    ensure_directory(MATERIALS_PACKAGE)
    factory = unreal.MaterialInstanceConstantFactoryNew()
    mi = asset_tools.create_asset(mi_name, MATERIALS_PACKAGE, unreal.MaterialInstanceConstant, factory)
    if mi is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {mi_name}")
    log(f"[CreatePortfolioAssets] Created {mi_name} at {mi_path}")

    MEL = unreal.MaterialEditingLibrary
    MEL.set_material_instance_parent(mi, parent)
    # The setters' bool return is not a reliable success signal in 5.6 (it
    # returned False while the value was applied), so read every value back.
    for name, rgb in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        got = MEL.get_material_instance_vector_parameter_value(mi, name)
        if abs(got.r - rgb[0]) > 1e-4 or abs(got.g - rgb[1]) > 1e-4 or abs(got.b - rgb[2]) > 1e-4:
            errors.append(f"[CreatePortfolioAssets] FAILED: {mi_name} vector parameter {name} reads back {got}")
    for name, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, name, value)
        got = MEL.get_material_instance_scalar_parameter_value(mi, name)
        if abs(got - value) > 1e-3:
            errors.append(f"[CreatePortfolioAssets] FAILED: {mi_name} scalar parameter {name} reads back {got}")
    MEL.update_material_instance(mi)

    save(mi_path)
    return mi


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
# 1c. DA_Character_Manny (engine third-person mannequin, DEFAULT profile)
# ---------------------------------------------------------------------------

def read_mesh_bone_names(mesh):
    """Bone names of `mesh`'s own reference skeleton (read-only: a
    SkeletonModifier that is never committed). Empty set if unavailable."""
    try:
        modifier = unreal.SkeletonModifier()
        if modifier.set_skeletal_mesh(mesh):
            return {str(n) for n in modifier.get_all_bone_names()}
    except Exception as exc:
        report_exception("SkeletonModifier.get_all_bone_names", exc)
    return set()


def read_reference_bone_z(skeleton, bone_names):
    """{bone: world-space Z (cm) in the skeleton's reference pose} via
    AnimPoseExtensions (read-only). Missing bones are simply absent."""
    result = {}
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        for bone in bone_names:
            try:
                result[bone] = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation.z
            except Exception as exc:
                report_exception(f"reference pose of bone '{bone}'", exc)
    except Exception as exc:
        report_exception("AnimPoseExtensions.get_reference_pose", exc)
    return result


def describe_material_textures(material):
    """Texture parameter resolutions of a Material Instance, read from the
    textures' AssetRegistry "Dimensions" tag, e.g.
    'Base Texture 1024x1024, BNormal 4096x4096, MRA 1024x1024'."""
    parts = []
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        for value in material.get_editor_property("texture_parameter_values") or []:
            tex = value.get_editor_property("parameter_value")
            if tex is None:
                continue
            name = value.get_editor_property("parameter_info").get_editor_property("name")
            dims = registry.get_asset_by_object_path(tex.get_path_name()).get_tag_value("Dimensions")
            parts.append(f"{name} {dims}")
    except Exception as exc:
        report_exception(f"reading texture parameters of {material.get_name() if material else None}", exc)
    return ", ".join(parts) if parts else "N/A (no texture parameters)"


def fit_camera_distance(half_height_cm, fov_deg, aspect=16.0 / 9.0):
    """Distance at which a vertical half-extent fills half the frame height.
    FViewerCameraFraming.FOV is the HORIZONTAL FOV (UE default
    AspectRatioAxisConstraint MaintainXFOV), so the vertical half-angle is
    atan(tan(FOV/2) / aspect)."""
    import math
    tan_vertical = math.tan(math.radians(fov_deg) * 0.5) / aspect
    return half_height_cm / tan_vertical


def make_framing(target_z, distance, fov, min_distance, max_distance, min_pitch, max_pitch):
    framing = unreal.ViewerCameraFraming()
    framing.set_editor_property("target_offset", unreal.Vector(0.0, 0.0, round(target_z, 1)))
    framing.set_editor_property("distance", round(distance, 1))
    framing.set_editor_property("fov", fov)
    framing.set_editor_property("min_distance", round(min_distance, 1))
    framing.set_editor_property("max_distance", round(max_distance, 1))
    framing.set_editor_property("min_pitch", min_pitch)
    framing.set_editor_property("max_pitch", max_pitch)
    return framing


def max_pitch_above_floor(target_z, max_distance, floor_clearance=5.0, cap=60.0):
    """Positive pitch puts the orbit camera BELOW its target
    (ACharacterViewerCameraPawn: camera = target - Forward * Distance). Clamp
    it so that even at MaxDistance the camera stays above the studio floor
    (Z = 0) instead of looking at the floor from underneath."""
    import math
    ratio = max(0.0, min(1.0, (target_z - floor_clearance) / max(max_distance, 1.0)))
    return float(min(cap, math.floor(math.degrees(math.asin(ratio)))))


def create_or_update_character_profile_manny():
    if EAL.does_asset_exist(DATA_ASSET_MANNY_PATH):
        profile = EAL.load_asset(DATA_ASSET_MANNY_PATH)
        log(f"[CreatePortfolioAssets] {DATA_ASSET_MANNY_NAME} already exists, preserving (read-only): {DATA_ASSET_MANNY_PATH}")
        validate_character_profile(profile, DATA_ASSET_MANNY_PATH)
        return profile

    skel_mesh = load_or_none(MANNY_MESH)
    if skel_mesh is None:
        msg = f"[CreatePortfolioAssets] FAILED: {MANNY_MESH} is missing; {DATA_ASSET_MANNY_NAME} not created."
        log_err(msg)
        errors.append(msg)
        return None

    ensure_directory(DATA_PACKAGE)
    data_asset_class = unreal.CharacterProfileData
    factory = unreal.DataAssetFactory()
    try:
        factory.set_editor_property("data_asset_class", data_asset_class)
    except Exception as exc:
        report_exception("DataAssetFactory.data_asset_class (manny)", exc)
    profile = asset_tools.create_asset(DATA_ASSET_MANNY_NAME, DATA_PACKAGE, data_asset_class, factory)
    if profile is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {DATA_ASSET_MANNY_NAME}")
    log(f"[CreatePortfolioAssets] Created {DATA_ASSET_MANNY_NAME} at {DATA_ASSET_MANNY_PATH}")

    # Parts are clickable only through the mesh's Physics Asset (bone bodies).
    # Report it either way -- the mesh itself is never modified or re-saved.
    physics_asset = skel_mesh.get_editor_property("physics_asset")
    if physics_asset is None:
        log_warn(f"[CreatePortfolioAssets] {MANNY_MESH} has NO physics_asset: Manny parts will not be clickable (mesh left unchanged).")
    else:
        log(f"[CreatePortfolioAssets] {skel_mesh.get_name()} physics_asset = {physics_asset.get_path_name()}")

    profile.set_editor_property("display_name", unreal.Text("Manny (placeholder)"))
    profile.set_editor_property(
        "description",
        unreal.Text(
            "언리얼 엔진 3인칭 템플릿의 기본 마네킹(SKM_Manny_Simple)을 임시로 등록한 프로필입니다. "
            "실제 포트폴리오 캐릭터가 아니며, 실제 인체 비율·텍스처 2슬롯·Physics Asset을 가진 메시로 "
            "조명, 카메라 구도, 애니메이션, 파츠 선택 기능을 확인하기 위한 placeholder입니다. "
            "실제 캐릭터를 Import하면 새 CharacterProfileData를 만들어 Default Profile을 교체하세요."
        ),
    )
    profile.set_editor_property("skeletal_mesh", skel_mesh)

    # --- Camera: measured from the mesh bounds + reference-pose bones ---
    origin, extent = measure_skeletal_mesh_extent(skel_mesh)
    bottom_z = origin.z - extent.z
    top_z = origin.z + extent.z
    height = max(top_z - bottom_z, 1.0)
    skeleton = skel_mesh.get_editor_property("skeleton")
    bone_z = read_reference_bone_z(skeleton, ["head", "pelvis"]) if skeleton else {}
    head_z = bone_z.get("head", bottom_z + height * 0.9)
    pelvis_z = bone_z.get("pelvis", bottom_z + height * 0.53)
    log(f"[CreatePortfolioAssets] Manny framing inputs: bottom={bottom_z:.1f} top={top_z:.1f} head={head_z:.1f} pelvis={pelvis_z:.1f} (cm)")

    # Full Body: whole mesh height with a 5% frame margin top and bottom at 16:9.
    # The fit is for a flat card at the pivot; the feet/toes stand up to
    # `front_depth` cm closer to the camera (UE mannequins face +Y; the level
    # actor is yawed so +Y points at the camera) and project lower, so the
    # camera backs off by that depth (first calibration render: toes at 2%
    # margin without it).
    full_fov = 45.0
    full_target = bottom_z + height * 0.5
    front_depth = abs(origin.y) + extent.y
    full_distance = fit_camera_distance(height / 0.9 * 0.5, full_fov) + front_depth
    full_max = full_distance * 2.0
    full_framing = make_framing(full_target, full_distance, full_fov, full_distance * 0.45, full_max,
                                -80.0, max_pitch_above_floor(full_target, full_max))

    # Upper Body: pelvis to top of head, 5% margins.
    upper_fov = 40.0
    upper_target = (pelvis_z + top_z) * 0.5
    upper_distance = fit_camera_distance((top_z - pelvis_z) / 0.9 * 0.5, upper_fov)
    upper_max = upper_distance * 2.5
    upper_framing = make_framing(upper_target, upper_distance, upper_fov, upper_distance * 0.5, upper_max,
                                 -80.0, max_pitch_above_floor(upper_target, upper_max))

    # Face: centred just above the head bone (skull base; the idle pose drops
    # the head ~3 cm below the reference pose), half-height 1.5x the bone-to-top
    # span so the whole head plus chin/neck stay in frame (calibration render:
    # 0.4/1.2 put the head low and cut the chin).
    face_fov = 30.0
    head_span = max(top_z - head_z, 5.0)
    face_target = head_z + head_span * 0.15
    face_distance = fit_camera_distance(head_span * 1.5, face_fov)
    face_max = face_distance * 2.5
    face_framing = make_framing(face_target, face_distance, face_fov, face_distance * 0.5, face_max,
                                -70.0, max_pitch_above_floor(face_target, face_max))

    profile.set_editor_property("default_framing", full_framing)

    def make_preset(id_name, display, framing):
        preset = unreal.ViewerCameraPreset()
        preset.set_editor_property("id", id_name)
        preset.set_editor_property("display_name", unreal.Text(display))
        preset.set_editor_property("framing", framing)
        return preset

    profile.set_editor_property("camera_presets", [
        make_preset("Face", "Face", face_framing),
        make_preset("Upper", "Upper Body", upper_framing),
        make_preset("Full", "Full Body", full_framing),
    ])
    profile.set_editor_property("default_preset_id", "Full")

    # --- Animations (SK_Mannequin clips; Walk/Jog are force_root_lock, i.e. in place) ---
    idle_seq = load_or_none(MANNY_IDLE)
    walk_seq = load_or_none(MANNY_WALK)
    jog_seq = load_or_none(MANNY_JOG)

    def make_anim(id_name, display, sequence, loop, is_pose, pose_time):
        entry = unreal.ViewerAnimationEntry()
        entry.set_editor_property("id", id_name)
        entry.set_editor_property("display_name", unreal.Text(display))
        entry.set_editor_property("sequence", sequence)
        entry.set_editor_property("loop", loop)
        entry.set_editor_property("is_pose", is_pose)
        entry.set_editor_property("pose_time", pose_time)
        return entry

    profile.set_editor_property("animations", [
        make_anim("Idle", "Idle", idle_seq, True, False, 0.0),
        make_anim("Walk", "Walk", walk_seq, True, False, 0.0),
        make_anim("Jog", "Jog", jog_seq, True, False, 0.0),
        make_anim("Pose", "Pose", idle_seq, False, True, 0.5),
    ])
    # DefaultAnimClass deliberately left empty: the viewer plays DefaultAnimationId.
    profile.set_editor_property("default_animation_id", "Idle")

    # --- Expressions: Neutral only (SKM_Manny_Simple has 0 morph targets) ---
    neutral = unreal.ViewerExpression()
    neutral.set_editor_property("id", "Neutral")
    neutral.set_editor_property("display_name", unreal.Text("Neutral"))
    neutral.set_editor_property("morphs", [])
    profile.set_editor_property("expressions", [neutral])

    # --- Material variants: Default (no overrides) + Grid on every real slot ---
    grid_mat = load_or_none(GRID_MAT)
    slot_materials = {}  # slot name -> material interface
    grid_slots = []
    for index, slot in enumerate(skel_mesh.get_editor_property("materials") or []):
        slot_name = slot.get_editor_property("material_slot_name")
        slot_materials[str(slot_name)] = slot.get_editor_property("material_interface")
        override = unreal.ViewerMaterialSlotOverride()
        override.set_editor_property("slot_name", slot_name)
        override.set_editor_property("slot_index", index)
        override.set_editor_property("material", grid_mat)
        grid_slots.append(override)
    log(f"[CreatePortfolioAssets] {skel_mesh.get_name()} slots = {list(slot_materials)}")

    default_variant = unreal.ViewerMaterialVariant()
    default_variant.set_editor_property("id", "Default")
    default_variant.set_editor_property("display_name", unreal.Text("Default"))
    default_variant.set_editor_property("slots", [])
    grid_variant = unreal.ViewerMaterialVariant()
    grid_variant.set_editor_property("id", "Grid")
    grid_variant.set_editor_property("display_name", unreal.Text("Grid"))
    grid_variant.set_editor_property("slots", grid_slots)
    profile.set_editor_property("material_variants", [default_variant, grid_variant])

    profile.set_editor_property("turntable_speed_degrees_per_second", 20.0)

    # --- Parts: SK_Mannequin bones; every name is checked against BOTH the
    # mesh's own reference skeleton and the SK_Mannequin skeleton asset, and
    # dropped (with a log line) if missing from either. ---
    mesh_bones = read_mesh_bone_names(skel_mesh)
    skeleton_bones = set()
    try:
        skeleton_bones = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(unreal.AnimPoseExtensions.get_reference_pose(skeleton))}
    except Exception as exc:
        report_exception("reading SK_Mannequin bone names", exc)
    log(f"[CreatePortfolioAssets] bone counts: mesh={len(mesh_bones)} skeleton={len(skeleton_bones)}")

    try:
        registry_tris = int(unreal.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(MANNY_MESH).get_tag_value("Triangles"))
    except Exception:
        registry_tris = -1
    measured_total = sum(sum(v.values()) for v in MANNY_PART_TRIANGLES.values())
    if registry_tris != MANNY_LOD0_TRIANGLES or measured_total != MANNY_LOD0_TRIANGLES:
        log_warn(f"[CreatePortfolioAssets] Manny triangle table out of date: registry={registry_tris} "
                 f"table total={measured_total} expected={MANNY_LOD0_TRIANGLES} -- re-measure MANNY_PART_TRIANGLES.")

    texture_text = {name: describe_material_textures(mat) for name, mat in slot_materials.items()}

    def make_part(id_name, display, description, bone_names):
        valid = [b for b in bone_names if b in mesh_bones and b in skeleton_bones]
        dropped = [b for b in bone_names if b not in valid]
        if dropped:
            log_warn(f"[CreatePortfolioAssets] {DATA_ASSET_MANNY_NAME} part {id_name}: dropped bones not in mesh/skeleton: {dropped}")
        split = MANNY_PART_TRIANGLES.get(id_name, {})
        total = sum(split.values())
        # Slots that hold >= 5% of this part's triangles, largest first.
        major = [s for s, n in sorted(split.items(), key=lambda kv: -kv[1]) if total and n >= 0.05 * total]
        mat_text = " + ".join(
            f"{slot_materials[s].get_name() if slot_materials.get(s) else '?'} ({s} {split[s] * 100 // total}%)" for s in major
        )
        tex_text = " / ".join(f"{s}: {texture_text.get(s, '?')}" for s in major)
        part = unreal.ViewerPartInfo()
        part.set_editor_property("id", id_name)
        part.set_editor_property("display_name", unreal.Text(display))
        part.set_editor_property("part_type", unreal.Text("Body Part"))
        part.set_editor_property("description", unreal.Text(description))
        part.set_editor_property("bone_names", [unreal.Name(b) for b in valid])
        part.set_editor_property("component_tag", unreal.Name())
        part.set_editor_property("triangle_count", total)
        part.set_editor_property("material_name", unreal.Text(mat_text))
        part.set_editor_property("texture_resolution", unreal.Text(tex_text))
        return part

    profile.set_editor_property("parts", [
        make_part("Head", "Head", "Head and neck.", ["head", "neck_01", "neck_02"]),
        make_part("Torso", "Torso", "Pelvis, spine and clavicles.",
                  ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "pelvis", "clavicle_l", "clavicle_r"]),
        make_part("LeftArm", "Left Arm", "Left upper arm through hand.", ["upperarm_l", "lowerarm_l", "hand_l"]),
        make_part("RightArm", "Right Arm", "Right upper arm through hand.", ["upperarm_r", "lowerarm_r", "hand_r"]),
        make_part("LeftLeg", "Left Leg", "Left thigh through ball of foot.", ["thigh_l", "calf_l", "foot_l", "ball_l"]),
        make_part("RightLeg", "Right Leg", "Right thigh through ball of foot.", ["thigh_r", "calf_r", "foot_r", "ball_r"]),
    ])

    profile.set_editor_property("wireframe_material", load_or_none(WIREFRAME_MAT_PATH))

    save(DATA_ASSET_MANNY_PATH)
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


def create_or_update_gamemode_blueprint(default_profile, profile_library, wbp):
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
    # Since 2026-10-01 the default is DA_Character_Manny (see main()).
    cdo.set_editor_property("default_profile", default_profile)
    wbp_generated_class = wbp.generated_class() if wbp else None
    cdo.set_editor_property("viewer_widget_class", wbp_generated_class)
    # P1 completion evidence (Docs/CHARACTER_VIEWER_SETUP.md section 6): the
    # runtime CHARACTER UI section offers every entry here (default profile
    # first) via ACharacterViewerController::SelectCharacterProfile() -- no
    # C++/Blueprint change needed to add DA_Character_Cube as a second choice.
    cdo.set_editor_property("profile_library", [p for p in profile_library if p is not None])

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        report_exception("compile BP_CharacterViewerGameMode (post)", exc)

    save(BP_ASSET_PATH)
    return bp


# ---------------------------------------------------------------------------
# 4. LV_Portfolio (level)
# ---------------------------------------------------------------------------

# Studio look (2026-10-01). Light intensities are lux at the fixed exposure
# set by the StudioPostProcess volume below (Manual, physical-camera exposure
# off, bias STUDIO_EXPOSURE_BIAS => scene exposure scale 2^bias), tuned so a
# 0.18 albedo floor reads mid-grey and the light-grey mannequin does not clip.
# Rotations: keyword Rotator (positional order is roll, pitch, yaw!). The
# viewer camera looks along +X at yaw 0; the character faces -X (the camera).
STUDIO_LIGHTS = [
    # Key: camera front-left, 40 deg down, warm, the only shadow caster and
    # the only atmosphere sun.
    dict(label="KeyLight", location=(0.0, 0.0, 400.0), pitch=-40.0, yaw=30.0, intensity=2.7,
         color=(255, 244, 229), cast_shadows=True, atmosphere_sun=True, sun_index=0, forward_priority=1),
    # Fill: camera front-right, low, cool, no shadow.
    dict(label="FillLight", location=(0.0, 0.0, 450.0), pitch=-15.0, yaw=-50.0, intensity=0.9,
         color=(222, 232, 255), cast_shadows=False, atmosphere_sun=False, sun_index=1, forward_priority=0),
    # Rim: behind the character on the camera-right side, separates the
    # silhouette from the dark backdrop, no shadow.
    dict(label="RimLight", location=(0.0, 0.0, 500.0), pitch=-35.0, yaw=-150.0, intensity=1.8,
         color=(255, 255, 255), cast_shadows=False, atmosphere_sun=False, sun_index=1, forward_priority=0),
]
STUDIO_SKYLIGHT_INTENSITY = 0.36
STUDIO_EXPOSURE_BIAS = 0.0
STUDIO_BLOOM_INTENSITY = 0.15
STUDIO_VIGNETTE_INTENSITY = 0.2
# Engine sphere is 100 cm across: scale 50 => 2500 cm radius, centred on the
# character so the camera (MaxDistance <= ~900 cm) is always inside it.
STUDIO_BACKDROP_SCALE = 50.0
# Engine cylinder is 100 cm across / 100 cm tall, pivot at its centre: scale
# 51 x 0.2 => 2550 cm radius, 20 cm thick, top face at Z = 0. The rim lies
# just OUTSIDE the backdrop sphere, so the platform edge is never visible;
# M_StudioFloor additionally fades the far floor into the backdrop.
STUDIO_PLATFORM_LOCATION = (0.0, 0.0, -10.0)
STUDIO_PLATFORM_SCALE = (51.0, 51.0, 0.2)


def _no_collision(mesh_comp):
    """Backdrop/floor must never intercept the Inspection line trace
    (ECC_Visibility) or anything else: NoCollision profile."""
    mesh_comp.set_collision_profile_name("NoCollision")
    mesh_comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def apply_studio_setup(actor_subsystem):
    """Adds or updates (by actor label) the studio actors of the currently
    open level: KeyLight/FillLight/RimLight (Directional, Movable),
    AmbientSkyLight intensity, PlatformCylinder (MI_StudioFloor, NoCollision),
    StudioBackdrop (inverted-view engine sphere, MI_StudioBackdrop, unlit,
    no shadow, NoCollision) and StudioPostProcess (unbound PostProcessVolume:
    manual exposure, low bloom, light vignette). Never touches the
    PortfolioCharacterActor, World Settings, or saves anything -- the caller
    decides that. Called by create_or_update_level() for a NEW level only;
    an existing LV_Portfolio is only ever changed by a deliberate one-off
    edit (Docs/CHARACTER_VIEWER_SETUP.md 6.13), never by this script's
    normal run. Returns a list of human-readable change lines."""
    changes = []
    actors = actor_subsystem.get_all_level_actors()
    by_label = {}
    for actor in actors:
        by_label.setdefault(actor.get_actor_label(), actor)

    def get_or_spawn(label, actor_class, location, rotation):
        actor = by_label.get(label)
        if actor is not None and not isinstance(actor, actor_class):
            raise RuntimeError(f"actor '{label}' exists but is a {actor.get_class().get_name()}, expected {actor_class.__name__}")
        if actor is None:
            actor = actor_subsystem.spawn_actor_from_class(actor_class, unreal.Vector(*location), rotation)
            actor.set_actor_label(label)
            by_label[label] = actor
            changes.append(f"spawned {label} ({actor_class.__name__})")
        return actor

    for spec in STUDIO_LIGHTS:
        rotation = unreal.Rotator(roll=0.0, pitch=spec["pitch"], yaw=spec["yaw"])
        light = get_or_spawn(spec["label"], unreal.DirectionalLight, spec["location"], rotation)
        light.set_actor_location(unreal.Vector(*spec["location"]), False, False)
        light.set_actor_rotation(rotation, False)
        comp = light.get_component_by_class(unreal.DirectionalLightComponent)
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("intensity", spec["intensity"])
        r, g, b = spec["color"]
        comp.set_editor_property("light_color", unreal.Color(r=r, g=g, b=b, a=255))
        comp.set_editor_property("cast_shadows", spec["cast_shadows"])
        comp.set_editor_property("atmosphere_sun_light", spec["atmosphere_sun"])
        comp.set_editor_property("atmosphere_sun_light_index", spec["sun_index"])
        comp.set_editor_property("forward_shading_priority", spec["forward_priority"])
        changes.append(f"{spec['label']}: pitch {spec['pitch']} yaw {spec['yaw']} {spec['intensity']} lux color {spec['color']} "
                       f"shadows={spec['cast_shadows']} sun={spec['atmosphere_sun']}")

    sky = get_or_spawn("AmbientSkyLight", unreal.SkyLight, (0.0, 0.0, 500.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    if sky_comp.get_editor_property("cubemap") is None:
        cubemap = load_or_none(AMBIENT_CUBEMAP)
        if cubemap:
            sky_comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
            sky_comp.set_editor_property("cubemap", cubemap)
    sky_comp.set_editor_property("intensity", STUDIO_SKYLIGHT_INTENSITY)
    sky_comp.recapture_sky()
    changes.append(f"AmbientSkyLight: intensity {STUDIO_SKYLIGHT_INTENSITY}")

    floor_mi = load_or_none(STUDIO_FLOOR_MI_PATH)
    platform = get_or_spawn("PlatformCylinder", unreal.StaticMeshActor, STUDIO_PLATFORM_LOCATION, unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    platform.set_actor_location(unreal.Vector(*STUDIO_PLATFORM_LOCATION), False, False)
    platform_comp = platform.get_component_by_class(unreal.StaticMeshComponent)
    platform_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    platform_comp.set_static_mesh(load_or_none(CYLINDER_MESH))
    platform_comp.set_world_scale3d(unreal.Vector(*STUDIO_PLATFORM_SCALE))
    if floor_mi:
        platform_comp.set_material(0, floor_mi)
    _no_collision(platform_comp)
    changes.append(f"PlatformCylinder: scale {STUDIO_PLATFORM_SCALE} at {STUDIO_PLATFORM_LOCATION}, material {STUDIO_FLOOR_MI_NAME}, NoCollision")

    backdrop_mi = load_or_none(STUDIO_BACKDROP_MI_PATH)
    backdrop = get_or_spawn("StudioBackdrop", unreal.StaticMeshActor, (0.0, 0.0, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    backdrop.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
    backdrop_comp = backdrop.get_component_by_class(unreal.StaticMeshComponent)
    backdrop_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    backdrop_comp.set_static_mesh(load_or_none(SPHERE_MESH))
    backdrop_comp.set_world_scale3d(unreal.Vector(STUDIO_BACKDROP_SCALE, STUDIO_BACKDROP_SCALE, STUDIO_BACKDROP_SCALE))
    if backdrop_mi:
        backdrop_comp.set_material(0, backdrop_mi)
    # A closed sphere that casts shadows would put the whole scene in shadow.
    backdrop_comp.set_cast_shadow(False)
    _no_collision(backdrop_comp)
    changes.append(f"StudioBackdrop: {SPHERE_MESH} scale {STUDIO_BACKDROP_SCALE}, material {STUDIO_BACKDROP_MI_NAME}, no shadow, NoCollision")

    ppv = get_or_spawn("StudioPostProcess", unreal.PostProcessVolume, (0.0, 0.0, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    ppv.set_editor_property("unbound", True)
    settings = ppv.get_editor_property("settings")
    for key, value in (
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("auto_exposure_apply_physical_camera_exposure", False),
        ("auto_exposure_bias", STUDIO_EXPOSURE_BIAS),
        ("bloom_intensity", STUDIO_BLOOM_INTENSITY),
        ("vignette_intensity", STUDIO_VIGNETTE_INTENSITY),
    ):
        settings.set_editor_property(f"override_{key}", True)
        settings.set_editor_property(key, value)
    ppv.set_editor_property("settings", settings)
    changes.append(f"StudioPostProcess: unbound, AutoExposure Manual (physical camera off) bias {STUDIO_EXPOSURE_BIAS}, "
                   f"bloom {STUDIO_BLOOM_INTENSITY}, vignette {STUDIO_VIGNETTE_INTENSITY}")
    return changes

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

    # --- Studio look (2026-10-01): re-tunes the lights/platform spawned above
    # by label and adds RimLight, StudioBackdrop and StudioPostProcess -- the
    # same function the one-off in-place edit of the existing LV_Portfolio used,
    # so a recreated level matches the committed one. ---
    try:
        for line in apply_studio_setup(actor_subsystem):
            log(f"[CreatePortfolioAssets] studio: {line}")
    except Exception as exc:
        report_exception("apply_studio_setup (new LV_Portfolio)", exc)

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
    backdrop_mat = create_or_update_studio_backdrop_material()
    create_or_update_studio_material_instance(
        STUDIO_BACKDROP_MI_NAME, STUDIO_BACKDROP_MI_PATH, backdrop_mat, STUDIO_BACKDROP_MAT_PATH,
        STUDIO_BACKDROP_VECTORS, STUDIO_BACKDROP_SCALARS)
    floor_mat = create_or_update_studio_floor_material()
    create_or_update_studio_material_instance(
        STUDIO_FLOOR_MI_NAME, STUDIO_FLOOR_MI_PATH, floor_mat, STUDIO_FLOOR_MAT_PATH,
        STUDIO_FLOOR_VECTORS, STUDIO_FLOOR_SCALARS)
    create_or_update_part_highlight_material()
    create_or_update_wireframe_overlay_material()
    profile = create_or_update_character_profile()
    cube_profile = create_or_update_character_profile_cube()
    manny_profile = create_or_update_character_profile_manny()
    wbp = create_or_update_widget_blueprint()
    # Since 2026-10-01 a newly created GameMode/level default to Manny; an
    # existing GameMode/level is only validated (never changed) here.
    default_profile = manny_profile or profile
    gamemode_bp = create_or_update_gamemode_blueprint(default_profile, [manny_profile, profile, cube_profile], wbp)
    create_or_update_level(default_profile, gamemode_bp)
    update_default_engine_ini()

    if errors:
        log_err(f"[CreatePortfolioAssets] ==== DONE WITH {len(errors)} ERROR(S) ====")
        for e in errors:
            log_err(e)
        sys.exit(1)
    else:
        log("[CreatePortfolioAssets] ==== DONE, NO ERRORS ====")


main()
