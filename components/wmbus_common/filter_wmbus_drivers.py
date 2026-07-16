from pathlib import Path
import shutil


def _copy_cc_to_cpp(component_dir: Path):
    for path in component_dir.glob("*.cc"):
        target = path.with_suffix(".cpp")
        if not target.exists() or path.stat().st_mtime > target.stat().st_mtime:
            shutil.copyfile(path, target)


def _filter_driver_cpp_files(component_dir: Path, selected: set[str]):
    if not selected:
        return

    for path in component_dir.glob("driver_*.cpp"):
        name = path.stem.removeprefix("driver_")
        if name not in selected:
            path.write_text(
                "// disabled by filter_wmbus_drivers.py for this build\n",
                encoding="utf-8",
            )


def main():
    component_dir = Path(__file__).resolve().parent
    _copy_cc_to_cpp(component_dir)

    value = None
    try:
        from SCons.Script import ARGUMENTS  # type: ignore

        value = ARGUMENTS.get("custom_wmbus_include_drivers")
    except Exception:
        value = None

    if not value:
        return

    selected = {item.strip() for item in value.split(",") if item.strip()}
    _filter_driver_cpp_files(component_dir, selected)


main()
