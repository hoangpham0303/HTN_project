#include <string.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include "app_priv.h"
#include "dht.h"
#include "esp_adc/adc_oneshot.h"
#include <math.h>

#define BUTTON2_GPIO            17
#define OUTPUT_GPIO_LED         4
#define OUTPUT_GPIO_LED2        5
#define DHT_GPIO                23
#define MQ135_ADC_CHANNEL       ADC_CHANNEL_6  // GPIO34
#define LIGHT_SENSOR_CHANNEL    ADC_CHANNEL_4  // GPIO32
#define LJ12A3_GPIO             21
#define BUZZER_PIN              19
#define RED_LED_GPIO            25
#define YELLOW_LED_GPIO         26
#define BLUE_LED_GPIO           27


typedef struct {
    float ppm;
    int16_t temperature;
    int16_t humidity;
    int light; 
    float light_percent; 
} sensor_event_t;


static esp_rmaker_param_t *g_led_param = NULL;
static esp_rmaker_param_t *g_led2_param = NULL;
static esp_rmaker_param_t *g_temp_param = NULL;
static esp_rmaker_param_t *g_humi_param = NULL;
static esp_rmaker_param_t *g_ppm_param = NULL;
static esp_rmaker_param_t *g_lj12_param = NULL;
static esp_rmaker_param_t *g_light_param = NULL;
static esp_rmaker_param_t *g_red_param = NULL;
static esp_rmaker_param_t *g_yellow_param = NULL;
static esp_rmaker_param_t *g_blue_param = NULL;

static bool current_led_state = false;
static bool current_led2_state = false;
static bool red_led_state = false;
static bool yellow_led_state = false;
static bool blue_led_state = false;

static SemaphoreHandle_t btn1_semaphore;
static SemaphoreHandle_t btn2_semaphore;
static SemaphoreHandle_t sensor_timer_semaphore;
static SemaphoreHandle_t lj12_semaphore;
static SemaphoreHandle_t gpio_mutex;    

static QueueHandle_t sensor_queue;
static adc_oneshot_unit_handle_t adc1_handle;

static float get_ppm(int raw_adc) {
    return raw_adc > 0 ? raw_adc / 20.0 : 0.0;
}

void app_driver_init_params(esp_rmaker_param_t *led, esp_rmaker_param_t *led2,
    esp_rmaker_param_t *temp, esp_rmaker_param_t *humi,
    esp_rmaker_param_t *ppm, esp_rmaker_param_t *lj12, esp_rmaker_param_t *light,
    esp_rmaker_param_t *red, esp_rmaker_param_t *yellow, esp_rmaker_param_t *blue) {
    g_led_param = led;
    g_led2_param = led2;
    g_temp_param = temp;
    g_humi_param = humi;
    g_ppm_param = ppm;
    g_lj12_param = lj12;
    g_light_param = light;
    g_red_param = red;
    g_yellow_param = yellow;
    g_blue_param = blue;
}

esp_err_t app_driver_set_gpio(const char *param_name, bool state) {
    if (xSemaphoreTake(gpio_mutex, pdMS_TO_TICKS(100))) {
        if (strcmp(param_name, "LED") == 0) {
            if (current_led_state != state) {
                gpio_set_level(OUTPUT_GPIO_LED, state);
                current_led_state = state;
            }
        } else if (strcmp(param_name, "LED2") == 0) {
            if (current_led2_state != state) {
                gpio_set_level(OUTPUT_GPIO_LED2, state);
                current_led2_state = state;
            }
        } else if (strcmp(param_name, "RED") == 0) {
            if (red_led_state != state) {
                gpio_set_level(RED_LED_GPIO , state);
                red_led_state = state;
            }
        } else if (strcmp(param_name, "YELLOW") == 0) {
            if (yellow_led_state != state) {
                gpio_set_level(YELLOW_LED_GPIO , state);
                yellow_led_state = state;
            }
        } else if (strcmp(param_name, "BLUE") == 0) {
            if (blue_led_state != state) {
                gpio_set_level(BLUE_LED_GPIO, state);
                blue_led_state = state;
            }
        }
        xSemaphoreGive(gpio_mutex);
    }
    return ESP_OK;
}

void IRAM_ATTR button2_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(btn2_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void IRAM_ATTR lj12_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(lj12_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void button_handler_task(void *arg) {
    while (1) {
        if (xSemaphoreTake(btn2_semaphore, 0) == pdTRUE) {
            app_driver_set_gpio("LED2", !current_led2_state);
            esp_rmaker_param_update_and_report(g_led2_param, esp_rmaker_bool(current_led2_state));
            ESP_LOGI("LED2", "%s", current_led2_state ? "ON" : "OFF");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void sensor_timer_callback(TimerHandle_t xTimer) {
    xSemaphoreGive(sensor_timer_semaphore);
}

static void update_ppm_leds(float ppm) {
    bool red = false, yellow = false, blue = false;

    if (ppm > 100.0f) {
        red = true;
    } else if (ppm < 55.0f) {
        blue = true;
    } else {
        yellow = true;
    }

    gpio_set_level(RED_LED_GPIO, red);
    gpio_set_level(YELLOW_LED_GPIO, yellow);
    gpio_set_level(BLUE_LED_GPIO, blue);

    red_led_state = red;
    yellow_led_state = yellow;
    blue_led_state = blue;

    if (g_red_param) esp_rmaker_param_update_and_report(g_red_param, esp_rmaker_bool(red));
    if (g_yellow_param) esp_rmaker_param_update_and_report(g_yellow_param, esp_rmaker_bool(yellow));
    if (g_blue_param) esp_rmaker_param_update_and_report(g_blue_param, esp_rmaker_bool(blue));

    
    if (red) {
        ESP_LOGI("PPM LED", "RED LED ON");
    } else if (yellow) {
        ESP_LOGI("PPM LED", "YELLOW LED ON");
    } else if (blue) {
        ESP_LOGI("PPM LED", "BLUE LED ON");
    }
}

static void sensor_task(void *arg) {
    sensor_event_t event;
    static float last_ppm = -1;
    static int16_t last_temp = -1;
    static int16_t last_humi = -1;
    static float last_light_percent = -1.0f;

    int raw_adc = 0;
    int raw_light = 0;

    while (1) {
        if (xSemaphoreTake(sensor_timer_semaphore, portMAX_DELAY)) {
            if (dht_read_data(DHT_TYPE_DHT11, DHT_GPIO, &event.humidity, &event.temperature) == ESP_OK) {
                if (abs(event.temperature - last_temp) >= 1) {
                    esp_rmaker_param_update_and_report(g_temp_param, esp_rmaker_float(event.temperature / 10.0));
                    last_temp = event.temperature;
                }
                if (abs(event.humidity - last_humi) >= 1) {
                    esp_rmaker_param_update_and_report(g_humi_param, esp_rmaker_float(event.humidity / 10.0));
                    last_humi = event.humidity;
                }
            } else {
                ESP_LOGW("DHT", "Failed to read from DHT11");
            }

            if (adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &raw_adc) == ESP_OK) {
                event.ppm = get_ppm(raw_adc);
                if (fabs(event.ppm - last_ppm) > 1.0f) {
                    esp_rmaker_param_update_and_report(g_ppm_param, esp_rmaker_float(event.ppm));
                    last_ppm = event.ppm;
                    update_ppm_leds(event.ppm);
                }
            }

            int sum = 0, samples = 10;
            for (int i = 0; i < samples; i++) {
                int val = 0;
                if (adc_oneshot_read(adc1_handle, LIGHT_SENSOR_CHANNEL, &val) == ESP_OK) {
                    sum += val;
                }
            }
            raw_light = sum / samples;

            float light_percent = (1.0f - ((float)raw_light / 4095.0f)) * 100.0f;
            event.light = raw_light - 4000;
            event.light_percent = light_percent;

            if (fabs(light_percent - last_light_percent) >= 10.0f) {
                esp_rmaker_param_update_and_report(g_light_param, esp_rmaker_float(light_percent));
                last_light_percent = light_percent;
            }

            if (light_percent < 50.0f && !current_led_state) {
                app_driver_set_gpio("LED", true);
                esp_rmaker_param_update_and_report(g_led_param, esp_rmaker_bool(true));
                ESP_LOGI("LED", "Bật LED1 do ánh sáng thấp");
            } else if (light_percent >= 50.0f && current_led_state) {
                app_driver_set_gpio("LED", false);
                esp_rmaker_param_update_and_report(g_led_param, esp_rmaker_bool(false));
                ESP_LOGI("LED", "Tắt LED1 do ánh sáng đủ");
            }

            xQueueSend(sensor_queue, &event, portMAX_DELAY);
        }
    }
}

static void lj12_task(void *arg) {
    
    static bool last_lj12 = false;
    while (1) {
        if (xSemaphoreTake(lj12_semaphore, portMAX_DELAY)) {
            bool current_lj12 = gpio_get_level(LJ12A3_GPIO) == 0;
            if (current_lj12 != last_lj12) {
                gpio_set_level(BUZZER_PIN, current_lj12 ? 1 : 0);
                esp_rmaker_param_update_and_report(g_lj12_param,
                    esp_rmaker_str(current_lj12 ? "Detected" : "Undetected"));
                
                ESP_LOGI("LJ12", "State: %s", current_lj12 ? "Detected" : "Undetected");
                last_lj12 = current_lj12;
            }
        }
    }
}

static void sensor_receiver_task(void *arg) {
    sensor_event_t event;
   
  
    while (1) {
        if (xQueueReceive(sensor_queue, &event, portMAX_DELAY)) {
           

            ESP_LOGI("SENSOR", "PPM: %.2f | Temp: %d.%d°C | Humidity: %d.%d%% | Light: %.1f%%",
            event.ppm,
            event.temperature / 10, abs(event.temperature % 10),
            event.humidity / 10, abs(event.humidity % 10),
            event.light_percent );  

        }
    }
}

void app_driver_init(void) {
    esp_log_level_set("esp_rmaker_param", ESP_LOG_WARN);
    esp_log_level_set("app_main", ESP_LOG_WARN);

    gpio_set_direction(OUTPUT_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(OUTPUT_GPIO_LED2, GPIO_MODE_OUTPUT);
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(YELLOW_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_direction(BUTTON2_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON2_GPIO, GPIO_PULLUP_ONLY);

    gpio_set_direction(LJ12A3_GPIO, GPIO_MODE_INPUT);
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(DHT_GPIO);
    gpio_set_direction(DHT_GPIO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);

    sensor_queue = xQueueCreate(10, sizeof(sensor_event_t));


    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1
    };
    adc_oneshot_new_unit(&adc_config, &adc1_handle);

    adc_oneshot_chan_cfg_t adc_channel_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    adc_oneshot_config_channel(adc1_handle, MQ135_ADC_CHANNEL, &adc_channel_cfg);

    adc_oneshot_chan_cfg_t light_config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc1_handle, LIGHT_SENSOR_CHANNEL, &light_config);

    gpio_mutex = xSemaphoreCreateMutex();
 
    btn2_semaphore = xSemaphoreCreateBinary();
    lj12_semaphore = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);
    gpio_set_intr_type(BUTTON2_GPIO, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(LJ12A3_GPIO, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(BUTTON2_GPIO, button2_isr_handler, NULL);
    gpio_isr_handler_add(LJ12A3_GPIO, lj12_isr_handler, NULL);
 
    sensor_timer_semaphore = xSemaphoreCreateBinary();

    
    TimerHandle_t sensor_timer = xTimerCreate("sensor_timer", pdMS_TO_TICKS(7000), pdTRUE, NULL, sensor_timer_callback);
    if (sensor_timer != NULL) {
        xTimerStart(sensor_timer, 0);
    } else {
        ESP_LOGE("TIMER", "Failed to create software timer");
    }
  
    xTaskCreatePinnedToCore(button_handler_task, "button_handler_task", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(sensor_receiver_task, "sensor_receiver_task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(lj12_task, "lj12_task", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", 4096, NULL, 1, NULL, 1);

}
