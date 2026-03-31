# Refactoring `main.c` To-Do List

This checklist tracks the progress of refactoring `main.c` into sub-domains.

## 1. Logger Module

- [x] Create `main/loggier/logger.h`
- [x] Create `main/logger/logger.c`

## 2. Metrics Module

- [ ] Create `main/metrics.h`
- [ ] Create `main/metrics.c`

## 3. GPIO Control Module

- [ ] Create `main/gpio_control.h` with constants.
- [ ] Create `main/gpio_control.c` using the new logger.

## 4. OTA Updater Module

- [ ] Create `main/ota_updater.h` with constants.
- [ ] Create `main/ota_updater.c` using the new logger and metrics.

## 5. Zigbee Handler Module

- [ ] Create `main/zigbee_handler.h` with extern constants.
- [ ] Create `main/zigbee_handler.c` with constant definitions and using the new logger and metrics.

## 6. Refactor `main.c`

- [ ] Update `main/main.c` to use the new modules.

## 7. Update Build System

- [ ] Update `main/CMakeLists.txt` to include the new source files.

## 8. Verification

- [ ] Build the project to ensure all changes compile correctly.
- [ ] Flash the firmware to the device.
- [ ] Test the functionality to confirm everything works as expected.
