"""Read-only validation of every CharacterProfileData (Docs/CHARACTER_VIEWER_SETUP.md section 2.10).

Runs the C++ checks in UCharacterProfileValidator (Source/CharacterShowcase/Character/
CharacterProfileValidator.h) on each profile and prints what is wrong, where, and how
to fix it. Never saves or modifies anything.

Recommended (exit code 0 = no Error, non-zero = at least one Error):
  UnrealEditor-Cmd.exe "<abs>\\CharacterShowcase.uproject" -run=pythonscript
      -script="<abs>\\Scripts\\ValidateProfiles.py" -unattended -nosplash -nop4 -NullRHI -log

Also works with -ExecutePythonScript="<abs>\\Scripts\\ValidateProfiles.py" (same report;
the editor then logs "Python script executed with errors" on an Error but its process
exit code stays 0 -- use the commandlet form above when the exit code matters).

Which profiles: every CharacterProfileData under /Game/Portfolio/Data (recursive), or
only the folders/assets given with one or more -ProfilePath=<path> arguments
(comma-separated also accepted), e.g. -ProfilePath=/Game/Portfolio/Data/DA_Character_Manny

Output (LogPython):
  [ValidateProfiles] <asset>: E=<n> W=<n> I=<n>
  [ValidateProfiles]   [Error][Part] <message> (<field>)
  ...
  [ValidateProfiles] TOTAL profiles=<n> E=<n> W=<n> I=<n> RESULT=PASS|FAIL
"""
import shlex
import sys

import unreal

TAG = "[ValidateProfiles]"
DEFAULT_PATHS = ["/Game/Portfolio/Data"]
PROFILE_CLASS = "CharacterProfileData"


def log(msg):
    unreal.log("%s %s" % (TAG, msg))


def parse_profile_paths():
    """-ProfilePath= values from the script arguments and the process command line."""
    tokens = list(sys.argv[1:])
    try:
        tokens += shlex.split(unreal.SystemLibrary.get_command_line(), posix=False)
    except Exception:  # noqa: a malformed command line just means "no extra paths"
        pass
    paths = []
    for token in tokens:
        token = token.strip().strip('"')
        if token.lower().startswith("-profilepath="):
            for value in token.split("=", 1)[1].split(","):
                value = value.strip().strip('"')
                if value and value not in paths:
                    paths.append(value)
    return paths or list(DEFAULT_PATHS)


def object_path(path):
    """/Game/Dir/DA_X -> /Game/Dir/DA_X.DA_X (leaves an object path alone)."""
    if "." in path.rsplit("/", 1)[-1]:
        return path
    return "%s.%s" % (path, path.rsplit("/", 1)[-1])


def find_profiles(paths):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([p for p in paths if unreal.EditorAssetLibrary.does_directory_exist(p)], True)
    found = []
    for path in paths:
        if unreal.EditorAssetLibrary.does_directory_exist(path):
            for data in registry.get_assets_by_path(path, recursive=True):
                if str(data.asset_class_path.asset_name) == PROFILE_CLASS:
                    found.append(str(data.package_name))
        elif unreal.EditorAssetLibrary.does_asset_exist(path):
            found.append(path.split(".")[0])
        else:
            unreal.log_warning("%s Path not found (skipped): %s" % (TAG, path))
    return sorted(set(found))


def severity_name(severity):
    if severity == unreal.ViewerIssueSeverity.ERROR:
        return "Error"
    if severity == unreal.ViewerIssueSeverity.WARNING:
        return "Warning"
    return "Info"


def main():
    paths = parse_profile_paths()
    log("Profile paths: %s" % ", ".join(paths))
    packages = find_profiles(paths)
    if not packages:
        raise RuntimeError("%s No CharacterProfileData found under: %s" % (TAG, ", ".join(paths)))

    totals = {"Error": 0, "Warning": 0, "Info": 0}
    load_failures = 0
    for package in packages:
        profile = unreal.EditorAssetLibrary.load_asset(object_path(package))
        if profile is None or not isinstance(profile, unreal.CharacterProfileData):
            unreal.log_error("%s %s: could not be loaded as CharacterProfileData" % (TAG, package))
            load_failures += 1
            continue
        issues = unreal.CharacterProfileValidator.validate_profile(profile)
        counts = {"Error": 0, "Warning": 0, "Info": 0}
        for issue in issues:
            counts[severity_name(issue.severity)] += 1
        log("%s: E=%d W=%d I=%d" % (package, counts["Error"], counts["Warning"], counts["Info"]))
        for issue in issues:
            log("  [%s][%s] %s (%s)" % (severity_name(issue.severity), issue.category, issue.message, issue.field))
        for key in totals:
            totals[key] += counts[key]

    failed = totals["Error"] > 0 or load_failures > 0
    log("TOTAL profiles=%d E=%d W=%d I=%d load_failures=%d RESULT=%s" % (
        len(packages), totals["Error"], totals["Warning"], totals["Info"], load_failures, "FAIL" if failed else "PASS"))
    if failed:
        # An uncaught exception makes -run=pythonscript return a non-zero exit code.
        raise RuntimeError("%s %d Error(s), %d load failure(s) -- see the lines above." % (TAG, totals["Error"], load_failures))


main()
