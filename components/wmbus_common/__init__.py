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
        # Build sets for include/exclude decisions on driver files.
        include_files = {f"driver_{name}.cc" for name in self.include_drivers}
        exclude_files = {f"driver_{name}.cc" for name in self.exclude_drivers}

        filtered = []
        for fr in super().resources:
            res = getattr(fr, "resource", fr)
            s = str(res)
            fname = Path(s).name
            # Only gate files that look like driver_*.cc.
            if fname.startswith("driver_") and fname.endswith(".cc"):
                # Prefer explicit include: keep only selected drivers.
                if fname in include_files and fname not in exclude_files:
                    filtered.append(fr)
                else:
                    # Skip unselected drivers.
                    continue
            else:
                # Non-driver files are always kept.
                filtered.append(fr)
        resources = filtered

        # Optional: emit a small hint in the manifest for troubleshooting.
        # This shows how many driver sources remain after filtering.
        try:
            from esphome.core import LOGGER  # noqa: F401
            # Only log if LOGGER exists to avoid import issues in early phases.
            LOGGER.info("wmbus_common: selected drivers=%s; excluded=%s; resources=%d",
                       sorted(_registered_drivers), sorted(self.exclude_drivers), len(resources))
            pass
        except Exception:
            pass

        return resources


async def to_code(config):
    component = get_component("wmbus_common")
    component.__class__ = WMBusComponentManifest
    # Drivers explicitly selected by the user.
    component.include_drivers = set(_registered_drivers)
    # Exclude any others.
    component.exclude_drivers = AVAILABLE_DRIVERS - component.include_drivers

    var = cg.new_Pvariable(config[CONF_ID], sorted(_registered_drivers))
    await cg.register_component(var, config)
