"""D0: read-only check of the placeholder mannequin assets under /Game/Characters/Mannequins.
Does not save or modify anything. Run headless:
  UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript="<abs>\\Scripts\\D0_CheckMannequinAssets.py" -unattended -nosplash -nop4 -NullRHI -log
"""
import unreal

ROOT = "/Game/Characters/Mannequins"
TAG = "[D0]"


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def warn(msg):
    unreal.log_warning("%s %s" % (TAG, msg))


def safe(fn, default="<n/a>"):
    try:
        return fn()
    except Exception as e:  # noqa
        return "%s (%s)" % (default, str(e).splitlines()[0][:120])


ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(["/Game/Characters"], True)

assets = ar.get_assets_by_path("/Game/Characters", recursive=True)
pkgs = sorted(set(str(a.package_name) for a in assets))
log("registry assets under /Game/Characters: %d" % len(pkgs))

loaded = 0
fail = []
objs = {}
for p in pkgs:
    o = unreal.EditorAssetLibrary.load_asset(p)
    if o:
        loaded += 1
        objs[p] = o
        log("LOAD OK   %s (%s)" % (p, o.get_class().get_name()))
    else:
        fail.append(p)
        warn("LOAD FAIL %s" % p)

# ---- dependencies
opts = unreal.AssetRegistryDependencyOptions()
opts.include_hard_package_references = True
opts.include_soft_package_references = False
opts.include_searchable_names = False
opts.include_soft_management_references = False
opts.include_hard_management_references = False

missing_deps = {}
script_deps = {}
for p in pkgs:
    try:
        deps = ar.get_dependencies(p, opts) or []
    except Exception as e:  # noqa
        warn("get_dependencies failed for %s: %s" % (p, e))
        continue
    for d in deps:
        d = str(d)
        if d.startswith("/Script/"):
            script_deps.setdefault(d, []).append(p.split("/")[-1])
            continue
        if d.startswith("/Game/"):
            if not unreal.EditorAssetLibrary.does_asset_exist(d):
                missing_deps.setdefault(d, []).append(p.split("/")[-1])
        elif d.startswith("/Engine/"):
            if not unreal.EditorAssetLibrary.does_asset_exist(d):
                missing_deps.setdefault(d, []).append(p.split("/")[-1])
for d, users in sorted(missing_deps.items()):
    warn("MISSING DEP %s  <- %s" % (d, ", ".join(sorted(set(users)))))
for d, users in sorted(script_deps.items()):
    if "TP_ThirdPerson" in d or "Variant_" in d:
        warn("PROJECT-CODE DEP %s <- %s" % (d, ", ".join(sorted(set(users)))))
log("script deps (all): %s" % sorted(script_deps.keys()))

# also soft deps report for key assets
sopts = unreal.AssetRegistryDependencyOptions()
sopts.include_hard_package_references = False
sopts.include_soft_package_references = True
sopts.include_searchable_names = False
sopts.include_soft_management_references = False
sopts.include_hard_management_references = False
soft_missing = {}
for p in pkgs:
    try:
        for d in (ar.get_dependencies(p, sopts) or []):
            d = str(d)
            if d.startswith("/Game/") and not unreal.EditorAssetLibrary.does_asset_exist(d):
                soft_missing.setdefault(d, []).append(p.split("/")[-1])
    except Exception:
        pass
for d, users in sorted(soft_missing.items()):
    log("SOFT-MISSING DEP %s <- %s" % (d, ", ".join(sorted(set(users)))))

# ---- helpers
def get(name):
    return objs.get(ROOT + "/" + name)


mesh = get("Meshes/SKM_Manny_Simple")
skel = get("Meshes/SK_Mannequin")
idle = get("Anims/Unarmed/MM_Idle")
bs = get("Anims/Unarmed/BS_Idle_Walk_Run")
abp = get("Anims/Unarmed/ABP_Unarmed")

# ---- skeleton compat
def skel_of_mesh(m):
    return m.get_editor_property("skeleton") if m else None


def skel_of_anim(a):
    if a is None:
        return None
    try:
        return a.get_skeleton()
    except Exception:
        return a.get_editor_property("skeleton")


def skel_of_bs(b):
    if b is None:
        return None
    try:
        return b.get_skeleton()
    except Exception:
        return safe(lambda: b.get_editor_property("skeleton"), None)


sk_mesh = skel_of_mesh(mesh)
sk_idle = skel_of_anim(idle)
sk_bs = skel_of_bs(bs)
sk_abp = safe(lambda: abp.get_editor_property("target_skeleton"), None) if abp else None
paths = {
    "SKM_Manny_Simple.skeleton": sk_mesh,
    "MM_Idle": sk_idle,
    "BS_Idle_Walk_Run": sk_bs,
    "ABP_Unarmed.target_skeleton": sk_abp,
}
for k, v in paths.items():
    log("skeleton %-32s -> %s" % (k, v.get_path_name() if hasattr(v, "get_path_name") else v))
vals = [v for v in paths.values() if hasattr(v, "get_path_name")]
skel_compat = len(vals) == 4 and len(set(v.get_path_name() for v in vals)) == 1
log("skeleton compatible: %s" % ("yes" if skel_compat else "no"))

# bone count (several fallbacks)
bone_count = None
try:
    sm = unreal.SkeletonModifier()
    sm.set_skeletal_mesh(mesh)
    names = sm.get_all_bone_names()
    bone_count = len(names)
    log("bone count (SkeletonModifier): %d ; first bones: %s" % (bone_count, [str(n) for n in names[:6]]))
except Exception as e:  # noqa
    log("SkeletonModifier unavailable: %s" % str(e).splitlines()[0][:100])
if bone_count is None:
    try:
        tn = unreal.AnimationLibrary.get_animation_track_names(idle)
        bone_count = len(tn)
        log("bone tracks in MM_Idle (fallback for bone count): %d" % bone_count)
    except Exception as e:  # noqa
        log("bone count not measurable via Python: %s" % str(e).splitlines()[0][:100])

# ---- root motion
root_motion = {}
CLIPS = [
    "Anims/Unarmed/MM_Idle",
    "Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd",
    "Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd",
    "Anims/Unarmed/Attack/MM_Attack_01",
    "Anims/Unarmed/Attack/MM_Attack_02",
]
for c in CLIPS:
    a = get(c)
    n = c.split("/")[-1]
    if a is None:
        warn("root motion: %s not loaded" % n)
        continue
    info = {}
    info["enable_root_motion"] = safe(lambda: a.get_editor_property("enable_root_motion"))
    info["root_motion_root_lock"] = safe(lambda: str(a.get_editor_property("root_motion_root_lock")))
    info["force_root_lock"] = safe(lambda: a.get_editor_property("force_root_lock"))
    length = safe(lambda: a.get_play_length(), None)
    info["length_s"] = round(length, 3) if isinstance(length, float) else length
    info["frame_rate"] = safe(lambda: str(unreal.AnimationLibrary.get_frame_rate(a)))
    info["num_frames"] = safe(lambda: unreal.AnimationLibrary.get_num_frames(a))
    info["rate_scale"] = safe(lambda: a.get_editor_property("rate_scale"))
    # root bone translation sampling
    try:
        t_end = a.get_play_length()
        for bone in ("root", "pelvis"):
            t0 = unreal.AnimationLibrary.get_bone_pose_for_time(a, bone, 0.0, False)
            t1 = unreal.AnimationLibrary.get_bone_pose_for_time(a, bone, t_end, False)
            d = t1.translation - t0.translation
            # also max XY excursion over the clip
            maxd = 0.0
            steps = 20
            for i in range(steps + 1):
                ti = unreal.AnimationLibrary.get_bone_pose_for_time(a, bone, t_end * i / steps, False)
                dd = ti.translation - t0.translation
                maxd = max(maxd, (dd.x ** 2 + dd.y ** 2) ** 0.5)
            info["%s_delta_end" % bone] = "(%.2f, %.2f, %.2f) cm" % (d.x, d.y, d.z)
            info["%s_max_xy_excursion_cm" % bone] = round(maxd, 2)
    except Exception as e:  # noqa
        info["root_translation"] = "not measurable via Python (%s)" % str(e).splitlines()[0][:120]
    # additive / notifies
    info["additive_anim_type"] = safe(lambda: str(a.get_editor_property("additive_anim_type")))
    try:
        ev = unreal.AnimationLibrary.get_animation_notify_events(a)
        info["notify_events"] = [
            "%s@%.2f" % (str(e.get_editor_property("notify_name")), e.get_editor_property("trigger_time_offset") if False else e.get_editor_property("display_time"))
            for e in ev
        ]
    except Exception as e:  # noqa
        try:
            ev = unreal.AnimationLibrary.get_animation_notify_event_names(a)
            info["notify_names"] = [str(x) for x in ev]
        except Exception as e2:  # noqa
            info["notifies"] = "not accessible via Python (%s)" % str(e2).splitlines()[0][:100]
    root_motion[n] = info
    log("ROOTMOTION %s: %s" % (n, info))

# ---- morph targets, bounds, slots, physics, LOD
morphs = None
if mesh:
    morphs = safe(lambda: len(mesh.get_editor_property("morph_targets")), None)
    log("morph_targets count: %s" % morphs)
    b = safe(lambda: mesh.get_bounds(), None)
    if hasattr(b, "box_extent"):
        log("bounds origin=%s box_extent=%s sphere=%.1f -> height approx %.1f cm (2*extent.z)" % (
            b.origin, b.box_extent, b.sphere_radius, 2 * b.box_extent.z))
    else:
        log("bounds: %s" % b)
    ib = safe(lambda: mesh.get_editor_property("imported_bounds"), None)
    log("imported_bounds: %s" % ib)
    try:
        slots = [str(m.get_editor_property("material_slot_name")) + "=" +
                 (m.get_editor_property("material_interface").get_path_name() if m.get_editor_property("material_interface") else "None")
                 for m in mesh.get_editor_property("materials")]
        log("material slots (%d): %s" % (len(slots), slots))
    except Exception as e:  # noqa
        log("material slots not accessible: %s" % e)
    log("physics_asset: %s" % safe(lambda: mesh.get_editor_property("physics_asset").get_path_name()))
    lod = safe(lambda: unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).get_lod_count(mesh), None)
    log("LOD count: %s" % lod)
    log("post_process_anim_blueprint: %s" % safe(lambda: mesh.get_editor_property("post_process_anim_blueprint")))

# ---- BlendSpace
if bs:
    for prop in ("blend_parameters", "axis_to_scale_animation", "interpolation_type", "target_weight_interpolation_speed_per_sec", "notify_trigger_mode"):
        log("BS.%s = %s" % (prop, safe(lambda: bs.get_editor_property(prop))))
    try:
        for i in range(3):
            log("BS.get_blend_parameter(%d) = %s" % (i, bs.get_blend_parameter(i)))
    except Exception as e:  # noqa
        log("BS.get_blend_parameter not exposed: %s" % str(e).splitlines()[0][:100])
    try:
        samples = bs.get_editor_property("sample_data")
        log("BS sample count: %d" % len(samples))
        for s in samples:
            a = s.get_editor_property("animation")
            log("  sample %s @ %s" % (a.get_name() if a else None, s.get_editor_property("sample_value")))
    except Exception as e:  # noqa
        log("BS samples not accessible: %s" % str(e).splitlines()[0][:100])
    try:
        n = unreal.AnimationLibrary.get_blend_space_samples if False else None
    except Exception:
        pass

# ---- ABP
if abp:
    gc = safe(lambda: abp.generated_class(), None)
    log("ABP generated_class: %s" % (gc.get_path_name() if hasattr(gc, "get_path_name") else gc))
    sup = safe(lambda: gc.get_super_class(), None) if hasattr(gc, "get_super_class") else None
    log("ABP parent class: %s" % (sup.get_path_name() if hasattr(sup, "get_path_name") else sup))
    log("ABP parent_class prop: %s" % safe(lambda: abp.get_editor_property("parent_class").get_path_name()))
    log("ABP target_skeleton: %s" % (sk_abp.get_path_name() if hasattr(sk_abp, "get_path_name") else sk_abp))
    cdo = safe(lambda: unreal.get_default_object(gc), None) if gc else None
    for prop in ("velocity", "ground_speed", "speed", "is_falling", "should_move", "is_moving", "acceleration", "direction"):
        log("ABP CDO.%s = %s" % (prop, safe(lambda: cdo.get_editor_property(prop))))

# ---- summary
blockers = []
if fail:
    blockers.append("load fail: %s" % [f.split("/")[-1] for f in fail])
if missing_deps:
    blockers.append("missing hard deps: %d" % len(missing_deps))
if any("TP_ThirdPerson" in d for d in script_deps):
    blockers.append("TP_ThirdPerson C++ dependency")
if not skel_compat:
    blockers.append("skeleton mismatch")
rm_short = {k: (v.get("enable_root_motion"), v.get("root_motion_root_lock"), v.get("root_max_xy_excursion_cm")) for k, v in root_motion.items()}
log("D0 SUMMARY: loaded %d/%d, missing deps: %s, skeleton compatible: %s, bones: %s, root motion: %s, morphs: %s, blockers: %s" % (
    loaded, len(pkgs), sorted(missing_deps.keys()), "yes" if skel_compat else "no", bone_count, rm_short, morphs, blockers))
