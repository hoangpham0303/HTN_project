/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

void app_driver_init(void);
esp_err_t app_driver_set_gpio(const char *name, bool state);
void app_driver_init_params(esp_rmaker_param_t *led, esp_rmaker_param_t *led2,
    esp_rmaker_param_t *temp, esp_rmaker_param_t *humi,
    esp_rmaker_param_t *ppm, esp_rmaker_param_t *lj12, esp_rmaker_param_t *light,
    esp_rmaker_param_t *red, esp_rmaker_param_t *yellow, esp_rmaker_param_t *blue);
