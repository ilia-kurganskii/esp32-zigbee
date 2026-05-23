# Parallel Zigbee + Extra Task Research

**Project:** `zigbee-motion-light`  
**Date:** 2026-05-23  
**Goal:** Collect documentation and source-code findings for running **Zigbee stack task** and an **abstract application task** in parallel with **no artificial blocking** between them.

**Related docs:**

- [`deep-sleep-fast-join-research.md`](./deep-sleep-fast-join-research.md) — join latency after deep sleep (orthogonal but critical)
- [`docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md`](../../docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md) — product concurrency rules
- [`docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md`](../../docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md) — dual-track supervisor spec

---

## Executive summary

1. **Zigbee must run in a dedicated FreeRTOS task** that calls `esp_zb_start(false)` then blocks in `esp_zb_stack_main_loop()`.
2. **Any extra task** (animation, sensor polling, custom worker) runs as a **second FreeRTOS task**, spawned immediately after init — **never wait for join** before starting work.
3. **Cross-task Zigbee API calls require `esp_zb_lock_acquire()` / `esp_zb_lock_release()`** unless running inside Zigbee stack callbacks or `esp_zb_task_queue_post()` callbacks.
4. **Prefer high-level wrappers** (`zigbee_motion_send_occupancy_report()`) that handle pending state and locking internally.
5. **Task priorities:** Zigbee task **5**; extra task **≤ 3–4** so radio work preempts long `vTaskDelay` loops.
6. **Supervisor (`app_main`)** polls completion of both tracks and gates deep sleep on **AND** of animation + Zigbee readiness.
7. **Join delay is separate from task parallelism** — motion-light currently forces full network steering on every reboot, adding 3–10+ s regardless of parallel task design.

---

## 1. Core concurrency model (Espressif)

### 1.1 Zigbee owns one dedicated FreeRTOS task

Every project in this repo follows the same pattern:

```c
static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    // ... register endpoints ...
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();   // blocks forever
}
```

Started from `app_main` via:

```c
xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
```

**Implication:** The "extra task" must be a **second FreeRTOS task**. `app_main` becomes a lightweight **supervisor** (or exits after spawning tasks).

Reference: [`zigbee-light/main/esp_zb_light.c`](../../zigbee-light/main/esp_zb_light.c)

### 1.2 Thread safety — mandatory lock

From [ESP Zigbee SDK §2.4.1 — Zigbee API Lock](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/developing.html):

| Context | Lock required? |
|---------|----------------|
| Callbacks from `esp_zb_stack_main_loop()` (incl. `esp_zb_app_signal_handler`) | **No** |
| Callbacks posted via `esp_zb_task_queue_post()` | **No** |
| Any other task (`app_main`, animation, sensor, monitor) | **Yes** — `esp_zb_lock_acquire()` / `esp_zb_lock_release()` |

Repo example in `zigbee_motion.c`:

```c
esp_zb_lock_acquire(portMAX_DELAY);
esp_zb_zcl_set_attribute_val(MOTION_LIGHT_ENDPOINT, ...);
esp_zb_lock_release();
```

**Note:** `esp_zb_task_queue_post()` is documented by Espressif but **not used anywhere in this repo**. All cross-task ZB access goes through the lock.

The same lock is held inside `esp_zb_stack_main_loop()` while the Zigbee task is running.

### 1.3 Three safe ways to touch Zigbee from the extra task

```
Extra task                          Zigbee_main task
──────────                          ────────────────
Call high-level wrapper ──────────► esp_zb_stack_main_loop
  (zigbee_motion_send_*)

Direct ZB API + lock ─────────────► esp_zb_stack_main_loop

                                    esp_zb_app_signal_handler
                                      → mark_joined / flush pending

                                    esp_zb_scheduler_alarm callbacks
                                      → retry commissioning
```

1. **High-level component API** (preferred) — e.g. `zigbee_motion_send_occupancy_report()` handles pending + lock internally.
2. **Direct ZB API + lock** — for custom clusters/attributes.
3. **Defer to Zigbee context** — `esp_zb_scheduler_alarm()` for retries (used in all signal handlers).

### 1.4 Typical Zigbee main task (official SDK)

From Espressif developing guide:

```c
void esp_zigbee_main_task(void *pvParameters)
{
   esp_zigbee_config_t config = ESP_ZIGBEE_ZC_CONFIG();
   ESP_ERROR_CHECK(esp_zigbee_init(&config));
   /* Do some stack configuration here. */
   /* Create and register the ZCL data model. */
   ESP_ERROR_CHECK(esp_zigbee_start(false));
   esp_zigbee_launch_mainloop();  // equivalent to esp_zb_stack_main_loop()
   esp_zigbee_deinit();
   vTaskDelete(NULL);
}
```

---

## 2. "No delay" rules (product + architecture specs)

From `docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md` §3.2 and `motion-light-app-main-structure-design.md`:

| Rule | Meaning |
|------|---------|
| **Start both immediately** | Extra task must **not** wait for join/time-sync before first frame/work unit |
| **Zigbee always on wake** | `zigbee_motion_init()` every wake — policy controls **how long supervisor waits**, not whether stack starts |
| **Occupancy decoupled** | Motion → occupancy intent immediately; `zigbee_motion` buffers until joined |
| **Priority ordering** | Zigbee task **5**; extra task **≤ 3–4** so radio preempts long `vTaskDelay` loops |
| **Sleep gate = AND** | Deep sleep only when **both** tracks complete (animation done + Zigbee baseline + no pending occupancy, with 60 s cap) |

### 2.1 Conceptual supervisor (spec §5)

```text
init drivers
zigbee_motion_init()          // Task A starts NOW
xTaskCreate(extra_task, ...)  // Task B starts NOW (or set anim_done immediately)
while (!(anim_done && zigbee_track_complete)):
    poll flags / event bits
    vTaskDelay(100ms)
enter_deep_sleep()
```

### 2.2 Concurrency rules from product spec §3.2

- **Strip** and **Zigbee** progress on the same wake; animation **must not** wait for join or time sync before first frame.
- **Occupancy** is sent **as soon as the stack can apply it**; if not joined, **defer** inside `zigbee_motion` (§4).
- **LED 0** owned by `link_status_led`; **LEDs 1…N−1** by animation task.

---

## 3. Repo patterns — reference implementations

### 3.1 Parallel task + Zigbee (no mutual blocking at start)

| Project | Extra task | Zigbee task | Pattern |
|---------|-----------|-------------|---------|
| **zigbee-motion-light** (current) | `light_anim` prio **5** | `Zigbee_main` prio **5** | Both spawned in `app_main`; supervisor polls |
| **zigbee-wtw** | `led_blink` prio **3** | `Zigbee_main` prio **5** | LED runs independently |
| **zigbee-co2** | `sensor_task` prio **6** | ZB inside sensor_task after I²C read | Sequential within one task (not true parallel) |
| **zigbee-wtw OTA** | `ota_update_task` prio **5** | spawned from ZB callback | Parallel heavy work |

### 3.2 Fast join (no unnecessary Zigbee delay)

| Project | Reboot behavior |
|---------|-----------------|
| `zigbee-light` | Steering **only** if `esp_zb_bdb_is_factory_new()` |
| `zigbee-co2` | Non-factory reboot → `go_to_deep_sleep()` immediately |
| `zigbee-remote` | Non-factory reboot → deep sleep immediately |

Correct pattern from `zigbee-light/main/esp_zb_light.c`:

```c
case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
        if (esp_zb_bdb_is_factory_new()) {
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Device rebooted");  // NVRAM rejoin — no steering
        }
```

### 3.3 Current motion-light bugs (add seconds of Zigbee delay)

From `components/zigbee_motion/zigbee_motion.c`:

```c
case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    if (err_status == ESP_OK) {
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);  // ALWAYS
```

Additional issues:

- Join marked only on `STEERING`, not `DEVICE_REBOOT` + `ESP_OK`
- `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` — multi-channel scan adds 3–10+ s
- Animation task prio **5** == Zigbee — spec says **≤ 3**

Full join analysis: [`deep-sleep-fast-join-research.md`](./deep-sleep-fast-join-research.md)

---

## 4. Current `zigbee-motion-light` task topology

```
app_main (supervisor, prio 1 default)
├── xTaskCreate → "Zigbee_main" (prio 5) → esp_zb_stack_main_loop [blocks]
├── xTaskCreate → "light_anim"  (prio 5) → LED strip animation [parallel]
└── after join → xTaskCreate → "zb_monitor" (prio 4) → motion polling + occupancy

Supervisor loop (main.c):
  while (!animation_finished || !zigbee_joined)
      vTaskDelay(100ms)
  enter_deep_sleep()
```

### 4.1 Public API for cross-task coordination

From `components/zigbee_motion/zigbee_motion.h`:

| API | Role in parallel design |
|-----|------------------------|
| `zigbee_motion_init()` | Spawns Zigbee task; returns handle |
| `zigbee_motion_send_occupancy_report()` | Safe from any task; queues if !joined |
| `zigbee_motion_publish_occupancy_refresh()` | Force write even if dedupe would skip |
| `zigbee_motion_is_joined()` | Supervisor join gate |
| `zigbee_motion_occupancy_intent_pending()` | Blocks sleep until flushed (option B) |
| `zigbee_motion_is_finished()` | Monitor task lifecycle (motion cleared) |

### 4.2 Pending occupancy (join latency interaction)

`zigbee_motion.c` queues occupancy until joined (`s_pending_valid`). Slow steering directly delays HA motion reports.

`flush_pending_occupancy()` clears pending **only after** successful `send_occupancy_to_network()` return — on failure, intent is retained for retry.

### 4.3 Monitor task (post-join worker)

Created in `zigbee_motion_mark_joined()` at priority **4**:

- Polls `motion_driver_get_state()` every 100 ms
- Sends occupancy on edge changes
- Sets `s_zigbee_finished = true` when motion clears
- **Note:** `main.c` currently does **not** wait on `zigbee_motion_is_finished()` — only on `zigbee_motion_is_joined()`

### 4.4 Planned but not fully implemented

From `docs/superpowers/plans/2026-05-08-motion-light-app-main-structure.md` (still pending vs current `main.c`):

- [ ] `EventGroup` + `WAKE_EG_ANIM_DONE_BIT` instead of polling `s_animation_finished`
- [ ] Unconditional Zigbee init (partially done)
- [ ] `ZB_HARD_CAP_EXTRA_PENDING_US` (60 s cap)
- [ ] Animation prio **3**
- [ ] Supervisor timing from `zigbee_motion_init()` time, not post-animation
- [ ] Dual-track sleep gate: animation **AND** Zigbee baseline **AND** pending occupancy rules

---

## 5. Template: abstract extra task + Zigbee (minimal delay)

```c
// app_main — supervisor pattern
void app_main(void) {
    nvs_flash_init();
    init_hardware();

    EventGroupHandle_t eg = xEventGroupCreate();

    zigbee_motion_init();                              // Zigbee starts immediately

    xTaskCreate(extra_worker, "extra", 4096, eg, 3, NULL);  // prio < 5

    int64_t t0 = esp_timer_get_time();
    bool anim_done = false, zb_done = false;

    while (!(anim_done && zb_done)) {
        anim_done = (xEventGroupGetBits(eg) & EXTRA_DONE_BIT) != 0;
        zb_done   = zigbee_motion_is_joined() && !zigbee_motion_occupancy_intent_pending();
        // optional: baseline margins, cap logic...
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    enter_deep_sleep();
}

// extra_worker — never calls ZB APIs directly; use component wrappers
static void extra_worker(void *arg) {
    EventGroupHandle_t eg = arg;
    // do work immediately — no wait for join
    do_extra_work();
    xEventGroupSetBits(eg, EXTRA_DONE_BIT);
    vTaskDelete(NULL);
}
```

Include order: **`freertos/FreeRTOS.h` before `freertos/task.h`** (project rule in `CLAUDE.md`).

### 5.1 Do / Don't for zero-delay parallelism

| ✅ Do | ❌ Don't |
|------|---------|
| Spawn both tasks right after init | Block extra task on `zigbee_motion_is_joined()` before starting |
| Use `zigbee_motion_send_*()` wrappers | Call `esp_zb_*` from extra task without lock |
| Handle `DEVICE_REBOOT` as joined | Always run network steering |
| Single-channel mask when PAN known | Scan all 16 channels |
| Extra task prio 3–4, Zigbee 5 | Same priority (starvation risk) |
| Keep lock sections short | Hold `esp_zb_lock` during long animation loops |
| Use `esp_zb_app_signal_handler` for join-side effects | Poll join from inside `esp_zb_task` before `esp_zb_start` |

---

## 6. Synchronization primitives available

| Mechanism | Used for | In repo |
|-----------|----------|---------|
| `bool` flags + `vTaskDelay` poll | animation finished, joined | `main.c`, `light_animation.c` |
| `EventGroup` bits | strip completion signal | Spec/plan only |
| `esp_zb_lock` | cross-task ZB API | `zigbee_motion.c` |
| `esp_zb_scheduler_alarm` | deferred ZB callbacks | all ZB projects |
| `esp_zb_app_signal_handler` | join/steering lifecycle | all ZB projects |
| `esp_timer` (one-shot) | LED0 orange pulse | `link_status_led` |
| Pending occupancy buffer | motion-before-join | `s_pending_valid` in `zigbee_motion.c` |

**Not used in repo:** `esp_zb_task_queue_post`, semaphores, queues for ZB↔app messaging.

---

## 7. Join latency vs task parallelism (separate concerns)

Parallel tasks eliminate **application-level** blocking (animation waiting for radio). **Radio join time** is independent:

| Path | Time | Fix |
|------|------|-----|
| NVRAM rejoin (`DEVICE_REBOOT` OK, no steering) | ~0.5–2 s | Factory-new-only steering |
| Full steering, all channels | 3–10+ s | Single channel + skip steering on reboot |
| Coordinator re-interview | +1–60 s | Coordinator config (outside firmware) |

See [`deep-sleep-fast-join-research.md`](./deep-sleep-fast-join-research.md) P0 checklist.

### 7.1 Recommended signal handler fix

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
esp_zb_set_secondary_network_channel_set(0);
```

---

## 8. Dual-track supervisor (full spec)

From `docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md`:

### 8.1 Track completion semantics

**Animation track:**

| Wake case | Completion |
|-----------|------------|
| `should_animate == false` | Immediate completion at supervisor start |
| `should_animate == true` | Worker finishes phase-out and sets event bit |

**Zigbee track — baseline wait:**

- If time sync required: wait until synced, failed, or timeout (30 s from last motion)
- If no time sync: wait until join gate + GPIO/timer margin
- **Option B extension:** while `zigbee_motion_occupancy_intent_pending()`, extend deadline to max(previous, wakeup + 60 s cap)

**Zigbee track complete when:**

1. Baseline exit from wait loop satisfied
2. `!zigbee_motion_occupancy_intent_pending()`, **or** 60 s cap elapsed with WARN logged

### 8.2 Timing constants (from implementation plan)

```c
#define ZB_HARD_CAP_EXTRA_PENDING_US   (60 * 1000000LL)
#define WAKE_EG_ANIM_DONE_BIT          BIT0
```

Task priorities from plan:

- Zigbee main: **5**
- Strip animation: **3**

---

## 9. Key file index

| File | Parallel-task relevance |
|------|---------------------------|
| `zigbee-motion-light/main/main.c` | Supervisor; spawns both tasks |
| `zigbee-motion-light/components/zigbee_motion/zigbee_motion.c` | ZB task, lock, pending queue, monitor task |
| `zigbee-motion-light/components/zigbee_motion/zigbee_motion.h` | Public API for cross-task coordination |
| `zigbee-motion-light/components/light_animation/light_animation.c` | Extra task example |
| `zigbee-motion-light/components/light_animation/light_animation.h` | Animation task API |
| `zigbee-motion-light/main/esp_zb_motion_light.h` | Channel mask, ED config constants |
| `zigbee-motion-light/ARCHITECTURE.md` | Wake flow (partially outdated vs dual-track spec) |
| `docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md` | §3.2 concurrency rules |
| `docs/superpowers/specs/2026-05-08-motion-light-app-main-structure-design.md` | Dual-track supervisor spec |
| `docs/superpowers/plans/2026-05-08-motion-light-app-main-structure.md` | Implementation plan |
| `docs/superpowers/plans/2026-05-08-zigbee-motion-light-parallel.md` | LED0 + pending occupancy plan |
| `zigbee-motion-light/docs/deep-sleep-fast-join-research.md` | Join speed optimization |
| `zigbee-light/main/esp_zb_light.c` | Correct join signal handling |
| `zigbee-co2/main/esp_zb_co2_sensor.c` | Deep sleep + correct reboot join pattern |
| `zigbee-remote/main/esp_zb_remote.c` | Sleepy ED + correct reboot pattern |
| `zigbee-wtw/components/led_signal/led_signal.c` | Low-prio parallel LED task |
| `zigbee-wtw/main/zigbee_handler/zigbee_handler.c` | OTA parallel task from ZB callback |
| `deep_sleep/main/esp_ot_sleepy_device.c` | Thread deep sleep analogy (ParentInfo restore) |

---

## 10. Gap summary (action items for two parallel tracks)

### Track A — Extra task (abstract)

- [ ] Lower animation/extra task priority to **3**
- [ ] Switch completion signalling to **EventGroup** (per plan)
- [ ] Never gate task creation on join
- [ ] Wire `zigbee_motion_is_finished()` into sleep gate if monitor task lifecycle matters

### Track B — Zigbee (minimize radio delay)

- [ ] Fix signal handler: steering only if `esp_zb_bdb_is_factory_new()`
- [ ] Mark joined on `DEVICE_REBOOT` + `ESP_OK`
- [ ] Replace `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` with `(1 << ESP_ZB_PRIMARY_CHANNEL_MASK)`
- [ ] Call `zigbee_motion_mark_joined()` → flush pending occupancy immediately

### Track C — Supervisor

- [ ] Implement dual-track sleep gate from app-main-structure spec
- [ ] Start `loop_start_us` at `zigbee_motion_init()`, not after animation
- [ ] Add `ZB_HARD_CAP_EXTRA_PENDING_US` pending occupancy cap with WARN path

---

## 11. External references

- [ESP Zigbee SDK — Developing (lock, main loop)](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/developing.html)
- [ESP Zigbee SDK FAQ §6.3.2 — rejoin on reboot](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)
- [ESP Zigbee SDK FAQ §6.4.4 — keep-alive / light sleep](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)
- [ESP Zigbee SDK FAQ §6.4.5 — channel scan scope](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/faq.html)
- [deep_sleep_end_device example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/sleepy_devices/deep_sleep_end_device)
- [light_sleep_end_device example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/sleepy_devices/light_sleep_end_device)
- [Issue #9 — silent rejoin / deep sleep](https://github.com/espressif/esp-zigbee-sdk/issues/9)
- [Issue #264 — steering on non-factory reboot (fixed in v1.2.0)](https://github.com/espressif/esp-zigbee-sdk/issues/264)
- [Issue #163 — nwk_join.c log interpretation](https://github.com/espressif/esp-zigbee-sdk/issues/163)
- [Issue #481 — 5 s awake may be too short for reports](https://github.com/espressif/esp-zigbee-sdk/issues/481)
- [ESP-IDF deep sleep modes](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/system/sleep_modes.html)

---

## 12. SDK signal reference (join lifecycle)

From `zdo/esp_zigbee_zdo_common.h`:

| Signal | When | Status ESP_OK means |
|--------|------|---------------------|
| `ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP` | After `esp_zb_start(false)` | Framework ready; start BDB init |
| `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START` | First boot / factory new init | Ready for commissioning |
| `ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT` | Reboot with saved network info | **Join or rejoin successfully** |
| `ESP_ZB_BDB_SIGNAL_STEERING` | After steering commissioning | Joined via discovery (factory-new path) |

APIs to check state:

- `esp_zb_bdb_is_factory_new()` — never joined / erased NVRAM
- `esp_zb_bdb_dev_joined()` — currently on network
- `esp_zb_get_pan_id()`, `esp_zb_get_current_channel()`, `esp_zb_get_short_address()`
