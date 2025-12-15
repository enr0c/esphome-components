# pylint: disable=E0602
Import("env")  # noqa

import os
from glob import glob
import logging
import sys


LOGGER = logging.getLogger(__name__)

def _get_selected_drivers(env) -> set[str]:
    # Primary source: dedicated project option injected by ESPHome codegen.
    try:
        raw = env.GetProjectOption("custom_wmbus_include_drivers")
    except Exception:
        raw = None

    if not raw:
        return set()

    raw = str(raw).strip().strip('"').strip("'")
    return {name.strip() for name in raw.split(",") if name.strip()}


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

if not os.path.isdir(src_dir):
    LOGGER.warning("src dir not found: %s", src_dir)
    raise SystemExit(0)

selected = _get_selected_drivers(env)
if not selected:
    LOGGER.info("no selected drivers; leaving all driver_*.cc as-is")
    raise SystemExit(0)

LOGGER.info("filtering drivers in %s", src_dir)

include_files = {f"driver_{name}.cc" for name in selected}
glob_off = os.path.join(src_dir, "driver_*.cc.off")
glob_cc = os.path.join(src_dir, "driver_*.cc")

off_before = len(glob(glob_off))
restored_count = 0
for off_path in glob(glob_off):
    base = os.path.basename(off_path)[:-4]  # strip .off
    if base in include_files and _rename(off_path, os.path.join(src_dir, base)):
        restored_count += 1

kept = []
excluded_now = 0
for path in glob(glob_cc):
    base = os.path.basename(path)
    if base in include_files:
        kept.append(base)
        continue
    if _rename(path, path + ".off"):
        excluded_now += 1

# If the build dir is reused, many drivers can already be .cc.off, so excluded_now may be 0.
already_off = max(0, off_before - restored_count)
LOGGER.info(
    "selected_drivers=%s kept=%s excluded_now=%d already_off=%d",
    sorted(selected),
    sorted(kept),
    excluded_now,
    already_off,
)
