#pragma once

// Forces all driver translation units into the final link so that their
// static self-registration initializers run at startup.
void wmbus_ensure_all_drivers(void);
