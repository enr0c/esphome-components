# Water and heat meter receiver (ESP32 + CC1101)

ESPHome-based wireless M-Bus (wM-Bus) receiver for water and heat meters, powered via a 5V USB supply.

Example configuration: [`examples/watermeter.yaml`](../examples/watermeter.yaml)
(meter IDs and names in this document are placeholders)

## Hardware

| Component       | Details                         |
|-----------------|---------------------------------|
| Microcontroller | ESP32 (`esp32dev`)              |
| Framework       | esp-idf (min. chip rev 3.1)     |
| Radio module    | CC1101 (868.95 MHz, wM-Bus)     |
| Power supply    | 5V USB                          |

---

## CC1101 Wiring

```
CC1101 Pin │ ESP32 GPIO │ Wire Colour
───────────┼────────────┼────────────
VCC        │ 3.3V       │ Red
GND        │ GND        │ Orange
MOSI       │ GPIO19     │ Yellow
S/CLK      │ GPIO18     │ Green
MISO       │ GPIO23     │ Blue
GDO2       │ GPIO27     │ Grey
GDO0       │ GPIO26     │ Purple
CSN        │ GPIO25     │ White
```

> GDO2 is configured with `INPUT_PULLDOWN` mode.

```
CC1101                          ESP32
┌──────────┐                ┌──────────────┐
│ VCC      │━━━━ Red ━━━━━━━│ 3.3V         │
│ GND      │━━━━ Orange ━━━━│ GND          │
│ MOSI     │━━━━ Yellow ━━━━│ GPIO19       │
│ S/CLK    │━━━━ Green ━━━━━│ GPIO18       │
│ MISO     │━━━━ Blue ━━━━━━│ GPIO23       │
│ GDO2     │━━━━ Grey ━━━━━━│ GPIO27       │
│ GDO0     │━━━━ Purple ━━━━│ GPIO26       │
│ CSN      │━━━━ White ━━━━━│ GPIO25       │
└──────────┘                └──────────────┘
                                    │
                               5V USB PSU
```

- **GDO2 (GPIO27):** sync word detection, asserts when a new frame starts
- **GDO0 (GPIO26):** FIFO threshold, asserts when data is ready to read
- Reception is polling-based

---

## ESPHome Software

The firmware uses the component library in this repository, a fork of
[SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components).

| Component      | Role |
|----------------|------|
| `wmbus_radio`  | CC1101 communication, receives raw wM-Bus frames |
| `wmbus_common` | [wmbusmeters](https://github.com/wmbusmeters/wmbusmeters) driver code: decodes and decrypts telegrams |
| `wmbus_meter`  | Bridges decoded meter data to ESPHome sensors and text sensors |

```yaml
external_components:
  - source: github://enr0c/esphome-components@main
    components: [ wmbus_radio, wmbus_meter, wmbus_common ]
    refresh: 0min
```

Drivers used: `waterstarm` (Lorenz water meters), `sensostar` (Engelmann heat meter).

### Settings

| Setting              | Value                          |
|----------------------|--------------------------------|
| Frequency            | 868.95 MHz                     |
| Loop task stack size | 16384 (`esp32.framework.advanced.loop_task_stack_size`) |
| Logger level         | `INFO` (see Memory notes)      |
| WiFi power save      | Disabled (`NONE`)              |
| WiFi reboot timeout  | 5 min                          |
| Fallback AP SSID     | `portal_<friendly_name>`       |
| Time sync            | Home Assistant                 |

---

## Meters

| Friendly Name         | Meter ID     | Driver     | Type      |
|-----------------------|--------------|------------|-----------|
| Water meter 1         | `0x11111111` | waterstarm | Lorenz    |
| Cold water 2          | `0x22222222` | waterstarm | Lorenz    |
| Cold water 3          | `0x33333333` | waterstarm | Lorenz    |
| Hot water 4           | `0x44444444` | waterstarm | Lorenz    |
| Hot water 5           | `0x55555555` | waterstarm | Lorenz    |
| Underfloor heating    | `0x66666666` | sensostar  | Engelmann |

Meter keys are stored in `secrets.yaml`.

---

## Sensors (Home Assistant)

### Water meters (Lorenz, waterstarm)

| Sensor                 | Field                         | Unit | Category   |
|------------------------|-------------------------------|------|------------|
| Total consumption      | `total_m3`                    | m³   | -          |
| Consumption last month | `consumption_at_history_1_m3` | m³   | -          |
| Battery voltage        | `battery_v`                   | V    | diagnostic |
| RSSI                   | `rssi_dbm`                    | dBm  | diagnostic |
| Status                 | `status`                      | -    | -          |
| Timestamp              | `timestamp_zulu`              | -    | -          |

Water meter 1 additionally exposes the text sensors `history_1_date`, `set_date`
and `meter_datetime`; Hot water 5 exposes `history_1_date`. Dates are
`PointInTime` values and must be text sensors, not numeric sensors.

The deprecated `current_status` field has been removed from the waterstarm driver
to match wmbusmeters.org. Use `status`.

### Heat meter (Engelmann, sensostar)

| Sensor          | Field            | Unit | Category   |
|-----------------|------------------|------|------------|
| Total energy    | `total_kwh`      | kWh  | -          |
| Total water     | `total_water_m3` | m³   | -          |
| Battery voltage | `battery_v`      | V    | diagnostic |
| RSSI            | `rssi_dbm`       | dBm  | diagnostic |
| Status          | `status`         | -    | -          |
| Timestamp       | `timestamp_zulu` | -    | -          |

Other sensostar fields: `target_kwh`, `target_date`, `meter_datetime`,
`target_<n>_kwh` (storage 2..32). `current_status`, `meter_timestamp`,
`consumption_N_months_ago_kwh` and `reporting_date` were removed or renamed to
match upstream `sensostar.xmq`.

---

## Logging telegrams

Each meter has an `on_telegram` trigger that logs the decoded telegram:

```yaml
on_telegram:
  then:
    - logger.log:
        level: INFO
        format: "wmbus %s"
        args:
          - meter.as_json(false).c_str()
```

Use `as_json(false)` (single line). The pretty-printed variant produces about
45 log lines per telegram, which overflows the log queue so that some telegrams
never appear in the log.

Raw frames (RSSI, hex, link mode) are logged at `DEBUG` by the radio `on_frame`
trigger. Only enable this for diagnosis; it allocates large strings.

```
[I][main:977]: wmbus 22222222: {"_":"telegram","media":"water","meter":"waterstarm",
  "id":"22222222","battery_v":2.9,"total_m3":49.108,"status":"OK",
  "meter_datetime":"2026-01-01 12:00","set_date":"2025-12-31","rssi_dbm":-53}
```

---

## Memory notes

The ESP32 has little free heap (about 70-100 KB, largest block about 50 KB), so:

- Keep the logger at `INFO`; `DEBUG` hex dumps allocate and can abort with out-of-memory.
- Fixes in this repository for out-of-memory aborts: JSON string reserve and
  move in `printMeter`, `DVEntryMap` no longer pre-reserves, `bin2hex` reserves
  exactly, and VLAs in AES decryption were replaced with heap vectors.
- Optional savings: remove `captive_portal:`, try `loop_task_stack_size: 8192`,
  remove debug sensors once tuned.
- Frames with a failed DLL CRC are dropped (`DLL CRC check failed, dropping corrupted frame`).
- A `CC1101 FIFO overflow while waiting for sync, flushing` warning at boot is
  harmless: the driver flushes the FIFO and keeps listening (one frame is lost).

---

## Keeping last values in Home Assistant after a restart

The `wmbus_meter` sensors only publish when a telegram arrives, so after an ESP
reboot the entities show *unknown* until the meter transmits again. To keep the
last value, add Home Assistant template sensors that fall back to their own
previous state:

```yaml
template:
  - sensor:
      - name: "Cold water 2 total (held)"
        unique_id: wm_cold_water_2_total_hold
        state: "{% set s = states('sensor.cold_water_2_22222222_total') %}{{ s if s not in ['unknown','unavailable'] else this.state }}"
        availability: "true"
        unit_of_measurement: "m³"
        device_class: water
        state_class: total_increasing
```

Repeat per sensor (totals, history 1, battery) and adjust the source entity ID
(check it under Developer tools → States). Point the Energy dashboard at the
new entities. RSSI, status and timestamp are intentionally not held.

---

## Verifying decoding

Compare against <https://wmbusmeters.org/analyze/> using the raw telegram hex
and the meter key. To compare with upstream, add the remote
`https://github.com/SzczepanLeon/esphome-components` and diff.
