# Deep Sleep → Fast Zigbee Join Research

**Project:** `zigbee-motion-light`  
**Date:** 2026-05-22  
**Goal:** Collect everything relevant to rejoining the Zigbee network as quickly as possible after `esp_deep_sleep_start()` wake (full chip reset).

---

## Executive summary

After deep sleep, the ESP32 reboots and **all RAM/runtime Zigbee state is lost**. ZBOSS cannot do a true “silent rejoin” that skips the spec re-attach path entirely. What *is* possible — and what commercial sleepy devices do — is a **fast rejoin using NVRAM-persisted network credentials** (PAN ID, extended PAN ID, NWK address, keys, parent info) stored in the **`zb_storage`** flash partition.

The fastest path is **not** BDB network steering (full discovery). It is:

1. Load network info from `zb_storage` on `esp_zb_start()`
2. Let the stack **auto rejoin** the saved parent/network
3. Treat **`ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT` + `ESP_OK`** as “already joined”
4. Only run **`ESP_ZB_BDB_MODE_NETWORK_STEERING`** when `esp_zb_bdb_is_factory_new()` is true

**Critical finding in this repo:** `zigbee_motion.c` currently starts network steering on **every** wake, including non-factory-reset reboots. That matches the old broken deep-sleep example (fixed in esp-zigbee-sdk v1.2.0) and is likely the largest avoidable join delay.

**Expected join times (order of magnitude, from Espressif issues/logs):**

| Path | Typical time | Notes |
|------|-------------|-------|
| NVRAM rejoin (correct reboot path) | ~0.5–2 s | OpenThread deep_sleep README: detached→child in ~180 ms after ParentInfo restore; Zigbee similar when steering skipped |
| Full network steering (all channels) | 3–10+ s | Scans 16 channels; motion-light uses `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` |
| Steering + coordinator re-interview | 4–60+ s | HA/Z2M may re-read clusters after device announce |
| Light sleep (no reboot) | Sub-second reports | Stack state preserved; Espressif recommended for frequent wake |

---

## What happens inside ZBOSS (`nwk/nwk_join.c`)

`nwk_join.c` lives inside the **precompiled ZBOSS library** (`espressif__esp-zboss-lib`), not in this repo. Source is not shipped; behavior must be inferred from logs, headers, and GitHub issues.

### Log signatures (from esp-zigbee-sdk issue #163)

```
I (...) ZBOSS: nwk/nwk_join.c:293   Will assoc to pan 0xf455 on channel 11, dev 0x0, iface_id 0
I (...) ZBOSS: zdo/zdo_app_join.c:771   CONGRATULATIONS! joined status 0, addr 0xa3fd, iface_id 0, ch 11, rejoin 0
I (...) ZBOSS: nwk/nwk_join.c:251   No dev for join
I (...) ZBOSS: nwk/nwk_main.c:4409   zb_nwk_do_leave param 6 rejoin 0
```

| Log line | Meaning |
|----------|---------|
| `Will assoc to pan ... on channel N` | Association/rejoin attempt to known PAN on specific channel (fast path when NVRAM has network info) |
| `CONGRATULATIONS! joined ... rejoin 0/1` | Join finished; `rejoin 1` = explicit NWK rejoin command path |
| `No dev for join` | Join machinery idle or no candidate parent — often seen during leave/restart races |
| `zb_nwk_do_leave param ... rejoin 0` | Stack leaving network; `rejoin 0` = leave without rejoin |

### Join vs rejoin vs steering (conceptual)

```text
esp_deep_sleep_start()
        │
        ▼
   Full chip reset (RAM lost, flash NVRAM intact)
        │
        ▼
esp_zb_init() → esp_zb_start(false) → esp_zb_stack_main_loop()
        │
        ├─ Load zb_storage (network key, PAN, short addr, parent, frame counters…)
        │
        ├─ [Factory new] ──► BDB steering ──► scan channels ──► associate ──► STEERING signal
        │
        └─ [Non-factory-new reboot] ──► nwk_join/rejoin to saved parent ──► DEVICE_REBOOT signal (ESP_OK)
                                              │
                                              ▼
                                    Optional: Device Announce (if addr/parent changed)
                                              │
                                              ▼
                                    Application can send ZCL reports
```

### “Silent rejoin” — not supported after deep sleep

Espressif closed [esp-zigbee-sdk#9](https://github.com/espressif/esp-zigbee-sdk/issues/9) with official position:

- **Deep sleep:** all runtime state lost → must follow Zigbee re-attach/rejoin procedure
- **Light sleep:** RAM preserved → seamless continuation (recommended for battery devices with short sleep intervals)
- Buffering full session in RTC for instant TX without rejoin: **no near-term plan** ([#501](https://github.com/espressif/esp-zigbee-sdk/issues/501))

Industry “silent rejoin” (PDF notes in #9): if parent + short address still valid in NVRAM, device talks to parent **without explicit rejoin command** until parent link fails. ZBOSS implements this via the **`DEVICE_REBOOT`** autostart path — **not** via application-triggered steering.

---

## ESP-Zigbee SDK signals (application layer)

From `managed_components/espressif__esp-zigbee-lib/include/zdo/esp_zigbee_zdo_common.h`:

| Signal | Value | When | Status ESP_OK means |
|--------|-------|------|---------------------|
| `ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP` | 0x01 | After `esp_zb_start(false)` | Framework ready; start BDB init |
| `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START` | 0x05 | First boot / factory new init | Ready for commissioning |
| `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT` | 0x06 | Reboot with saved network info | **Join or rejoin successfully** |
| `ESP_ZB_BDB_SIGNAL_STEERING` | 0x02 | After steering commissioning | Joined via discovery (factory-new path) |
| `ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE` | 0x02 | Remote/local announce | Device visible on network |

From `esp_zigbee_core.h` — `esp_zb_start()` autostart behavior:

> Startup means either Formation (for ZC), **rejoin** or discovery/association join.

FAQ ([ESP Zigbee SDK FAQ §6.3.2](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)):

> Device not on network but previously joined → **will attempt to rejoin previous network after rebooting**.

APIs to check state:

- `esp_zb_bdb_is_factory_new()` — never joined / erased NVRAM
- `esp_zb_bdb_dev_joined()` — currently on network
- `esp_zb_get_pan_id()`, `esp_zb_get_current_channel()`, `esp_zb_get_short_address()`

---

## Correct application pattern (official + this repo)

### ✅ Official deep sleep example (esp-zigbee-sdk `sleepy_devices/deep_sleep_end_device`)

```c
case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (status == SUCCESS) {
        if (ezb_bdb_is_factory_new()) {
            ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
        } else {
            // Already rejoined from NVRAM — do application work immediately
        }
    }
    break;
```

Also sets **single-channel scan mask** before start:

```c
ezb_bdb_set_primary_channel_set(ESP_ZIGBEE_PRIMARY_CHANNEL_MASK);
ezb_bdb_set_secondary_channel_set(ESP_ZIGBEE_SECONDARY_CHANNEL_MASK);
ezb_nwk_set_rx_on_when_idle(false);  // sleepy ED
```

### ✅ This repo — correct examples

| Project | Reboot behavior |
|---------|-----------------|
| `zigbee-light/main/esp_zb_light.c` | Steering **only** if `esp_zb_bdb_is_factory_new()` |
| `zigbee-co2/main/esp_zb_co2_sensor.c` | Same; non-factory → `go_to_deep_sleep()` immediately |
| `zigbee-remote/main/esp_zb_remote.c` | Same; non-factory → sleep immediately |

### ❌ `zigbee-motion-light` — current bug

```182:211:zigbee-motion-light/components/zigbee_motion/zigbee_motion.c
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ...
            ESP_LOGI(TAG, "Start network steering");
            link_status_led_set_steering();
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
```

**Problems:**

1. Always starts steering — forces full discovery even when NVRAM rejoin already succeeded
2. Marks joined only on `ESP_ZB_BDB_SIGNAL_STEERING`, not on `DEVICE_REBOOT` success
3. Uses `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` (all channels) instead of single known channel

This matches the **broken** pattern described in [esp-zigbee-sdk#264](https://github.com/espressif/esp-zigbee-sdk/issues/264) (fixed in SDK v1.2.0 example).

### Recommended fix sketch

```c
case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
        if (esp_zb_bdb_is_factory_new()) {
            link_status_led_set_steering();
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Rejoined from NVRAM");
            zigbee_motion_mark_joined();  // flush pending occupancy here
        }
    }
    break;
```

And in `esp_zb_task()` before `esp_zb_start()`:

```c
esp_zb_set_primary_network_channel_set(1 << ESP_ZB_PRIMARY_CHANNEL_MASK);  // channel 11
esp_zb_set_secondary_network_channel_set(0);  // optional: no fallback scan
```

---

## NVRAM / partitions (required for fast rejoin)

`zigbee-motion-light/partitions.csv`:

```csv
zb_storage, data, fat, 0xf1000, 16K,   # Zigbee persistent network + ZCL data
zb_fct,     data, fat, 0xf5000, 1K,    # Factory channel mask (optional, from mfg_tool)
```

**What persists across deep sleep:** `zb_storage` (flash), not RTC RAM.

**What is lost:** all `RTC_DATA_ATTR` variables (e.g. OpenThread `s_sleep_enter_time` in `deep_sleep/`), Zigbee neighbor tables in RAM, binding state not yet persisted.

Newer SDK deep sleep example also calls:

```c
nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME);
```

Motion-light currently only calls `nvs_flash_init()` in `main.c`. Verify whether `esp_zb_start()` mounts `zb_storage` implicitly (it does in official examples when partition exists) — if rejoin fails after sleep, add explicit partition init.

**Do not** call `esp_zb_factory_reset()` or erase flash between sleeps — that wipes rejoin credentials.

---

## Configuration knobs affecting join speed

### End device aging timeout (`ed_timeout`)

Current motion-light: `ESP_ZB_ED_AGING_TIMEOUT_64MIN` (same as co2/remote).

Parent removes child if no keep-alive before aging expires. Must satisfy:

```text
deep_sleep_interval + join_time + report_time  <<  ed_timeout
```

Motion-light timer wake: **3 minutes** (`DEEP_SLEEP_TIMER_INTERVAL_SEC`). 64 min aging is safe.

If sleep interval approaches aging timeout, parent drops child → **slow steering re-join** instead of fast rejoin.

### Keep-alive (`keep_alive`)

Current: **3000 ms** in `ESP_ZB_ZED_CONFIG()`.

Relevant for **light sleep** (parent polling), not deep sleep wake. After deep sleep reboot, rejoin is a one-shot on wake.

For light-sleep alternative: increase `keep_alive` to lengthen sleep between polls ([FAQ §6.4.4](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)).

Note from [esp-zigbee-sdk#9](https://github.com/espressif/esp-zigbee-sdk/issues/9): default long poll interval is **5 s** until set after join via `zb_zdo_pim_set_long_poll_interval()`.

### Channel scan scope

| Setting | Motion-light today | Recommended |
|---------|-------------------|-------------|
| Primary channel mask | All channels (`ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK`) | Single channel, e.g. `1 << 11` |
| Secondary mask | Not set | `0` or backup channel only |
| `esp_zb_bdb_set_scan_duration()` | Default | Lower duration if steering required |

FAQ §6.4.5: scanning all channels causes **many RF peaks at startup** and adds seconds of latency.

### Sleepy end device mode

For minimum join traffic after rejoin, ED should be **`rx_off_when_idle = true`** (sleepy). Official deep sleep example calls `ezb_nwk_set_rx_on_when_idle(false)`.

Motion-light does not explicitly set this — confirm default for `ESP_ZB_DEVICE_TYPE_ED`.

---

## Deep sleep application flow in this repo

### `main/main.c`

| Parameter | Value | Join impact |
|-----------|-------|-------------|
| `DEEP_SLEEP_TIMER_INTERVAL_SEC` | 180 s (3 min) | Periodic heartbeat wake |
| `DEEP_SLEEP_MAX_WAIT_MS` | 300 s (5 min) | Max awake before sleep |
| Wake sources | GPIO (PIR) + timer | GPIO wake needs fast join for occupancy |

Wake sequence: init drivers → `zigbee_motion_init()` always → wait for animation + ZB done → `enter_deep_sleep()`.

### `motion_driver/motion_driver.c`

- GPIO deep sleep wake: `esp_deep_sleep_enable_gpio_wakeup()` on motion pin HIGH
- No Zigbee-specific wake stub

### Pending occupancy (join latency interaction)

`zigbee_motion.c` queues occupancy until joined (`s_pending_valid`). Slow steering directly delays HA motion reports.

Design specs require flushing pending occupancy immediately on join (`docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md` §4).

---

## OpenThread `deep_sleep/` example (Thread, not Zigbee — useful analogy)

Path: `deep_sleep/main/esp_ot_sleepy_device.c`

Key observations from README logs:

```
Read NetworkInfo {rloc:0x4001, extaddr:..., role:child, ...}
Read ParentInfo {extaddr:3afe8db4802dc1aa, version:4}
Role disabled -> detached
Role detached -> child        ← ~180 ms reattach using flash-stored parent
```

Same pattern: **flash-stored parent info → fast reattach**. Deep sleep README warns reattach still needs packets; beneficial when sleep > ~30 min.

---

## Light sleep vs deep sleep (Espressif guidance)

| Mode | Join after wake | Power | Use when |
|------|-----------------|-------|----------|
| **Light sleep** | None (state preserved) | ~tens of µA (C6, improving) | Wake every few seconds–minutes |
| **Deep sleep** | Rejoin from NVRAM | ~7 µA (H2/C6 figures in examples) | Wake every tens of minutes+ |

Light sleep sdkconfig (ESP32-C6): `CONFIG_ESP_PHY_MAC_BB_PD=y` required ([issue #787](https://github.com/espressif/esp-zigbee-sdk/issues/787)).

Motion-light product choice: deep sleep + 3 min timer + PIR GPIO — optimize **rejoin**, not skip it.

---

## Coordinator-side latency (outside firmware)

Even with 1 s rejoin, total motion-to-HA latency can be **3–4 s** ([#9](https://github.com/espressif/esp-zigbee-sdk/issues/9)):

1. Device announce → coordinator sees device
2. Coordinator re-reads attributes / interview
3. Then attribute report appears in Z2M/HA

Mitigations (coordinator/config):

- Keep device in coordinator child table (respect aging timeout)
- Reduce unnecessary re-interview (stable model identifier, consistent endpoints)
- Use binding if appropriate (reduces report routing delay for some setups)

---

## Prioritized optimization checklist for motion-light

### P0 — Application logic (largest win)

- [ ] **Stop steering on non-factory-reset reboot** — handle `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT` + `ESP_OK` as joined
- [ ] **Only steer when `esp_zb_bdb_is_factory_new()`**
- [ ] Call `zigbee_motion_mark_joined()` on reboot rejoin path (flush pending occupancy)

### P1 — Scan / RF

- [ ] Set `esp_zb_set_primary_network_channel_set(1 << 11)` (or known coordinator channel)
- [ ] Set secondary mask to `0` or one backup channel
- [ ] Replace `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` in `esp_zb_task()`

### P2 — NVRAM / partitions

- [ ] Confirm `zb_storage` mounted (`nvs_flash_init_partition(ESP_ZIGBEE_STORAGE_PARTITION_NAME)` if needed)
- [ ] Never erase flash in normal operation; document factory-reset procedure
- [ ] Optional: populate `zb_fct` via Espressif mfg_tool for production channel lock

### P3 — Timing / sleep policy

- [ ] Ensure `DEEP_SLEEP_TIMER_INTERVAL_SEC` ≪ `ED_AGING_TIMEOUT` (currently OK: 3 min vs 64 min)
- [ ] Don't enter deep sleep until occupancy report sent (already partially handled via `app_events`)
- [ ] Increase awake window if reports miss coordinator ([issue #481](https://github.com/espressif/esp-zigbee-sdk/issues/481) — 5 s may be tight)

### P4 — Alternative architecture (product decision)

- [ ] Evaluate **light sleep** if wake interval drops below ~30 min
- [ ] `esp_zb_sleep_enable(true)` + `esp_zb_power_save_init()` — different power model, no reboot

### P5 — Debugging join path

- [ ] Enable ZBOSS debug: `CONFIG_ZB_DEBUG_MODE=y` (already in sdkconfig)
- [ ] Log watch for `nwk/nwk_join.c:293 Will assoc to pan` (fast) vs multi-channel scan (slow)
- [ ] Wireshark 802.15.4 capture on coordinator channel

---

## Reference code locations (this repo)

| File | Relevance |
|------|-----------|
| `components/zigbee_motion/zigbee_motion.c` | Join signal handling, ED config, channel mask — **primary fix target** |
| `main/main.c` | Deep sleep entry, wake sources, supervisor timeout |
| `main/esp_zb_motion_light.h` | `ED_AGING_TIMEOUT`, `ED_KEEP_ALIVE`, channel 11 define |
| `components/motion_driver/motion_driver.c` | GPIO deep sleep wake |
| `partitions.csv` | `zb_storage` / `zb_fct` |
| `sdkconfig.defaults` | `CONFIG_ZB_ZED=y` |
| `zigbee-light/main/esp_zb_light.c` | **Correct** factory-new-only steering |
| `zigbee-co2/main/esp_zb_co2_sensor.c` | Deep sleep + correct reboot join pattern |
| `zigbee-remote/main/esp_zb_remote.c` | Sleepy ED + correct reboot pattern |
| `deep_sleep/main/esp_ot_sleepy_device.c` | Thread deep sleep analogy (ParentInfo restore) |

---

## External references

- [ESP Zigbee SDK FAQ](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html) — rejoin on reboot, channel scan, keep-alive
- [esp-zigbee-sdk deep_sleep_end_device example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/sleepy_devices/deep_sleep_end_device)
- [esp-zigbee-sdk light_sleep_end_device example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/sleepy_devices/light_sleep_end_device)
- [Issue #9 — silent rejoin / deep sleep](https://github.com/espressif/esp-zigbee-sdk/issues/9)
- [Issue #264 — steering on non-factory reboot (fixed in v1.2.0)](https://github.com/espressif/esp-zigbee-sdk/issues/264)
- [Issue #163 — nwk_join.c log interpretation](https://github.com/espressif/esp-zigbee-sdk/issues/163)
- [Issue #481 — 5 s awake may be too short for reports](https://github.com/espressif/esp-zigbee-sdk/issues/481)
- [Issue #501 — RTC session buffer request (declined)](https://github.com/espressif/esp-zigbee-sdk/issues/501)
- [ESP-IDF deep sleep modes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/system/sleep_modes.html)

---

## SDK headers (local, v1.6.0 managed component)

Key files under `managed_components/espressif__esp-zigbee-lib/include/`:

- `esp_zigbee_core.h` — `esp_zb_start()`, channel masks, `esp_zb_bdb_is_factory_new()`, `esp_zb_bdb_dev_joined()`
- `zdo/esp_zigbee_zdo_common.h` — signal definitions including `DEVICE_REBOOT` = rejoin success
- `nwk/esp_zigbee_nwk.h` — neighbor/aging types

ZBOSS public NWK API (`managed_components/espressif__esp-zboss-lib/include/zboss_api_nwk.h`):

- `zb_nwk_get_parent()` — parent short addr after join
- `ZB_REJOIN_BACKOFF` enabled in vendor config

---

## Related internal design docs

- `docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md` — pending occupancy flush on join
- `docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md` — always start Zigbee on wake; bounded wait caps
- `ARCHITECTURE.md` — wake flow overview
