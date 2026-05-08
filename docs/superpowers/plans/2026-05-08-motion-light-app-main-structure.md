# Motion light (`app_main`) dual-track supervisor — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor `zigbee-motion-light` wake handling per [`docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md`](../specs/2026-05-08-motion-light-app-main-structure-design.md): Zigbee starts every wake; strip animation runs in a parallel FreeRTOS task; supervisor exits to deep sleep only when **both** the animation lifecycle and Zigbee readiness (baseline wait + occupancy intent / cap rule) allow it.

**Architecture:** Extend `zigbee_motion` with `zigbee_motion_occupancy_intent_pending()` and correct `flush_pending_occupancy()` so failed applies keep `s_pending_valid`. In `main.c`, allocate a small `EventGroup` for strip completion, spawn `motion_strip_anim_task` (priority `<` Zigbee main task priority `5`), compute **radio timers from `zigbee_motion_init()`** (not post-animation — fixes concurrency vs current code), and consolidate motion edge polling plus baseline exit + pending-cap gating into one supervisor loop shared with animation completion polling.

**Tech stack:** ESP-IDF, ESP-Zigbee (`esp-zigbee-sdk`), FreeRTOS (`tasks`, `event_groups`), `light_driver`, `motion_driver`, `time_schedule`, `link_status_led`, host build target `esp32c6` (or project default).

---

## File structure

| Path | Responsibility |
|------|----------------|
| [`zigbee-motion-light/components/zigbee_motion/zigbee_motion.h`](zigbee-motion-light/components/zigbee_motion/zigbee_motion.h) | Declare `zigbee_motion_occupancy_intent_pending()`. |
| [`zigbee-motion-light/components/zigbee_motion/zigbee_motion.c`](zigbee-motion-light/components/zigbee_motion/zigbee_motion.c) | Implement pending query; fix flush ordering + failure retention. |
| [`zigbee-motion-light/main/main.c`](zigbee-motion-light/main/main.c) | Strip task, unconditional `zigbee_motion_init()`, supervisor timing from init, unified sleep gate, `ZB_HARD_CAP_EXTRA_PENDING_US`, remove `zigbee_started` gating. |

No new components required unless `main.c` grows past maintainability (~300+ LOC of logic); defer split to `wake_session.c` unless needed after implementation.

---

### Task 1: `zigbee_motion` — intent query + safe flush

**Files:**

- Modify: [`zigbee-motion-light/components/zigbee_motion/zigbee_motion.h`](zigbee-motion-light/components/zigbee_motion/zigbee_motion.h)
- Modify: [`zigbee-motion-light/components/zigbee_motion/zigbee_motion.c`](zigbee-motion-light/components/zigbee_motion/zigbee_motion.c)

- [ ] **Step 1 — Add public prototype**

Append before the `#ifdef __cplusplus` closing block:

```c
/**
 * @brief True while occupancy must still be pushed to the stack after join or after a failed apply.
 */
bool zigbee_motion_occupancy_intent_pending(void);
```

- [ ] **Step 2 — Fix `flush_pending_occupancy` and implement query**

Replace the existing `flush_pending_occupancy` body with logic that **does not clear** `s_pending_valid` until `send_occupancy_to_network` returns `ESP_OK`, and implement the new function after `zigbee_motion_is_joined` (keep include order unchanged — file already includes `zigbee_motion.h` via self).

Replace:

```c
static void flush_pending_occupancy(void)
{
    if (!s_joined || !s_pending_valid) {
        return;
    }
    s_pending_valid = false;
    esp_err_t r = send_occupancy_to_network(s_pending_occupied);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "flush pending occupancy failed");
    }
}
```

with:

```c
static void flush_pending_occupancy(void)
{
    if (!s_joined || !s_pending_valid) {
        return;
    }
    bool occ = s_pending_occupied;
    esp_err_t r = send_occupancy_to_network(occ);
    if (r == ESP_OK) {
        s_pending_valid = false;
        return;
    }
    ESP_LOGW(TAG, "flush pending occupancy failed, keeping intent for retry");
}
```

Append after `zigbee_motion_is_joined(void)` definition block:

```c
bool zigbee_motion_occupancy_intent_pending(void)
{
    return s_pending_valid;
}
```

(Dedupe path inside `send_occupancy_to_network`: still returns `ESP_OK`; that counts as clearing intent per spec.)

- [ ] **Step 3 — Build component in isolation**

Run (after loading IDF):

```bash
cd zigbee-motion-light && idf.py set-target esp32c6 build
```

**Expected:** Build succeeds up to linking `main`; if Task 2 not applied yet you may temporarily stub `zigbee_motion_occupancy_intent_pending` callers or complete Task 2 in same branch before final build passes.

If your machine uses `esp32h2` only for this CI target, substitute `esp32h2` in the command.

- [ ] **Step 4 — Commit**

```bash
git add zigbee-motion-light/components/zigbee_motion/zigbee_motion.h \
        zigbee-motion-light/components/zigbee_motion/zigbee_motion.c
git commit -m "fix(zigbee_motion): occupancy intent query and safe flush on failure"
```

---

### Task 2: `main.c` — always-on Zigbee, strip task, unified supervisor

**Files:**

- Modify: [`zigbee-motion-light/main/main.c`](zigbee-motion-light/main/main.c)

- [ ] **Step 1 — Includes and `#define`s**

Keep `FreeRTOS.h` before `task.h`. After `#include "freertos/task.h"` add `#include "freertos/event_groups.h"`.

Add next to existing timing defines:

```c
/* Absolute supervisor cap when occupancy intent pending (µs). */
#define ZB_HARD_CAP_EXTRA_PENDING_US   (60 * 1000000LL)

/* Strip animation signals this bit when finished (scheduled path). */
#define WAKE_EG_ANIM_DONE_BIT          BIT0
```

Declare static globals after `s_last_motion_time_us`:

```c
static EventGroupHandle_t s_wake_event_group;

static inline int64_t i64_max(int64_t a, int64_t b)
{
    return (a > b) ? a : b;
}
```

- [ ] **Step 2 — Animation task**

After `led_phase_out_animation` (still file-local `static`s), insert:

```c
static void motion_strip_anim_task(void *pvParameters)
{
    (void)pvParameters;

    bool motion_detected = motion_driver_get_state();
    if (motion_detected) {
        zigbee_motion_send_occupancy_report(true);
    }

    do {
        led_active_animation();

        motion_detected = motion_driver_get_state();
        if (motion_detected) {
            s_last_motion_time_us = esp_timer_get_time();
            ESP_LOGI(TAG, "Motion still detected, looping animation");
        } else {
            ESP_LOGI(TAG, "No motion detected, starting phase out");
            zigbee_motion_send_occupancy_report(false);
            led_phase_out_animation();
            break;
        }
    } while (motion_detected);

    xEventGroupSetBits(s_wake_event_group, WAKE_EG_ANIM_DONE_BIT);
    vTaskDelete(NULL);
}
```

- [ ] **Step 3 — Replace `app_main` body logic**

Structural rules (matches spec §2, §5, §4):

1. **Remove** `const bool zigbee_started` entirely.
2. **Remove** unconditional `link_status_led_set_steering()` before Zigbee **except** if you deliberately keep short first-boot UX; spec prefers stack/callback driving LED — **delete** the pre-`zigbee_motion_init()` `link_status_led_set_steering()` and the early `if (zigbee_started)` wrapper; keep **first-boot** `ESP_LOGI` + optional `vTaskDelay(200)` without forcing blue (factory-new steering still comes from `zigbee_motion` signal path).
3. After `motion_driver_init()` and `need_time_sync` / RTC accumulation (unchanged), **always** `ESP_ERROR_CHECK(zigbee_motion_init());` **no** `if` around it.
4. **Immediately** after successful init, set `int64_t wakeup_start_us = esp_timer_get_time();` and compute `int64_t loop_start_us = wakeup_start_us` (same as today’s `loop_start_us` role but **before** animation).
5. **GPIO first sample:** `if (gpio_wake) { motion_prev = motion_driver_get_state(); zigbee_motion_send_occupancy_report(motion_prev); }` (drop `&& zigbee_started`).
6. **Event group:** `s_wake_event_group = xEventGroupCreate();` `ESP_ERROR_CHECK(s_wake_event_group ? ESP_OK : ESP_ERR_NO_MEM);`
7. **`should_animate` block:** instead of inline loop, if `should_animate` then `xTaskCreate(motion_strip_anim_task, "strip_anim", 4096, NULL, 3, NULL);` else `xEventGroupSetBits(s_wake_event_group, WAKE_EG_ANIM_DONE_BIT);`
8. **Baseline wait budget** (mirrors current `run_wait_loop` / `wait_deadline_us`, always using **join/margin branch** whenever `need_time_sync` is false):

   Always set `wait_deadline_us = loop_start_us` first (avoids uninitialized reads in the supervisor when the baseline slice is skipped).

   - If `need_time_sync`: compute `remaining_wait_us = 30000000LL - time_since_motion` as today.
     - If `remaining_wait_us > 0`: `wait_deadline_us = loop_start_us + remaining_wait_us`; `run_baseline_wait = true`.
     - If `remaining_wait_us <= 0`: keep `wait_deadline_us = loop_start_us`; `run_baseline_wait = false` and **`zb_baseline_done = true` immediately** (same as skipping the old loop).
   - Else (`!need_time_sync`): `wait_deadline_us = loop_start_us + (gpio_wake ? GPIO_WAKE_NEED_ZB_MARGIN_US : TIMER_WAKE_ZB_MARGIN_US);` **`run_baseline_wait = true`** (always-Zigbee on timer wake).
9. **Supervisor loop** (replaces the old `if (run_wait_loop) { while (...` only — keep its *inner* exit conditions):

```c
    bool zb_baseline_done = !run_baseline_wait;
    bool pending_cap_warn_logged = false;

    for (;;) {
        bool anim_done = (xEventGroupGetBits(s_wake_event_group) & WAKE_EG_ANIM_DONE_BIT) != 0;

        if (zigbee_motion_occupancy_intent_pending()) {
            wait_deadline_us = i64_max(wait_deadline_us, wakeup_start_us + ZB_HARD_CAP_EXTRA_PENDING_US);
        }

        int64_t now_us = esp_timer_get_time();
        bool within_baseline_deadline = run_baseline_wait && now_us < wait_deadline_us;

        if (!zb_baseline_done) {
            bool exit_baseline = false;

            if (need_time_sync) {
                if (zigbee_motion_is_time_synced()) {
                    ESP_LOGI(TAG, "Zigbee time sync completed successfully");
                    s_last_sync_time_us = esp_timer_get_time();
                    s_accumulated_sleep_us = 0;
                    exit_baseline = true;
                } else if (zigbee_motion_is_sync_failed()) {
                    ESP_LOGW(TAG, "Zigbee sync failed");
                    exit_baseline = true;
                } else if (!within_baseline_deadline) {
                    ESP_LOGW(TAG, "Zigbee sync timeout");
                    exit_baseline = true;
                }
            } else {
                if (zigbee_motion_is_joined() &&
                    (now_us - loop_start_us) >= ZB_JOIN_GATE_US) {
                    exit_baseline = true;
                } else if (!within_baseline_deadline) {
                    exit_baseline = true;
                }
            }

            if (exit_baseline) {
                zb_baseline_done = true;
            }
        }

        bool pending_now = zigbee_motion_occupancy_intent_pending();
        bool cap_elapsed = (now_us - wakeup_start_us) >= ZB_HARD_CAP_EXTRA_PENDING_US;

        if (zb_baseline_done && pending_now && cap_elapsed && !pending_cap_warn_logged) {
            ESP_LOGW(TAG, "Sleeping with pending occupancy; will retry next wake");
            pending_cap_warn_logged = true;
        }

        bool zigbee_track_complete = zb_baseline_done &&
                                     (!pending_now || pending_cap_warn_logged);

        if (anim_done && zigbee_track_complete) {
            break;
        }

        if (gpio_wake) {
            bool m = motion_driver_get_state();
            if (m != motion_prev) {
                motion_prev = m;
                zigbee_motion_send_occupancy_report(m);
                if (m) {
                    s_last_motion_time_us = esp_timer_get_time();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Wakeup: %s, synced: %d", wakeup_str, time_schedule_is_time_synced());

    enter_deep_sleep();
```

**Delete** the old blocks: inlined `should_animate` `do{}while`; the standalone `loop_start_us` / `wait_deadline_us` / `if(run_wait_loop){while(...)}` entirely — they are superseded.

**Consistency check:** Supervisor `for(;;)` must include the final `ESP_LOGI` + `enter_deep_sleep()` sequence **after** the loop (as snippet shows). Preserve `enter_deep_sleep` implementation unchanged unless spec later requires wakeup pin tweak.

10. **`need_time_sync` timeout warning:** the old trailing `if (need_time_sync && !time_synced && !sync_failed) ESP_LOGW timeout` moves into `exit_baseline` branch `!within_baseline_deadline` for sync path inside supervisor (shown above). Remove duplicate stray warning after loops.

- [ ] **Step 4 — Full project build**

```bash
cd zigbee-motion-light && idf.py set-target esp32c6 build
```

**Expected:** `Project build complete` with no linker or compile errors.

- [ ] **Step 5 — Commit**

```bash
git add zigbee-motion-light/main/main.c
git commit -m "refactor(motion-light): concurrent strip task and Zigbee supervisor gate"
```

---

### Task 3: Manual verification (no automated unit tests)

**Files:** None — device + hub.

- [ ] **Step 1 — Flash and monitor**

```bash
cd zigbee-motion-light && idf.py -p $PORT flash monitor
```

**Expected:** App boots without watchdog leaks; Zigbee initializes on timer wakes (visible join / keep-alive chatter in coordinator logs shorter than skipping stack entirely).

- [ ] **Step 2 — Walk spec acceptance matrix**

| Scenario | Procedure | Pass |
|---------|-----------|------|
| GPIO + animate + slow join | Cover PIR → strip starts same sample window as steering | Strip runs while BLE/ZB settles; sleeps only after strip done + supervisor allows |
| Motion before join | Factory reset / leave network, trigger GPIO | Logs show queued occupancy; sleeps only after flush or WARN + cap (~60 s logged) |
| Timer wake | Let 10 s asleep timer fire | Zigbee initializes; supervisor runs margin path (~2 s TIMER margin); minimal strip noise |
| Transient occupancy fail | (Optional) Simulate by temporarily breaking radio after join | Pending stays asserted until success or WARN path |

Checkbox each row mentally or in PR description.

---

## Plan self‑review vs spec

| Spec § | Planned coverage |
|--------|-------------------|
| §2 Always `zigbee_motion_init()` | Task 2 |
| §3 Strip task prio ≤ Zigbee (`3` vs `5`) | Task 2 (`xTaskCreate` prio `3`) |
| §4.1 Animation completion signalling | Task 2 `EventGroup` |
| §4.2 Baseline waits + timer/GPIO margins | Task 2 supervisor + `need_time_sync` branching |
| §4.3–4.4 Pending + cap WARN + composition | Task 2 (`ZB_HARD_CAP_EXTRA_PENDING_US`, `pending_cap_warn_logged`) ; Task 1 pending API |
| §6 Flush correctness | Task 1 |
| §9 Power trade-off | Behavioural consequence of Task 2 (document in release notes optional) |

**Gap check:** Related product spec §2.3 / palette B untouched here (already wired through `zigbee_motion` + `link_status_led`). **`loop_start_us` moved earlier** deliberately implements §3.2 parallelism vs old `main.c` — acceptance test should confirm hubs still receive occupancy reliably.

---

**Plan complete and saved to `docs/superpowers/plans/2026-05-08-motion-light-app-main-structure.md`. Two execution options:**

1. **Subagent‑Driven (recommended)** — Dispatch a fresh subagent per task, review between tasks, fast iteration. **REQUIRED SUB‑SKILL:** superpowers:subagent‑driven‑development  

2. **Inline execution** — Run tasks in one session using superpowers:executing‑plans with checkpoints.

**Which approach do you prefer?**
