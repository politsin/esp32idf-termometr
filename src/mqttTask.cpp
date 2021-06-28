#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mqttTask.h>
// wifi
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "nvs_flash.h"
// extras
#include "iot.h"
#include "string.h"
#include <iostream>
using std::string;
static const char *MQTAG = "MQTT";
#define DEFAULT_SCAN_LIST_SIZE 16
// EspMQTT mqtt;
mqttMessage msg;
// #include "blinkTask.h"

// QueueHandle_t mqttQueue;

void mqttTask(void *pvParam) {
  vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
  Iot iot;
  iot.start(mqtt_callback);
  while (true) {
    if (xQueueReceive(mqttQueue, &msg, portMAX_DELAY) == pdTRUE) {
      ESP_LOGI(MQTAG, "mqtt [%s] push = %s", msg.name.c_str(),
               msg.metric.c_str());
      iot.publishEvent(msg.name, msg.metric);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Command callback.
void mqtt_callback(string param, string value) {
  uint16_t val = atoi(value.c_str());
  ESP_LOGW(MQTAG, "%s=%s [%d]\n", param.c_str(), value.c_str(), val);
}
