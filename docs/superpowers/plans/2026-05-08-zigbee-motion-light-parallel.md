# Zigbee motion light — parallel Zigbee / LED0 status Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Match `docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md`: occupancy always on GPIO motion independent of strip; Zigbee concurrent; palette B on LED 0 via `link_status_led`; LEDs 1…N‑1 strip only.

**Architecture:** Small `link_status_led` owns pixel 0 + `esp_timer` orange pulse. `zigbee_motion` queues occupancy while disconnected, flushes on join, notifies status on coordinator time success and occupancy write. `main` computes `run_strip`, refactors animations, adds motion polling during bounded post‑wake window when Zigbee started.

**Tech stack:** ESP‑IDF, ESP‑Zigbee, FreeRTOS, `light_driver` strip.

---

### Task 1: `link_status_led` component

**Files:**
- Create: `zigbee-motion-light/components/link_status_led/link_status_led.h`
- Create: `zigbee-motion-light/components/link_status_led/link_status_led.c`
- Create: `zigbee-motion-light/components/link_status_led/CMakeLists.txt`
- Modify: `zigbee-motion-light/main/CMakeLists.txt` add `link_status_led` to `PRIV_REQUIRES`

- [ ] Implement API: `init`, `off`, `set_steering`, `set_joined`, `set_time_synced_from_coordinator`, `notify_occupancy_issued` (orange ~280 ms one‑shot `esp_timer`).

---

### Task 2: `zigbee_motion` — pending occupancy + LED hooks

**Files:**
- Modify: `zigbee-motion-light/components/zigbee_motion/zigbee_motion.c`
- Modify: `zigbee-motion-light/components/zigbee_motion/zigbee_motion.h`
- Modify: `zigbee-motion-light/components/zigbee_motion/CMakeLists.txt` add `REQUIRES link_status_led`

- [ ] Track `s_joined`, `s_pending_valid`, `s_pending_occupied`; `flush_pending_occupancy()` on join paths (STEERING OK, reboot non‑factory).
- [ ] `zigbee_motion_send_occupancy_report`: if `!s_joined` stash pending; else write with dedupe + `link_status_led_notify_occupancy_issued()`.
- [ ] Call `link_status_led_set_steering` / `set_joined` / `set_time_synced_from_coordinator` from existing signal + time read paths; clear `s_joined` on stack init failure.
- [ ] Add `bool zigbee_motion_is_joined(void)`.

---

### Task 3: `main.c` — strip 1…N−1, occupancy + wait loop

**Files:**
- Modify: `zigbee-motion-light/main/main.c`

- [ ] `link_status_led_init()` after `light_driver_init`; remove ad‑hoc LED 0 boot colours (first boot: optional `set_steering` blue only if needed).
- [ ] `led_active_animation` / `led_phase_out`: loops `1 .. N-1`; end “keeper” pixel on index 1.
- [ ] GPIO wake: after `zigbee_motion_init` if applicable, `zigbee_motion_send_occupancy_report(motion_driver_get_state())`.
- [ ] `should_animate` unchanged logic for schedule.
- [ ] Post‑wake wait: when Zigbee was started **or** `need_time_sync`, loop `vTaskDelay` slices, poll motion on GPIO wake, call `send_occupancy_report` on edges; exit when time sync resolved or timeout or `zigbee_motion_is_joined` + min window for GPIO-only path (e.g. 5 s cap).
- [ ] `enter_deep_sleep`: `link_status_led_off()` before strip power off.

---

### Task 4: Build

**Run:** `cd zigbee-motion-light && idf.py set-target esp32c6 && idf.py build` (or user target).

**Expected:** Build succeeds.

---

### Self-review vs spec

| Spec § | Task |
|--------|------|
| 2.1 Occupancy always / strip gated | Task 3 |
| 2.3 Palette B | Task 1 + 2 |
| 3.3 Pixel 0 reserved | Tasks 1 + 3 |
| 4 Pending occupancy | Task 2 |
| 5 Bounded wait, no block before animation | Task 3 |

---

**Plan saved.** Inline execution used in this session after plan write.
