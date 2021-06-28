#pragma once
#include <freertos/queue.h>
#include <freertos/task.h>
#include <iostream>
using std::string;
// logs
#include "esp_wifi.h"
#include <esp_log.h>

extern TaskHandle_t mqttHahdle;
void mqttTask(void *pvParam);
void mqtt_callback(std::string param, std::string value);

// Queue.
extern QueueHandle_t mqttQueue;
typedef struct {
  string name;
  string metric;
} mqttMessage;
