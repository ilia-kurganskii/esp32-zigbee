# NVRAM Rejoin & Diagnostic Logging

**Project:** `zigbee-motion-light`  
**Component:** `components/zigbee_motion/zigbee_motion.c`  
**Status:** Implemented and verified on hardware (deep sleep wake → fast rejoin)

This document describes how the motion light rejoins a Zigbee network after deep sleep, how to read the NVRAM diagnostic logs, and what was fixed to make rejoin reliable.

For background research (ZBOSS internals, partition layout, timing estimates), see [deep-sleep-fast-join-research.md](./deep-sleep-fast-join-research.md).

---

## Summary

After `esp_deep_sleep_start()`, the chip performs a **full reset**. All RAM Zigbee state is lost, but network credentials persist in the **`zb_storage`** flash partition (16 KB in `partitions.csv`).

The correct wake path is:

1. Stack loads credentials from `zb_storage`
2. ZBOSS re-associates with the saved parent on the saved channel
3. Application receives `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT`
4. Application marks the device joined and sends occupancy — **without** network steering

Network steering is only for **factory-new** pairing or when stored credentials are invalid.

---

## Boot flow

```text
Deep sleep wake (full reset)
        │
        ▼
esp_zb_start(false)  →  load zb_storage
        │
        ├─ factory_new=1  ──► NETWORK_STEERING  ──► first join
        │
        └─ factory_new=0  ──► stack rejoins parent
                                    │
                    ┌───────────────┴───────────────┐
                    ▼                               ▼
         DEVICE_REBOOT + valid network      still associating
                    │                               │
                    ▼                               ▼
         mark_joined(), send occupancy     retry INITIALIZATION
         (~0.7–2 s typical)                  (do NOT start steering)
```

Implementation lives in `esp_zb_app_signal_handler()` — cases `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START`, `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT`, and `ESP_ZB_BDB_SIGNAL_STEERING`.

---

## NVRAM snapshot log

Every important boot/rejoin event prints one line:

```text
I (700) ZIGBEE_MOTION: NVRAM snapshot [DEVICE_REBOOT]: factory_new=0 dev_joined=1 valid=1 PAN=0xa18f Ch=25 Addr=0xed51 ExtPAN=00:12:4b:00:31:df:99:9a
```

| Field | Meaning |
|-------|---------|
| `factory_new` | `1` = never joined / erased flash; `0` = credentials in `zb_storage` |
| `dev_joined` | `esp_zb_bdb_dev_joined()` — stack considers itself on a network |
| `valid` | Application check: joined **and** PAN/short address are not `0x0000`, `0xfffe`, or `0xffff` |
| `PAN` | 16-bit PAN ID |
| `Ch` | RF channel (11–26) |
| `Addr` | Device short address on the network |
| `ExtPAN` | Extended PAN ID (8 bytes, MSB first in log) |

### Snapshot contexts

| Context tag | When |
|-------------|------|
| `DEVICE_FIRST_START` | First boot after erase, before steering |
| `DEVICE_REBOOT` | Successful reboot signal with saved network |
| `BDB Device Reboot` | Failed reboot signal (may still be mid-rejoin) |
| `STEERING join` | First-time join via network steering |
| `STEERING invalid` | Steering returned OK but network state is bogus |

There is no public API to dump the raw `zb_storage` blob. These snapshots are the practical way to verify that flash restore is working.

---

## Signal handler rules

### `DEVICE_REBOOT` / `DEVICE_FIRST_START`

**Success (`ESP_OK`):**

- `factory_new` → start network steering (first pairing)
- `!factory_new` + `valid=1` → **Rejoined from NVRAM** — lock channel, mark joined
- `!factory_new` + `valid=0` → credentials corrupt; fall back to steering

**Failure (`ESP_FAIL` or other):**

- `valid=1` → treat as joined anyway (stack can report failure while network is already up)
- `valid=0` + `!factory_new` → **Rejoin in progress, retrying initialization** — wait for stack, do not steer
- otherwise → retry initialization

### `STEERING`

- Ignored if already joined (prevents duplicate join handling after a fast rejoin)
- Used only for factory-new pairing or invalid NVRAM recovery

### Helpers

```c
zigbee_motion_network_is_valid()      // rejects fake joins (0xffff PAN/addr)
zigbee_motion_log_nvram_snapshot()    // one-line diagnostic dump
zigbee_motion_lock_channel_from_stack() // restrict scans to joined channel after rejoin
```

---

## What was broken (and fixed)

### Problem 1: Steering on every wake

Older code started network steering even when `zb_storage` had valid credentials. That forced a full channel scan (3–10+ s) instead of a fast parent rejoin (~0.7–2 s).

**Fix:** On non-factory-reset reboot, mark joined from `DEVICE_REBOOT` when the network is valid. Only steer when `factory_new` or credentials are invalid.

### Problem 2: Misinterpreting `DEVICE_REBOOT` + `ESP_FAIL`

During rejoin, the stack may emit several `DEVICE_REBOOT` signals with `ESP_FAIL` **before** association completes. Early snapshots show `dev_joined=0 valid=0` but PAN/Ch/Addr already loaded from NVRAM — that is normal, not a storage failure.

The old handler logged *"Silent rejoin failed, falling back to network steering"* and scheduled steering. That interfered with the in-progress rejoin and stretched wake time to ~8 s.

**Fix:**

1. If `valid=1` on a failed signal → mark joined immediately
2. If `valid=0` and NVRAM present → retry `INITIALIZATION`, not steering

---

## Expected logs

### Good — fast rejoin after deep sleep (~700 ms–2 s)

```text
I (700) ZIGBEE_MOTION: NVRAM snapshot [DEVICE_REBOOT]: factory_new=0 dev_joined=1 valid=1 PAN=0xa18f Ch=25 Addr=0xed51 ExtPAN=...
I (700) ZIGBEE_MOTION: Device started up in non factory-reset mode
I (700) ZIGBEE_MOTION: Rejoined from NVRAM
I (710) ZIGBEE_MOTION: Monitor task created after joining
I (8656) MOTION_LIGHT: Wake cycle complete (bits 0x3), entering deep sleep
```

Checklist:

- Same `PAN`, `Ch`, `Addr`, `ExtPAN` across wake cycles
- No `Start network steering` after sleep
- No `Silent rejoin failed` message

### Good — slow rejoin, fixed handler

Rejoin can still take a few seconds if the parent is slow to respond. With the fix you should see initialization retries, not steering:

```text
I (5726) ZIGBEE_MOTION: NVRAM snapshot [BDB Device Reboot]: factory_new=0 dev_joined=0 valid=0 PAN=0xa18f Ch=25 Addr=0xed51 ExtPAN=...
W (5726) ZIGBEE_MOTION: BDB Device Reboot failed with status: ESP_FAIL
W (5736) ZIGBEE_MOTION: Rejoin in progress, retrying initialization
...
I (8316) ZIGBEE_MOTION: NVRAM snapshot [BDB Device Reboot]: factory_new=0 dev_joined=1 valid=1 PAN=0xa18f Ch=25 Addr=0xed51 ExtPAN=...
I (8316) ZIGBEE_MOTION: Rejoined from NVRAM (BDB Device Reboot reported ESP_FAIL)
```

PAN/Ch/Addr stay consistent — **storage is fine**; the stack just needed more time.

### Bad — storage empty or erased

```text
I (...) ZIGBEE_MOTION: NVRAM snapshot [DEVICE_FIRST_START]: factory_new=1 dev_joined=0 valid=0 PAN=0xffff Ch=0 Addr=0xffff ExtPAN=00:00:...
I (...) ZIGBEE_MOTION: Device started up in factory-reset mode
I (...) ZIGBEE_MOTION: Start network steering
```

Requires coordinator permit join and first-time pairing.

### Bad — credentials present but permanently invalid

```text
I (...) NVRAM snapshot [DEVICE_REBOOT]: factory_new=0 dev_joined=0 valid=0 PAN=0xffff Ch=0 Addr=0xfffe ExtPAN=...
W (...) NVRAM data present but network invalid, falling back to steering
```

Network may have been reset on the coordinator, or `zb_storage` is corrupt. Erase flash and re-pair, or leave/join from the coordinator.

---

## How to test

### 1. First pairing (after erase)

```bash
idf.py -p /dev/cu.usbmodem1101 erase-flash flash monitor
```

Open permit join on the coordinator. Expect `STEERING join` snapshot with `valid=1`. Save PAN/Ch/Addr/ExtPAN from the log.

### 2. Deep sleep rejoin

Trigger motion (or wait for a wake cycle). After `Entering deep sleep...`, trigger another wake. Compare the new `DEVICE_REBOOT` snapshot to the saved values — they must match.

### 3. Regression check

After several sleep/wake cycles, confirm:

- Wake-to-joined time stays under ~2 s in the common case
- No steering on non-factory wakes
- Occupancy reports reach the coordinator

### 4. Firmware update without re-pairing

```bash
idf.py -p /dev/cu.usbmodem1101 flash monitor   # no erase-flash
```

`zb_storage` is preserved; device should rejoin on first boot with the same PAN/Ch/Addr.

---

## Flash erase reference

| Command | Effect |
|---------|--------|
| `idf.py -p PORT erase-flash` | Wipes app, NVS, **`zb_storage`** — factory-new device |
| `idf.py -p PORT flash` | Updates firmware only; keeps join credentials |
| `idf.py -p PORT erase-flash flash` | Clean slate + new firmware (use for pairing tests) |

---

## Related files

| File | Role |
|------|------|
| `components/zigbee_motion/zigbee_motion.c` | Signal handler, NVRAM snapshots, join logic |
| `main/main.c` | Supervisor waits for Zigbee track, then deep sleep |
| `partitions.csv` | `zb_storage` partition at `0xf1000`, 16 KB |
| `docs/deep-sleep-fast-join-research.md` | Research notes, ZBOSS log signatures, partition details |

---

## Troubleshooting

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| Steering on every wake | Old firmware or `factory_new=1` | Flash current build; avoid `erase-flash` between tests |
| `valid=0` but PAN/Addr look real | Rejoin still in progress | Wait; should resolve with init retry (not steering) |
| Different PAN/Addr after wake | Coordinator changed or storage corrupt | Re-pair after `erase-flash` |
| Join takes 8+ s with steering fallback | Pre-fix firmware | Flash build with init-retry fix |
| `STEERING join` after `Rejoined from NVRAM` | Stray steering scheduled during rejoin | Fixed: steering ignored when already joined |
