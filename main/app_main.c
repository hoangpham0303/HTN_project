#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <app_network.h>
#include <app_insights.h>
#include "app_priv.h"

static const char *TAG = "app_main";

static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (app_driver_set_gpio(esp_rmaker_param_get_name(param), val.val.b) == ESP_OK) {
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}

void app_main(void) {
    app_driver_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_network_init();

    esp_rmaker_config_t rainmaker_cfg = { .enable_time_sync = true };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP GPIO Controller", "ESP32_Device");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

//  LED
esp_rmaker_device_t *led_device = esp_rmaker_device_create("LED", ESP_RMAKER_DEVICE_LIGHTBULB , NULL);
esp_rmaker_param_t *LED_param = esp_rmaker_param_create("LED", NULL, esp_rmaker_bool(false), PROP_FLAG_READ );
esp_rmaker_param_add_ui_type(LED_param, ESP_RMAKER_UI_TOGGLE);
esp_rmaker_device_add_param(led_device, LED_param);
esp_rmaker_node_add_device(node, led_device);
esp_rmaker_device_assign_primary_param(led_device, LED_param);

//  LED2
esp_rmaker_device_t *led2_device = esp_rmaker_device_create("LED2", ESP_RMAKER_DEVICE_LIGHTBULB , NULL);
esp_rmaker_device_add_cb(led2_device, write_cb, NULL);
esp_rmaker_param_t *LED2_param = esp_rmaker_param_create("LED2", NULL, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
esp_rmaker_param_add_ui_type(LED2_param, ESP_RMAKER_UI_TOGGLE);
esp_rmaker_device_add_param(led2_device, LED2_param);
esp_rmaker_node_add_device(node, led2_device);
esp_rmaker_device_assign_primary_param(led2_device, LED2_param);

// Temperature
esp_rmaker_device_t *temp_device = esp_rmaker_device_create("Temperature", ESP_RMAKER_DEVICE_TEMP_SENSOR, NULL);
esp_rmaker_param_t *temp_param = esp_rmaker_param_create("Temperature", NULL, esp_rmaker_float(0),PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(temp_param, ESP_RMAKER_UI_TEXT);
esp_rmaker_device_add_param(temp_device, temp_param);
esp_rmaker_node_add_device(node, temp_device);
esp_rmaker_device_assign_primary_param(temp_device, temp_param);

//  Humidity
esp_rmaker_device_t *humi_device = esp_rmaker_device_create("Humidity", NULL, NULL);
esp_rmaker_param_t *humi_param = esp_rmaker_param_create("Humidity", NULL, esp_rmaker_float(0),PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(humi_param, ESP_RMAKER_UI_TEXT);
esp_rmaker_device_add_param(humi_device, humi_param);
esp_rmaker_node_add_device(node, humi_device);
esp_rmaker_device_assign_primary_param(humi_device, humi_param);


//  MQ135
// MQ135 device with PPM value and 3 LED status parameters
esp_rmaker_device_t *ppm_device = esp_rmaker_device_create("MQ135", NULL, NULL);
esp_rmaker_device_add_cb(ppm_device, write_cb, NULL);

// PPM value (text)
esp_rmaker_param_t *ppm_param = esp_rmaker_param_create("MQ135", NULL, esp_rmaker_float(0), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(ppm_param, ESP_RMAKER_UI_TEXT);
esp_rmaker_device_add_param(ppm_device, ppm_param);

// RED LED status (read-only toggle)
esp_rmaker_param_t *red_param = esp_rmaker_param_create("RED", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(red_param, ESP_RMAKER_UI_TOGGLE);
esp_rmaker_device_add_param(ppm_device, red_param);

// YELLOW LED status (read-only toggle)
esp_rmaker_param_t *yellow_param = esp_rmaker_param_create("YELLOW", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(yellow_param, ESP_RMAKER_UI_TOGGLE);
esp_rmaker_device_add_param(ppm_device, yellow_param);

// BLUE LED status (read-only toggle)
esp_rmaker_param_t *blue_param = esp_rmaker_param_create("BLUE", NULL, esp_rmaker_bool(false), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(blue_param, ESP_RMAKER_UI_TOGGLE);
esp_rmaker_device_add_param(ppm_device, blue_param);

esp_rmaker_node_add_device(node, ppm_device);
esp_rmaker_device_assign_primary_param(ppm_device, ppm_param);


//  LJ12A3
esp_rmaker_device_t *lj_device = esp_rmaker_device_create("Metal Detector", NULL, NULL);
esp_rmaker_param_t *lj_param = esp_rmaker_param_create("Metal Detection", NULL,esp_rmaker_str("None"), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(lj_param, ESP_RMAKER_UI_TEXT);
esp_rmaker_device_add_param(lj_device, lj_param);
esp_rmaker_node_add_device(node, lj_device);
esp_rmaker_device_assign_primary_param(lj_device, lj_param);

// light sensorsensor
esp_rmaker_device_t *light_device = esp_rmaker_device_create("Light Sensor", NULL, NULL);
esp_rmaker_param_t *light_param = esp_rmaker_param_create("Light (ADC)", NULL, esp_rmaker_float(0), PROP_FLAG_READ);
esp_rmaker_param_add_ui_type(light_param, ESP_RMAKER_UI_TEXT);
esp_rmaker_device_add_param(light_device, light_param);
esp_rmaker_node_add_device(node, light_device);
esp_rmaker_device_assign_primary_param(light_device, light_param);

// Khởi tạo driver
app_driver_init_params(LED_param, LED2_param,
                       temp_param, humi_param,
                       ppm_param, lj_param, light_param,
                       red_param, yellow_param, blue_param);



    esp_rmaker_ota_enable_default();
    app_insights_enable();
    esp_rmaker_start();

    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
}