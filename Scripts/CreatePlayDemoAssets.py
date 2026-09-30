"""
Creates the D1 play-demo assets (Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md section 12):

  /Game/PlayDemo/Data/DA_PlayCharacter_Manny       CharacterProfileData (placeholder Manny)
  /Game/PlayDemo/Blueprints/BP_DemoCharacter       Blueprint, parent ADemoCharacter
  /Game/PlayDemo/Blueprints/BP_DemoGameMode        Blueprint, parent ADemoGameMode
  /Game/PlayDemo/Maps/LV_PlayDemo                  small flat play level

Run headlessly, e.g.:
    "C:\\Program Files\\Epic Games\\UE_5.6\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" ^
        "<project>\\CharacterShowcase.uproject" ^
        -ExecutePythonScript="<project>\\Scripts\\CreatePlayDemoAssets.py" ^
        -unattended -nosplash -nop4 -log

Same conventions as Scripts/CreatePortfolioAssets.py: CREATE-MISSING-ONLY,
PRESERVE-EXISTING. An existing asset is loaded read-only, validated against a
minimal expected shape and reported as
    [keep] <path> OK
    [keep] <path> DIFFERS: <what>
and is never modified or re-saved. To regenerate an asset, delete that one
asset in the Editor and re-run. This script never touches Content/Portfolio
and never writes Config/*.ini (the viewer stays the project's default map).

Placeholder content only: the mannequin subset copied in by D0 under
/Game/Characters/Mannequins (SKM_Manny_Simple + ABP_Unarmed).
"""

import gc
import sys

import unreal

# ---------------------------------------------------------------------------
# Paths / constants
# ---------------------------------------------------------------------------

DATA_PACKAGE = "/Game/PlayDemo/Data"
DATA_NAME = "DA_PlayCharacter_Manny"
DATA_PATH = f"{DATA_PACKAGE}/{DATA_NAME}"

BP_PACKAGE = "/Game/PlayDemo/Blueprints"
CHAR_BP_NAME = "BP_DemoCharacter"
CHAR_BP_PATH = f"{BP_PACKAGE}/{CHAR_BP_NAME}"
GM_BP_NAME = "BP_DemoGameMode"
GM_BP_PATH = f"{BP_PACKAGE}/{GM_BP_NAME}"

MAP_PACKAGE = "/Game/PlayDemo/Maps"
MAP_NAME = "LV_PlayDemo"
MAP_PATH = f"{MAP_PACKAGE}/{MAP_NAME}"

MANNY_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
MANNY_ABP = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"

CUBE_MESH = "/Engine/BasicShapes/Cube.Cube"
GRID_MAT = "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"
AMBIENT_CUBEMAP = "/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"

# Level layout (cm). Floor top surface is Z = 0.
FLOOR_HALF = 1500.0      # 30 m x 30 m floor
WALL_THICKNESS = 50.0
WALL_HEIGHT = 200.0

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

errors = []
log = unreal.log
log_warn = unreal.log_warning
log_err = unreal.log_error


def report_exception(context, exc):
    msg = f"[CreatePlayDemoAssets] FAILED at: {context}: {exc!r}"
    log_err(msg)
    errors.append(msg)


def ensure_directory(package_path):
    if not EAL.does_directory_exist(package_path):
        EAL.make_directory(package_path)


def load_or_none(path):
    obj = unreal.load_asset(path) if path else None
    if obj is None:
        log_warn(f"[CreatePlayDemoAssets] Could not load '{path}'.")
    return obj


def save(path):
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    if not ok:
        msg = f"[CreatePlayDemoAssets] FAILED: save_asset returned False for '{path}'."
        log_err(msg)
        errors.append(msg)
    return ok


def report_keep(path, diffs):
    if diffs:
        log(f"[CreatePlayDemoAssets] [keep] {path} DIFFERS: {'; '.join(diffs)}")
    else:
        log(f"[CreatePlayDemoAssets] [keep] {path} OK")


# ---------------------------------------------------------------------------
# 1. DA_PlayCharacter_Manny
# ---------------------------------------------------------------------------

def validate_profile(profile, path):
    diffs = []
    try:
        if profile.get_editor_property("skeletal_mesh") is None:
            diffs.append("skeletal_mesh is not set")
        if profile.get_editor_property("default_anim_class") is None:
            diffs.append("default_anim_class is not set")
        if profile.get_editor_property("walk_speed") <= 0.0:
            diffs.append("walk_speed <= 0")
        if profile.get_editor_property("run_speed") < profile.get_editor_property("walk_speed"):
            diffs.append("run_speed < walk_speed")
    except Exception as exc:
        diffs.append(f"could not validate ({exc!r})")
    report_keep(path, diffs)


def create_profile():
    if EAL.does_asset_exist(DATA_PATH):
        profile = EAL.load_asset(DATA_PATH)
        log(f"[CreatePlayDemoAssets] {DATA_NAME} already exists, preserving (read-only): {DATA_PATH}")
        validate_profile(profile, DATA_PATH)
        return profile

    ensure_directory(DATA_PACKAGE)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CharacterProfileData)
    profile = asset_tools.create_asset(DATA_NAME, DATA_PACKAGE, unreal.CharacterProfileData, factory)
    if profile is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {DATA_NAME}")
    log(f"[CreatePlayDemoAssets] Created {DATA_NAME} at {DATA_PATH}")

    mesh = load_or_none(MANNY_MESH)
    abp_class = EAL.load_blueprint_class(MANNY_ABP)
    if abp_class is None:
        log_warn(f"[CreatePlayDemoAssets] Could not load blueprint class '{MANNY_ABP}'.")

    profile.set_editor_property("display_name", unreal.Text("Manny (placeholder)"))
    profile.set_editor_property(
        "description",
        unreal.Text(
            "Engine template mannequin used as a placeholder play character (D1 technical "
            "verification only, not the artist's character). Replace SkeletalMesh, "
            "DefaultAnimClass and the speeds with the real character's."
        ),
    )
    profile.set_editor_property("skeletal_mesh", mesh)
    profile.set_editor_property("default_anim_class", abp_class)
    profile.set_editor_property("walk_speed", 300.0)
    profile.set_editor_property("run_speed", 600.0)

    save(DATA_PATH)
    return profile


# ---------------------------------------------------------------------------
# 2. Blueprints
# ---------------------------------------------------------------------------

def create_blueprint(name, path, parent_class):
    ensure_directory(BP_PACKAGE)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = asset_tools.create_asset(name, BP_PACKAGE, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError(f"asset_tools.create_asset returned None for {name}")
    log(f"[CreatePlayDemoAssets] Created {name} at {path}")

    if bp.generated_class() is None:
        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        except Exception as exc:
            report_exception(f"compile {name} (pre)", exc)
    if bp.generated_class() is None:
        raise RuntimeError(f"{name} has no generated_class() even after compiling")
    return bp


def validate_character_blueprint(bp, path):
    diffs = []
    try:
        generated_class = bp.generated_class()
        if generated_class is None:
            diffs.append("generated_class() is None (blueprint not compiled)")
        else:
            cdo = unreal.get_default_object(generated_class)
            if not isinstance(cdo, unreal.DemoCharacter):
                diffs.append(f"parent class is not DemoCharacter (CDO class = {cdo.get_class().get_name()})")
    except Exception as exc:
        diffs.append(f"could not validate ({exc!r})")
    report_keep(path, diffs)


def create_character_blueprint():
    if EAL.does_asset_exist(CHAR_BP_PATH):
        bp = EAL.load_asset(CHAR_BP_PATH)
        log(f"[CreatePlayDemoAssets] {CHAR_BP_NAME} already exists, preserving (read-only): {CHAR_BP_PATH}")
        validate_character_blueprint(bp, CHAR_BP_PATH)
        return bp

    bp = create_blueprint(CHAR_BP_NAME, CHAR_BP_PATH, unreal.DemoCharacter)
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        report_exception(f"compile {CHAR_BP_NAME} (post)", exc)
    save(CHAR_BP_PATH)
    return bp


def class_path_or_none(cls):
    return cls.get_path_name() if cls is not None else None


def validate_gamemode_blueprint(bp, path, character_bp):
    diffs = []
    try:
        generated_class = bp.generated_class()
        if generated_class is None:
            diffs.append("generated_class() is None (blueprint not compiled)")
        else:
            cdo = unreal.get_default_object(generated_class)
            if not isinstance(cdo, unreal.DemoGameMode):
                diffs.append(f"parent class is not DemoGameMode (CDO class = {cdo.get_class().get_name()})")
            if cdo.get_editor_property("demo_profile") is None:
                diffs.append("demo_profile is not set")
            pawn_class = cdo.get_editor_property("default_pawn_class")
            expected_pawn = character_bp.generated_class() if character_bp is not None else None
            if pawn_class is None:
                diffs.append("default_pawn_class is not set")
            elif expected_pawn is None:
                diffs.append(f"default_pawn_class is {class_path_or_none(pawn_class)} but {CHAR_BP_NAME} has no generated class to compare")
            elif class_path_or_none(pawn_class) != class_path_or_none(expected_pawn):
                diffs.append(f"default_pawn_class is {class_path_or_none(pawn_class)}, expected {class_path_or_none(expected_pawn)}")
    except Exception as exc:
        diffs.append(f"could not validate ({exc!r})")
    report_keep(path, diffs)


def create_gamemode_blueprint(profile, character_bp):
    if EAL.does_asset_exist(GM_BP_PATH):
        bp = EAL.load_asset(GM_BP_PATH)
        log(f"[CreatePlayDemoAssets] {GM_BP_NAME} already exists, preserving (read-only): {GM_BP_PATH}")
        validate_gamemode_blueprint(bp, GM_BP_PATH, character_bp)
        return bp

    bp = create_blueprint(GM_BP_NAME, GM_BP_PATH, unreal.DemoGameMode)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("demo_profile", profile)
    if character_bp is not None and character_bp.generated_class() is not None:
        cdo.set_editor_property("default_pawn_class", character_bp.generated_class())
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    except Exception as exc:
        report_exception(f"compile {GM_BP_NAME} (post)", exc)
    save(GM_BP_PATH)
    return bp


# ---------------------------------------------------------------------------
# 3. LV_PlayDemo
# ---------------------------------------------------------------------------

def validate_level(path, gamemode_bp):
    """Read-only shape check: a PlayerStart, >=5 static mesh actors (floor +
    4 walls), at least one light, World Settings DefaultGameMode ==
    BP_DemoGameMode's generated class. Same
    load_level() caveats as CreatePortfolioAssets.validate_level(): if the
    currently open level is dirty in an interactive session, this is skipped."""
    diffs = []
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal_editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

    current_world = unreal_editor_subsystem.get_editor_world() if unreal_editor_subsystem else None
    current_package = current_world.get_outer() if current_world else None
    current_package_name = current_package.get_name() if current_package else None

    # Drop every Python reference to the currently open world/package before
    # load_level(): a lingering reference (e.g. the startup map LV_Portfolio)
    # makes UE fail the old-world cleanup with "World Memory Leaks" (fatal).
    current_world = None
    current_package = None
    gc.collect()

    if current_package_name != path:
        if not unreal.SystemLibrary.is_unattended():
            try:
                dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or []
            except Exception as exc:
                dirty = []
                report_exception("get_dirty_map_packages()", exc)
            if current_package_name in {p.get_name() for p in dirty if p}:
                log(f"[CreatePlayDemoAssets] [keep] {path} not validated (another unsaved level is open)")
                return
        if not level_subsystem.load_level(path):
            report_keep(path, ["LevelEditorSubsystem.load_level() returned False; could not validate contents"])
            return

    try:
        actors = actor_subsystem.get_all_level_actors()
        if not any(isinstance(a, unreal.PlayerStart) for a in actors):
            diffs.append("no PlayerStart")
        mesh_actors = [a for a in actors if isinstance(a, unreal.StaticMeshActor)]
        if len(mesh_actors) < 5:
            diffs.append(f"expected >= 5 StaticMeshActors (floor + 4 walls), found {len(mesh_actors)}")
        light_types = (unreal.DirectionalLight, unreal.PointLight, unreal.SpotLight, unreal.RectLight, unreal.SkyLight)
        if not any(isinstance(a, light_types) for a in actors):
            diffs.append("no light actors found")
        world = unreal_editor_subsystem.get_editor_world()
        game_mode = world.get_world_settings().get_editor_property("default_game_mode")
        expected_game_mode = gamemode_bp.generated_class() if gamemode_bp is not None else None
        if game_mode is None:
            diffs.append("World Settings DefaultGameMode is not set")
        elif expected_game_mode is None:
            diffs.append(f"World Settings DefaultGameMode is {class_path_or_none(game_mode)} but {GM_BP_NAME} has no generated class to compare")
        elif class_path_or_none(game_mode) != class_path_or_none(expected_game_mode):
            diffs.append(f"World Settings DefaultGameMode is {class_path_or_none(game_mode)}, expected {class_path_or_none(expected_game_mode)}")
    except Exception as exc:
        diffs.append(f"could not validate level contents ({exc!r})")

    report_keep(path, diffs)


def spawn_box(actor_subsystem, label, location, scale, cube_mesh, material=None):
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    )
    actor.set_actor_label(label)
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    if comp:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        if cube_mesh:
            comp.set_static_mesh(cube_mesh)
        comp.set_world_scale3d(unreal.Vector(*scale))
        if material:
            comp.set_material(0, material)
    return actor


def create_level(gamemode_bp):
    ensure_directory(MAP_PACKAGE)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if EAL.does_asset_exist(MAP_PATH):
        log(f"[CreatePlayDemoAssets] {MAP_NAME} already exists, preserving (read-only): {MAP_PATH}")
        validate_level(MAP_PATH, gamemode_bp)
        return True

    log(f"[CreatePlayDemoAssets] Creating new level at {MAP_PATH}")
    if not level_subsystem.new_level(MAP_PATH):
        msg = f"[CreatePlayDemoAssets] FAILED: new_level() returned False for {MAP_PATH}"
        log_err(msg)
        errors.append(msg)
        return False

    cube = load_or_none(CUBE_MESH)
    grid = load_or_none(GRID_MAT)

    # --- Floor: 30 m x 30 m, top surface at Z = 0 (Cube is 100 uu) ---
    spawn_box(actor_subsystem, "Floor", (0.0, 0.0, -10.0), (FLOOR_HALF * 2 / 100.0, FLOOR_HALF * 2 / 100.0, 0.2), cube, grid)

    # --- Edge walls (2 m high). Inner faces at +-(FLOOR_HALF - WALL_THICKNESS/2). ---
    t = WALL_THICKNESS / 100.0
    h = WALL_HEIGHT / 100.0
    span = (FLOOR_HALF * 2 + WALL_THICKNESS) / 100.0
    z = WALL_HEIGHT / 2.0
    spawn_box(actor_subsystem, "Wall_PosX", (FLOOR_HALF, 0.0, z), (t, span, h), cube)
    spawn_box(actor_subsystem, "Wall_NegX", (-FLOOR_HALF, 0.0, z), (t, span, h), cube)
    spawn_box(actor_subsystem, "Wall_PosY", (0.0, FLOOR_HALF, z), (span, t, h), cube)
    spawn_box(actor_subsystem, "Wall_NegY", (0.0, -FLOOR_HALF, z), (span, t, h), cube)

    # --- Obstacles (kept off the +X axis through the origin used by the smoke test) ---
    spawn_box(actor_subsystem, "Obstacle_A", (600.0, -450.0, 50.0), (2.0, 2.0, 1.0), cube, grid)
    spawn_box(actor_subsystem, "Obstacle_B", (-500.0, 500.0, 75.0), (1.5, 1.5, 1.5), cube, grid)
    spawn_box(actor_subsystem, "Obstacle_C", (900.0, 650.0, 100.0), (1.0, 3.0, 2.0), cube, grid)

    # --- PlayerStart at origin (+Z 100), facing +X ---
    start = actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    )
    start.set_actor_label("PlayerStart")

    # --- Lights: same values as LV_Portfolio (exposure is fixed project-wide) ---
    # unreal.Rotator positional order is (roll, pitch, yaw): always use keywords.
    key_light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 400.0), unreal.Rotator(roll=0.0, pitch=-40.0, yaw=30.0)
    )
    key_light.set_actor_label("KeyLight")
    key_comp = key_light.get_component_by_class(unreal.DirectionalLightComponent)
    if key_comp:
        key_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        key_comp.set_editor_property("intensity", 7.0)
        try:
            key_comp.set_editor_property("forward_shading_priority", 1)
        except Exception as exc:
            report_exception("KeyLight forward_shading_priority", exc)

    fill_light = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 450.0), unreal.Rotator(roll=0.0, pitch=-15.0, yaw=-50.0)
    )
    fill_light.set_actor_label("FillLight")
    fill_comp = fill_light.get_component_by_class(unreal.DirectionalLightComponent)
    if fill_comp:
        fill_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        fill_comp.set_editor_property("intensity", 2.0)
        fill_comp.set_editor_property("cast_shadows", False)
        fill_comp.set_editor_property("atmosphere_sun_light_index", 1)

    sky_light = actor_subsystem.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)
    )
    sky_light.set_actor_label("AmbientSkyLight")
    sky_comp = sky_light.get_component_by_class(unreal.SkyLightComponent)
    if sky_comp:
        sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky_comp.set_editor_property("intensity", 1.0)
        try:
            cubemap = load_or_none(AMBIENT_CUBEMAP)
            if cubemap:
                sky_comp.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
                sky_comp.set_editor_property("cubemap", cubemap)
            sky_comp.recapture_sky()
        except Exception as exc:
            report_exception("SkyLight cubemap", exc)

    # --- World Settings: GameMode override ---
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None:
        world = level_subsystem.get_current_level().get_outer()
    generated_class = gamemode_bp.generated_class() if gamemode_bp else None
    world.get_world_settings().set_editor_property("default_game_mode", generated_class)

    if not level_subsystem.save_current_level():
        msg = f"[CreatePlayDemoAssets] FAILED: save_current_level() returned False for {MAP_PATH}"
        log_err(msg)
        errors.append(msg)
        return False
    return True


# ---------------------------------------------------------------------------

def main():
    log("[CreatePlayDemoAssets] ==== START ====")
    try:
        profile = create_profile()
        character_bp = create_character_blueprint()
        gamemode_bp = create_gamemode_blueprint(profile, character_bp)
        create_level(gamemode_bp)
    except Exception as exc:
        report_exception("main", exc)

    if errors:
        log_err(f"[CreatePlayDemoAssets] ==== DONE WITH {len(errors)} ERROR(S) ====")
        for e in errors:
            log_err(e)
        sys.exit(1)
    else:
        log("[CreatePlayDemoAssets] ==== DONE, NO ERRORS ====")


main()
