# pylint: disable=E0602
Import("env")  # noqa

import os
from glob import glob


def _get_include_list_from_defines(env):
    # Prefer a dedicated project option injected by ESPHome codegen.
    try:
        opt = env.GetProjectOption("custom_wmbus_include_drivers")
    except Exception:
        opt = None
    if opt:
        opt = str(opt).strip().strip('"').strip("'")
        return {name.strip() for name in opt.split(",") if name.strip()}

    # Fallback: try CPPDEFINES if the option is missing.
    defines = env.get("CPPDEFINES", [])
    for d in defines:
        if isinstance(d, tuple) and len(d) == 2 and d[0] == "ESPHOME_WMBUS_INCLUDE_DRIVERS":
            val = d[1]
            if isinstance(val, str):
                val = val.strip().strip('"').strip("'")
            return {name.strip() for name in str(val).split(",") if name.strip()}
        if isinstance(d, str) and d.startswith("ESPHOME_WMBUS_INCLUDE_DRIVERS="):
            val = d.split("=", 1)[1].strip().strip('"').strip("'")
            return {name.strip() for name in val.split(",") if name.strip()}

    return set()


def _rename(path_from, path_to):
    try:
        os.rename(path_from, path_to)
        return True
    except Exception:
        return False


# Determine the path where PlatformIO compiles sources from.
# For ESPHome, component sources live under $PROJECT_SRC_DIR/esphome/components/<component>.
project_src_dir = env.subst("$PROJECT_SRC_DIR")
src_dir = os.path.join(project_src_dir, "esphome", "components", "wmbus_common")

included = _get_include_list_from_defines(env)
include_files = {f"driver_{name}.cc" for name in included}

if not os.path.isdir(src_dir):
    print(f"wmbus_common.pre: src dir not found: {src_dir}")
else:
    print(f"wmbus_common.pre: filtering drivers in {src_dir}")

    # If no selection was provided, keep everything as-is.
    if not included:
        print("wmbus_common.pre: no selected drivers (leaving all driver_*.cc as-is)")
        raise SystemExit(0)

    # Exclude all driver_*.cc not in include_files by renaming to .cc.off
    excluded = []
    kept = []

    off_before = glob(os.path.join(src_dir, "driver_*.cc.off"))
    restored = []

    # Restore any previously excluded file that is now included
    for off_path in glob(os.path.join(src_dir, "driver_*.cc.off")):
        base = os.path.basename(off_path)
        orig = base[:-4]  # strip .off
        if orig in include_files:
            if _rename(off_path, os.path.join(src_dir, orig)):
                restored.append(orig)
    # Now process all .cc files
    for path in glob(os.path.join(src_dir, "driver_*.cc")):
        base = os.path.basename(path)
        if base in include_files:
            kept.append(base)
        else:
            off_path = path + ".off"
            if _rename(path, off_path):
                excluded.append(base)

    # Note: if build dir is reused, most drivers may already be .cc.off, so "excluded" can be empty.
    already_off = max(0, len(off_before) - len(restored))
    print(
        "wmbus_common.pre: include=", sorted(list(included)),
        " kept=", sorted(kept),
        " excluded=", sorted(excluded),
        " already_off=", already_off,
    )
