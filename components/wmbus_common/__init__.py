import esphome.config_validation as cv
from esphome.const import SOURCE_FILE_EXTENSIONS, CONF_ID
from esphome.loader import get_component, ComponentManifest
from esphome import codegen as cg
from pathlib import Path

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
        pass

    var = cg.new_Pvariable(config[CONF_ID], sorted(_registered_drivers))
    await cg.register_component(var, config)
