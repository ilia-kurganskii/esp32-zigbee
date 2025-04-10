#include "esp_zb_light.h"
#include "esp_check.h"
#include "esp_log.h"
#include "ha/esp_zigbee_ha_standard.h"

static const char *TAG = "ESP_ZB_MSG_HANDLERS";

/* Convert CIE xy color space to RGB */
static void xy_to_rgb(uint16_t x, uint16_t y, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float x_f = (float)x / 65535.0f;
    float y_f = (float)y / 65535.0f;
    
    /* Convert xy to XYZ */
    float z_f = 1.0f - x_f - y_f;
    float Y = 1.0f; // Full brightness
    float X = (Y / y_f) * x_f;
    float Z = (Y / y_f) * z_f;

    /* Convert XYZ to RGB */
    float R =  3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
    float G = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
    float B =  0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

    /* Clamp RGB values */
    R = R < 0 ? 0 : (R > 1 ? 1 : R);
    G = G < 0 ? 0 : (G > 1 ? 1 : G);
    B = B < 0 ? 0 : (B > 1 ? 1 : B);

    /* Convert to 8-bit RGB */
    *r = (uint8_t)(R * 255);
    *g = (uint8_t)(G * 255);
    *b = (uint8_t)(B * 255);
}

esp_err_t handle_on_off_message(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && 
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        bool light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : false;
        ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
        light_driver_set_power(light_state);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "On/Off cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t handle_color_control_message(const esp_zb_zcl_set_attr_value_message_t *message)
{
    uint16_t light_color_x = 0;
    uint16_t light_color_y = 0;
    uint8_t red, green, blue;

    if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID && 
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        light_color_x = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : 0;
        light_color_y = *(uint16_t *)esp_zb_zcl_get_attribute(message->info.dst_endpoint, message->info.cluster,
                                                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID)
                             ->data_p;
        ESP_LOGI(TAG, "Light color x changes to 0x%x", light_color_x);
    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID &&
               message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
        light_color_y = message->attribute.data.value ? *(uint16_t *)message->attribute.data.value : 0;
        light_color_x = *(uint16_t *)esp_zb_zcl_get_attribute(message->info.dst_endpoint, message->info.cluster,
                                                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID)
                             ->data_p;
    } else {
        ESP_LOGW(TAG, "Color control cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
        return ESP_ERR_INVALID_ARG;
    }

    xy_to_rgb(light_color_x, light_color_y, &red, &green, &blue);
    light_driver_set_rgb(red, green, blue);
    ESP_LOGI(TAG, "Light color changes to (0x%x, 0x%x, 0x%x)", red, green, blue);
    return ESP_OK;
}

esp_err_t handle_level_control_message(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && 
        message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
        uint8_t light_level = message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : 0;
        light_driver_set_level(light_level);
        ESP_LOGI(TAG, "Light level changes to %d", light_level);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Level Control cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
    return ESP_ERR_INVALID_ARG;
} 