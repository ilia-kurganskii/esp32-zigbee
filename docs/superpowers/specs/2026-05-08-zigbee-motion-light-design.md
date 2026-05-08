# Zigbee motion light: parallel Zigbee, schedule-gated strip, LED0 link map

**Date:** 2026-05-08  
**Status:** Approved (brainstorming sign-off)  
**Project:** `zigbee-motion-light`

## 1. Purpose

Make the motion device reliable for Home Assistant and similar hubs by:

- **Always** publishing **occupancy** when motion is real, independent of whether the LED strip animates.
- **Gating the strip** on the night schedule only when **time is synced**; when time is unknown, keep **fail-open** strip behaviour (matches existing `time_schedule` intent).
- Running **Zigbee** (join, time read, reports) **without blocking** the strip animation.
- Using **LED 0** exclusively as a **hub-style link/status** indicator with a fixed colour map.

## 2. Product behaviour

### 2.1 Motion wake (GPIO)

| Condition | Strip animation | Occupancy to coordinator |
|-----------|-----------------|---------------------------|
| Time **not** synced | **Run** (fail-open) | **Yes** on motion edges / policy (see §4) |
| Time **synced** and **inside** active window | **Run** | **Yes** |
| Time **synced** and **outside** active window | **No** | **Yes** |

### 2.2 Timer / other wakes

Unchanged from current product intent unless otherwise specified in implementation: timer wake does not start strip for motion; Zigbee may still run for periodic time resync policy.

### 2.3 LED 0 colour map (palette B)

| Colour | Meaning |
|--------|---------|
| **Off** | Deep sleep (or deliberate all-off before sleep) |
| **Blue** | Network steering / join in progress |
| **Green** | Joined to network |
| **Purple** | Time sync valid (`time_schedule` updated from coordinator successfully on this conceptual “session”; same moment schedule is authoritative) |
| **Orange** | Occupancy report **in flight** (queued or attribute write underway); hold or short pulse, then revert to **green** or **purple** (purple takes precedence when time is synced) |

Failures that are not mapped to colours in **B**: rely on **ESP_LOG**; LED remains the last applicable state (e.g. **blue** while steering retries).

## 3. Architecture

### 3.1 Components

| Unit | Responsibility |
|------|----------------|
| **`main`** | Wake cause, initialization order, computes `run_strip`, runs animation loop on pixels **1…N−1** only, starts Zigbee when policy requires, bounded post-motion radio window **without blocking** animation, deep sleep entry |
| **`zigbee_motion`** | Stack task, steering/join/time read/pending occupancy until connected, notifies status module for steering/join/sync/report lifecycle |
| **`link_status_led` (new)** | Owns **only pixel 0**; implements §2.3 API; no strip patterns here |
| **`time_schedule`** | Existing: `is_time_synced()`, `is_active()` for **strip** only |

### 3.2 Concurrency rules

- **Strip** and **Zigbee** progress on the same wake; animation **must not** wait for join or time sync before first frame.
- **Occupancy** is sent **as soon as the stack can apply it**; if not joined, **defer** inside `zigbee_motion` (§4).

### 3.3 Pixel ownership

- **LED 0:** `link_status_led` only (including orange pulse timing).
- **LEDs 1…N−1:** motion animation (`led_active_animation` / phase-out equivalents refactored to skip index 0).

## 4. Occupancy semantics

- **After join:** keep **deduplication**: do not repeat the same occupied/unoccupied as `s_last_occupancy_state` (same behaviour as today’s `zigbee_motion_send_occupancy_report`).
- **Before join:** maintain a **pending** target occupancy from GPIO. When **joined** (`ZB_CONNECTED_BIT`), synchronise pending to the cluster **once**: if pending differs from `s_last_occupancy_state`, perform one write as today; notify **orange** for that issuance. Subsequent motion edges use the normal path.
- **Orange** applies only when an occupancy attribute update is actually issued via the stack, not during steering/time alone.

## 5. Timing and sleep

- After animation completes (when run), retain a **bounded** wait for Zigbee purposes (reuse/tune existing ~30 s from **last motion** policy) **without** delaying the animation start.
- Zigbee initialization on wake remains policy-driven (`need_zigbee`: resync interval, GPIO motion, etc.—match or minimally extend existing `main` rules).

## 6. Errors

- Steering failure: existing retry behaviour + **blue** while retrying.
- Time read failure: log; LED stays **green** if joined (no purple).
- Occupancy: never silently drop pending **occupied** intent before sleep; if join impossible within window, document log + leave for next wake (implementation may increment metrics in logs).

## 7. Testing and acceptance

1. **Unsynced time + motion:** strip runs; LED **blue → green → purple** when time arrives; HA sees occupancy shortly after radio ready.  
2. **Synced + in window:** strip runs; occupancy + orange blink.  
3. **Synced + out of window:** no strip; occupancy still reaches HA; LED **purple** when synced.  
4. **Motion before join:** occupancy appears after join consistent with §4 buffering rule.  
5. **Sleep:** LED0 and strip off at deep sleep entry.

## 8. Implementation boundaries (non-goals)

- No ZCL **On/Off** commands to downstream lights (occupancy-only, option **A** from brainstorming).
- Do not introduce **red** or other colours outside palette **B** for this feature unless a future revision extends the palette.
- No Visual Companion dependency for delivery.

## 9. Resolved decisions (traceability)

- Command path: **A** — occupancy reporting to HA/automation only.  
- Parallelism UX: **2** — animate per schedule/time rules immediately; Zigbee concurrent.  
- LED map: **B** — off / blue / green / purple / orange.  
- Occupancy vs strip: **Yes** — always report motion to network; strip schedule-gated when synced.
