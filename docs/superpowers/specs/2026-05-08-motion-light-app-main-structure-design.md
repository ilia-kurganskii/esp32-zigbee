# Motion light: simplified `app_main` — dual tracks + sleep gate

**Date:** 2026-05-08  
**Status:** Draft for review  
**Project:** `zigbee-motion-light`  
**Related:** `docs/superpowers/specs/2026-05-08-zigbee-motion-light-design.md` (product behaviour §2–3; unchanged unless noted below).

## 1. Purpose

Refactor wake handling so `app_main` reads as **two parallel lifecycles** plus a clear **combined exit** to deep sleep:

1. **Radio / Zigbee track** — steering, join, time read/sync, occupancy reports, and strict handling of **pending occupancy** until applied (option **B** from brainstorming).
2. **Strip animation track** — runs only when schedule rules demand it; never blocks Zigbee startup or the radio wait predicates.

Eliminate the current sequential pattern (long blocking animation loop, then separate Zigbee wait loop interleaved with motion polling).

## 2. Behaviour vs existing spec

All Home Assistant facing rules in the related spec **remain**:

- Occupancy reporting independent of strip gating (§2.1).
- LED 0 palette and ownership (§2.3, §3.3).
- Concurrent Zigbee and strip (§3.2).

**Additive rules (this doc):**

- **Sleep gate:** deep sleep **only after**  
  **(A)** the animation track has **finished or was not scheduled** for this wake, **and**  
  **(B)** the Zigbee track is **complete** per §5 (including **no pending occupancy intent** awaiting application after join, within cap).

## 3. Recommended architecture (approach 2)

- **`zigbee_motion`** — unchanged public role plus small API for §6; owns pending occupancy buffering and flush-on-join (**with correct rollback on flush failure**, see §6).
- **Animation worker** — `FreeRTOS` task created only when `should_animate`; runs existing `led_active_animation` / `led_phase_out` logic (pixels **1…N−1** only); signals completion to the supervisor (event bit or semaphore).
- **Supervisor (`app_main`)** — after shared init and optional `zigbee_motion_init()`, starts the animation task if needed (**or** sets “animation complete” immediately), then runs the **bounded radio wait loop** (time sync / join margins / motion edge polling **unchanged** in intent from current `main.c`), extended per §5 when occupancy is pending. Loop exits when **both** tracks satisfy §4.

No third “framework” abstraction is required unless the wait loop exceeds ~150 lines—in that case a static `wake_session.c` in `main/` is optional consolidation; **prefer keeping logic in `main.c`** until size forces split.

### Task priorities

Default: Zigbee stack task unchanged; animation task at **≤** Zigbee priority (e.g. `3`) so steering and attribute handling preempt long `vTaskDelay` slices. Document chosen values next to `xTaskCreate`.

## 4. Track completion semantics

### 4.1 Animation track

| Wake case | Completion |
|-----------|-------------|
| `should_animate == false` | **Immediate** completion at start of supervise phase (same task sets “done”). |
| `should_animate == true` | Complete when worker finishes phase-out loop and exits cleanly (signal main). |

If animation task faults (assert / early return — not expected), log and still set completion to avoid deadlock; strip should be cleared in `enter_deep_sleep` as today.

### 4.2 Zigbee track (baseline, unchanged intent)

Reuse current `main.c` branching:

- If **time sync required** this wake: wait until **time synced**, **sync failed**, or **timeout**. Timeout baseline remains tied to **30 s after last motion** as in today’s logic (subtract elapsed since last motion if applicable).
- If **no** time sync required but Zigbee started: wait until join gate (`ZB_JOIN_GATE_US`) **after** `loop_start_us` **and** use GPIO vs timer margin (`GPIO_WAKE_NEED_ZB_MARGIN_US` / `TIMER_WAKE_ZB_MARGIN_US`).
- If Zigbee **never** started (`zigbee_started == false`): track complete **immediately**.

### 4.3 Option B extension — pending occupancy

**Definition:** Coordinator-relevant occupancy **intent** not yet reflected in successful application after join: `s_pending_valid == true` in `zigbee_motion` **or** explicitly documented equivalent after §6 rollback rules.

While `zigbee_motion_occupancy_intent_pending()` is true:

- Zigbee track is **not** complete regardless of join-only margins.
- **Extend** radio deadline `wait_deadline_us` to **`max(previous_deadline_us, wakeup_start_us + ZB_HARD_CAP_EXTRA_PENDING_US)`** where `ZB_HARD_CAP_EXTRA_PENDING_US` is a **single** absolute ceiling from wake start (default **60 s**; place `#define` or Kconfig next to existing margin constants in `main.c`).

If the **absolute cap** elapses and intent is **still** pending:

- **Log** at `ESP_LOGW` with explicit “sleeping with pending occupancy; will retry next wake”.
- Treat Zigbee track as **complete** for sleep (battery bound). **Do not** clear pending except via successful network application or explicit policy in `zigbee_motion` (next wake retries).

### 4.4 Zigbee track “complete” (summary)

`zigbee_track_complete` is true when **either** Zigbee was not started this wake, **or** all of the following hold:

1. **Baseline exit** from the **existing** `main.c` wait loop (time sync resolved / failed / timeout, or join gate + margin satisfied when time sync not awaited).
2. **`!zigbee_motion_occupancy_intent_pending()`**, **or** the supervisor has hit the §4.3 absolute cap **and** logged the WARN path (pending carried to next wake).

## 5. Supervisor loop (conceptual)

```text
init (unchanged order: NVS, light, link_status_led, schedule, motion, logs)
compute need_time_sync, zigbee_started, gpio_wake, early occupancy on GPIO (unchanged)
compute should_animate (unchanged)

if zigbee_started:
    zigbee_motion_init()  // as today

record wakeup_start_us = esp_timer_get_time()
compute base wait_deadline_us from existing main.c rules (time sync path vs join margin path)
anim_done = !should_animate
if should_animate:
    xTaskCreate(animation_worker, ...)

while (!(anim_done && zigbee_track_complete)):
    if should_animate:
        anim_done = animation_signalled_complete()
    if zigbee_motion_occupancy_intent_pending():
        wait_deadline_us = max(wait_deadline_us, wakeup_start_us + ZB_HARD_CAP_EXTRA_PENDING_US)
    inner_baseline_exit = /* mirror current main.c: time bits, join gate, timeout */
    cap_elapsed = (esp_timer_get_time() - wakeup_start_us) >= ZB_HARD_CAP_EXTRA_PENDING_US
    pending = zigbee_motion_occupancy_intent_pending()
    zigbee_track_complete = inner_baseline_exit && (!pending || (cap_elapsed && logged_pending_sleep_warn))
    on GPIO wake + zigbee_started: motion edge polling → send_occupancy_report (unchanged)
    vTaskDelay(100ms)

enter_deep_sleep()
```

Concrete code may merge “deadline reached” vs “explicit early exit bits” exactly as today; the diagram is **normative** for **ordering** only.

## 6. `zigbee_motion` additions and flush correctness

### 6.1 New query API

```c
/** True while occupancy must still be pushed to stack after join / failed prior attempt */
bool zigbee_motion_occupancy_intent_pending(void);
```

Implement as `return s_pending_valid` (rename internally if clearer: `intent` vs `queued`).

### 6.2 Flush failure must not silently drop intent

Today `flush_pending_occupancy()` clears `s_pending_valid` **before** calling `send_occupancy_to_network`. **Change:** clear pending **only after** successful return from `send_occupancy_to_network`; on failure (`!= ESP_OK` or documented attribute error path), **restore** pending with the same occupied value and log.

Dedup path (“already same as `s_last_occupancy_state`”) counts as successful application for purposes of clearing pending.

## 7. Errors and observability

- Hard-cap sleep with pending: **WARN** + one-line rationale (join never succeeded vs attribute failure vs coordinator offline).
- No new LED colours beyond palette B.

## 8. Testing and acceptance

| # | Scenario | Expect |
|---|----------|--------|
| 1 | GPIO wake, `should_animate`, slow join | Strip runs immediately; Zigbee progresses; sleep only after strip done **and** join margin / sync logic satisfied **and** pending cleared or cap logged. |
| 2 | Motion before join | Pending set; animation may finish early; supervisor stays up until flush succeeds or **60 s** cap WARN. |
| 3 | ZB never started | Immediate Zigbee-complete; animation rule unchanged; sleep promptly after strip if any. |
| 4 | Attribute write fails transiently | Pending remains or is restored; retry within window until success or cap. |

## 9. Implementation boundaries

- Single wake session only; **no** new cluster types or HA behaviour beyond sleep gating refinements above.
- **Do not** block animation start on join/sync (preserves §3.2 of related spec).

## 10. Resolved decisions (traceability)

| Decision | Choice |
|----------|--------|
| Structural pattern | Separate animation task + `app_main` supervisor (brainstorm approach **2**) |
| Occupancy strictness before sleep | **B** — no sleep while pending intent, subject to §4.3 cap |
| Cap | **60 s** absolute-from-wakeup default (`ZB_HARD_CAP_EXTRA_PENDING_US`); tuning via same pattern as existing `*_US` defines |
| Post-cap behaviour | WARN, keep pending for next wake, enter deep sleep |
