import esphome.config_validation as cv
from esphome.const import SOURCE_FILE_EXTENSIONS, CONF_ID
from esphome.loader import get_component, ComponentManifest
from esphome import codegen as cg
from pathlib import Path
import os

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_DRIVERS = "drivers"

wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    f.stem.removeprefix("driver_") for f in Path(__file__).parent.glob("driver_*.cc")
}

_registered_drivers = set()


validate_driver = cv.All(
    cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_"),
    lambda driver: _registered_drivers.add(driver) or driver,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_DRIVERS, default=set()): cv.All(
            lambda x: AVAILABLE_DRIVERS if x == "all" else x,
            {validate_driver},
        ),
    }
)


class WMBusComponentManifest(ComponentManifest):
    exclude_drivers: set[str]
    include_drivers: set[str]

    @property
    def resources(self):
        # Allow loader to pick up C++ sources with .cc extension
        SOURCE_FILE_EXTENSIONS.add(".cc")
        try:
            base_resources = list(super().resources)
            # Verbose logging of all discovered resources before filtering
            try:
                from esphome.core import LOGGER
                base_names = [Path(str(getattr(fr, 'resource', fr))).name for fr in base_resources]
                LOGGER.info("wmbus_common: base_resources_count=%d", len(base_names))
                # Log driver-looking files separately for easier inspection
                driver_like = sorted([n for n in base_names if n.startswith('driver_') and n.endswith('.cc')])
                LOGGER.info("wmbus_common: base_driver_candidates=%s", driver_like)
            except Exception:
                # As a fallback, print to stdout so it appears in HA build logs
                try:
                    print(f"wmbus_common: base_resources_count={len(base_names)}")
                    print(f"wmbus_common: base_driver_candidates={driver_like}")
                except Exception:
                    pass
        finally:
            # Restore extension set to avoid side effects for other components
            SOURCE_FILE_EXTENSIONS.discard(".cc")

        # Build include/exclude filename sets for driver sources
        include_files = {f"driver_{name}.cc" for name in self.include_drivers}
        exclude_files = {f"driver_{name}.cc" for name in self.exclude_drivers}

        filtered = []
        for fr in base_resources:
            res = getattr(fr, "resource", fr)
            s = str(res)
            fname = Path(s).name
            if fname.startswith("driver_") and fname.endswith(".cc"):
                # Only include selected drivers and exclude the rest
                if fname in include_files and fname not in exclude_files:
                    filtered.append(fr)
            else:
                # Keep all non-driver files
                filtered.append(fr)

        # Debug info to verify which drivers are included
        try:
            from esphome.core import LOGGER
            included_driver_files = sorted([
                Path(str(getattr(fr, 'resource', fr))).name for fr in filtered
                if Path(str(getattr(fr, 'resource', fr))).name.startswith('driver_') and
                   Path(str(getattr(fr, 'resource', fr))).name.endswith('.cc')
            ])
            LOGGER.info("wmbus_common: include_set=%s exclude_set=%s",
                        sorted(self.include_drivers), sorted(self.exclude_drivers))
            LOGGER.info("wmbus_common: included_driver_files=%s", included_driver_files)
            LOGGER.info("wmbus_common: filtered_resources_count=%d", len(filtered))
        except Exception:
            # Fallback to stdout prints if LOGGER is unavailable in this context
            try:
                print(f"wmbus_common: include_set={sorted(self.include_drivers)} exclude_set={sorted(self.exclude_drivers)}")
                print(f"wmbus_common: included_driver_files={included_driver_files}")
                print(f"wmbus_common: filtered_resources_count={len(filtered)}")
            except Exception:
                pass

        return filtered


async def to_code(config):
    component = get_component("wmbus_common")
    component.__class__ = WMBusComponentManifest
    component.include_drivers = set(_registered_drivers)
    component.exclude_drivers = AVAILABLE_DRIVERS - component.include_drivers

    # Log what the validator captured from YAML
    try:
        from esphome.core import LOGGER
        LOGGER.info("wmbus_common: yaml_selected_drivers=%s", sorted(_registered_drivers))
        LOGGER.info("wmbus_common: available_drivers=%s", sorted(AVAILABLE_DRIVERS))
    except Exception:
        try:
            print(f"wmbus_common: yaml_selected_drivers={sorted(_registered_drivers)}")
            print(f"wmbus_common: available_drivers={sorted(AVAILABLE_DRIVERS)}")
        except Exception:
            pass

    # Expose selected drivers to the build system and add a pre-build filter script
    try:
        selected = ",".join(sorted(_registered_drivers))
        if selected:
            cg.add_define("ESPHOME_WMBUS_INCLUDE_DRIVERS", selected)
            # Also inject into PlatformIO build flags so extra_scripts can read it reliably
            # (cg.add_define may not always end up in CPPDEFINES at pre-script time).
            cg.add_platformio_option(
                "build_flags",
                [f'-DESPHOME_WMBUS_INCLUDE_DRIVERS=\\"{selected}\\"'],
            )
            print(f"selected drivers: {selected}")
        # Register a pre-build script to physically exclude non-selected drivers.
        # Use an absolute path to avoid relying on ESP32's extra_build_files copy step.
        script_path = os.path.join(os.path.dirname(__file__), "filter_wmbus_drivers.py")
        if os.path.exists(script_path):
            cg.add_platformio_option("extra_scripts", [f"pre:{script_path}"])
    except Exception:
        # Best-effort; build will proceed even if script/define cannot be added
        pass

    var = cg.new_Pvariable(config[CONF_ID], sorted(_registered_drivers))
    await cg.register_component(var, config)
