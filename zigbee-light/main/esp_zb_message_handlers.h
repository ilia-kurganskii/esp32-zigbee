#ifndef ESP_ZB_MESSAGE_HANDLERS_H
#define ESP_ZB_MESSAGE_HANDLERS_H

#include "esp_err.h"

esp_err_t handle_on_off_message(const esp_zb_zcl_set_attr_value_message_t *message);
esp_err_t handle_color_control_message(const esp_zb_zcl_set_attr_value_message_t *message);
esp_err_t handle_level_control_message(const esp_zb_zcl_set_attr_value_message_t *message);

#endif /* ESP_ZB_MESSAGE_HANDLERS_H */ 