# Zigbee Motion Light Architecture

## Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        zigbee_motion_light                       │
│                          (main.c)                                │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    Application Logic                      │  │
│  │                                                          │  │
│  │  • Wake-up handling (GPIO/Timer)                         │  │
│  │  • Time schedule check (22:00-08:00)                     │  │
│  │  • Motion tracking & animation loop                     │  │
│  │  • Deep sleep management                                 │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ uses
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      zigbee_motion                              │
│                    (zigbee_motion.c)                            │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                  Zigbee Stack Management                  │  │
│  │                                                          │  │
│  │  • Zigbee initialization                                 │  │
│  │  • Cluster configuration:                                │  │
│  │    - Basic, Identify, On/Off, Time, Occupancy            │  │
│  │  • Network steering & commissioning                      │  │
│  │  • Time sync from coordinator                            │  │
│  │  • Occupancy reporting (state change detection)          │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ requires
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        time_schedule                              │
│                    (time_schedule.c)                             │
│                                                                  │
│  • Time zone management                                         │
│  • Active hours configuration (22:00-08:00)                    │
│  • Zigbee time sync (2000 epoch → Unix)                        │
│  • RTC memory persistence                                      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        motion_driver                             │
│                    (motion_driver.c)                            │
│                                                                  │
│  • GPIO configuration (AM312 PIR sensor)                       │
│  • Motion state polling                                       │
│  • Deep sleep wake-up configuration                           │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        light_driver                              │
│                    (light_driver.c)                             │
│                                                                  │
│  • LED strip control (65 LEDs)                                  │
│  • RGB color management                                        │
│  • Individual pixel control                                    │
└─────────────────────────────────────────────────────────────────┘
```

## Data Flow

### Wake-up Flow

```
┌──────────────┐
│ Deep Sleep   │
└──────┬───────┘
       │ wake up
       │ (GPIO/Timer)
       ▼
┌──────────────────┐
│   main.c        │
│ app_main()      │
└──────┬───────────┘
       │
       ├─► Initialize NVS
       ├─► Initialize LED
       ├─► Initialize time_schedule
       ├─► Initialize motion_driver
       │
       ├─► Check wake-up reason
       │   │
       │   ├─► GPIO (motion) → need Zigbee + animation
       │   └─► Timer → periodic check
       │
       ├─► Start Zigbee (if needed)
       │   │
       │   └─► zigbee_motion_init()
       │       │
       │       ├─► Create Zigbee task
       │       ├─► Configure clusters
       │       ├─► Start network steering
       │       └─► Request time from coordinator
       │
       ├─► Check time schedule
       │   │
       │   ├─► Time unknown? → animate
       │   └─► Time 22:00-08:00? → animate
       │
       ├─► Animation loop
       │   │
       │   ├─► led_active_animation()
       │   ├─► Check motion state
       │   │   │
       │   │   ├─► Motion detected
       │   │   │   ├─► Report occupancy (true)
       │   │   │   └─► Loop animation
       │   │   │
       │   │   └─► No motion
       │   │       ├─► Report occupancy (false)
       │   │       └─► led_phase_out_animation()
       │
       ├─► Wait for Zigbee sync (30s from last motion)
       │   │
       │   └─► zigbee_motion_wait_for_sync()
       │
       └─► enter_deep_sleep()
```

### Zigbee Communication Flow

```
┌──────────────────┐
│ zigbee_motion    │
│                  │
│  ┌────────────┐ │
│  │ Zigbee     │ │
│  │ Task       │ │
│  └─────┬──────┘ │
└────────┼─────────┘
         │
         ├─► Join network (steering)
         │
         ├─► Request time from coordinator
         │   (Time cluster, read attribute)
         │
         ├─► Receive time response
         │   → Update time_schedule
         │   → Set event flag (synced)
         │
         └─► Report occupancy state
             (Occupancy Sensing cluster, write attribute)
             → Only on state change (true↔false)
```

## Component Dependencies

```
main
  ├─► zigbee_motion
  │     ├─► time_schedule
  │     ├─► espressif__esp-zigbee-lib
  │     └─► espressif__esp-zboss-lib
  ├─► motion_driver
  ├─► light_driver
  ├─► time_schedule
  └─► nvs_flash

zigbee_motion
  ├─► time_schedule
  ├─► espressif__esp-zigbee-lib
  └─► espressif__esp-zboss-lib

time_schedule
  └─► nvs_flash

motion_driver
  └─► driver

light_driver
  ├─► driver
  └─► espressif__led_strip
```

## Key Design Decisions

1. **Component Separation**: Zigbee logic isolated in `zigbee_motion` component for better maintainability
2. **State Change Detection**: Occupancy reports only sent on state changes to reduce Zigbee traffic
3. **Event Groups**: Kept for Zigbee coordination (best practice for async operations)
4. **RTC Memory**: Time sync status and sleep tracking persisted through deep sleep
5. **Animation Loop**: Continuous animation while motion detected, with phase-out on stop
6. **30-Second Timeout**: Wait for Zigbee sync after motion ends before deep sleep

## File Structure

```
zigbee-motion-light/
├── main/
│   ├── main.c                    (220 lines - application logic)
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── components/
│   ├── zigbee_motion/            (NEW - Zigbee stack management)
│   │   ├── zigbee_motion.h
│   │   ├── zigbee_motion.c
│   │   └── CMakeLists.txt
│   ├── time_schedule/            (Time management)
│   ├── motion_driver/            (PIR sensor)
│   └── light_driver/             (LED control)
└── docs/
    └── ARCHITECTURE.md           (This file)
```
