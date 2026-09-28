"""
Creates/updates the Editor-only assets needed by section 13.6 of
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

Idempotent: safe to re-run. Existing assets are loaded and their properties
reset/overwritten deterministically (not duplicated), so re-running after a
profile/lighting/framing tweak in this script is the intended workflow.

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
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    if not ok:
        log_warn(f"[CreatePortfolioAssets] save_asset returned False for '{path}'.")
    return ok


# ---------------------------------------------------------------------------
# 1. DA_Character (CharacterProfileData data asset)
# ---------------------------------------------------------------------------

def create_or_update_character_profile():
    ensure_directory(DATA_PACKAGE)

    data_asset_class = unreal.CharacterProfileData

    if EAL.does_asset_exist(DATA_ASSET_PATH):
        profile = EAL.load_asset(DATA_ASSET_PATH)
        log(f"[CreatePortfolioAssets] DA_Character already exists, updating in place: {DATA_ASSET_PATH}")
    else:
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
# 1b. DA_Character_Cube (second CharacterProfileData, P1 completion evidence:
#     a code-free runtime profile switch needs a second profile to switch to)
# ---------------------------------------------------------------------------

def create_or_update_character_profile_cube():
    ensure_directory(DATA_PACKAGE)

    data_asset_class = unreal.CharacterProfileData

    if EAL.does_asset_exist(DATA_ASSET_CUBE_PATH):
        profile = EAL.load_asset(DATA_ASSET_CUBE_PATH)
        log(f"[CreatePortfolioAssets] DA_Character_Cube already exists, updating in place: {DATA_ASSET_CUBE_PATH}")
    else:
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

    save(DATA_ASSET_CUBE_PATH)
    return profile


# ---------------------------------------------------------------------------
# 2. WBP_CharacterViewer (Widget Blueprint, parent = UCharacterViewerWidget)
#    Left with an empty designer tree on purpose: the C++ fallback panel
#    (CharacterViewerWidget::NativeConstruct) builds the UI when
#    WidgetTree->RootWidget is null.
# ---------------------------------------------------------------------------

def create_or_update_widget_blueprint():
    ensure_directory(WBP_PACKAGE)

    if EAL.does_asset_exist(WBP_ASSET_PATH):
        wbp = EAL.load_asset(WBP_ASSET_PATH)
        log(f"[CreatePortfolioAssets] WBP_CharacterViewer already exists: {WBP_ASSET_PATH}")
    else:
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

def create_or_update_gamemode_blueprint(profile, cube_profile, wbp):
    ensure_directory(BP_PACKAGE)

    if EAL.does_asset_exist(BP_ASSET_PATH):
        bp = EAL.load_asset(BP_ASSET_PATH)
        log(f"[CreatePortfolioAssets] BP_CharacterViewerGameMode already exists: {BP_ASSET_PATH}")
    else:
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

def create_or_update_level(profile, gamemode_bp):
    ensure_directory(MAP_PACKAGE)

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if EAL.does_asset_exist(MAP_ASSET_PATH):
        # Re-running this script used to load the existing level and destroy
        # only the actors this script itself placed, but EditorActorSubsystem
        # actor destruction inside a single headless Python tick is not
        # guaranteed to be visible to the SAME session's TActorIterator before
        # the level is saved, which produced two overlapping
        # APortfolioCharacterActor instances in LV_Portfolio (caught by the
        # -game smoke test: "Exactly one APortfolioCharacterActor exists" was
        # 2, see Docs/CHARACTER_VIEWER_SETUP.md section 13.7). Deleting and
        # recreating the level asset from scratch every run sidesteps that
        # whole class of leftover/duplicate-actor bugs and keeps the level
        # fully deterministic.
        log(f"[CreatePortfolioAssets] LV_Portfolio already exists, deleting and recreating it: {MAP_ASSET_PATH}")
        level_subsystem.new_level("/Temp/CreatePortfolioAssets_Scratch")
        EAL.delete_asset(MAP_ASSET_PATH)

    log(f"[CreatePortfolioAssets] Creating new level at {MAP_ASSET_PATH}")
    level_subsystem.new_level(MAP_ASSET_PATH)

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

    level_subsystem.save_current_level()


# ---------------------------------------------------------------------------
# 5. Config/DefaultEngine.ini: GlobalDefaultGameMode -> BP_CharacterViewerGameMode_C
# ---------------------------------------------------------------------------

def update_default_engine_ini():
    import os

    project_dir = unreal.Paths.project_dir()
    ini_path = os.path.normpath(os.path.join(unreal.Paths.convert_relative_path_to_full(project_dir), "Config", "DefaultEngine.ini"))
    if not os.path.exists(ini_path):
        log_warn(f"[CreatePortfolioAssets] {ini_path} does not exist, skipping ini update.")
        return

    with open(ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    old_line = "GlobalDefaultGameMode=/Script/CharacterShowcase.CharacterViewerGameMode"
    new_line = f"GlobalDefaultGameMode={BP_ASSET_PATH}.{BP_ASSET_NAME}_C"

    if new_line in content:
        log(f"[CreatePortfolioAssets] {ini_path} already has the Blueprint GlobalDefaultGameMode.")
        return

    if old_line in content:
        content = content.replace(old_line, new_line)
    else:
        log_warn("[CreatePortfolioAssets] Expected GlobalDefaultGameMode line not found verbatim; appending explicit override instead.")
        content += f"\n[/Script/EngineSettings.GameMapsSettings]\n{new_line}\n"

    with open(ini_path, "w", encoding="utf-8") as f:
        f.write(content)
    log(f"[CreatePortfolioAssets] Updated {ini_path}: GlobalDefaultGameMode -> {new_line}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    log("[CreatePortfolioAssets] ==== START ====")
    profile = create_or_update_character_profile()
    cube_profile = create_or_update_character_profile_cube()
    wbp = create_or_update_widget_blueprint()
    gamemode_bp = create_or_update_gamemode_blueprint(profile, cube_profile, wbp)
    create_or_update_level(profile, gamemode_bp)
    update_default_engine_ini()

    if errors:
        log_err(f"[CreatePortfolioAssets] ==== DONE WITH {len(errors)} ERROR(S) ====")
        for e in errors:
            log_err(e)
    else:
        log("[CreatePortfolioAssets] ==== DONE, NO ERRORS ====")


main()
