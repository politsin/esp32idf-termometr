#include "main.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
// app.
#define MAINTAG "MAIN"
#include "ds18b20Task.h"
#include "mqttTask.h"

TaskHandle_t ds18b20;
TaskHandle_t mqttHahdle;
QueueHandle_t mqttQueue;
extern "C" void app_main(void) {
  uint32_t min = configMINIMAL_STACK_SIZE;
  ESP_LOGW(MAINTAG, "Hello world!");
  mqttQueue = xQueueCreate(10, sizeof(mqttMessage));
  xTaskCreate(&loop, "loop", 1024 * 3, NULL, 2, NULL);
  xTaskCreate(mqttTask, "mqtt", 1024 * 4, NULL, 1, &mqttHahdle);
  xTaskCreate(ds18b20Task, "ds18b20", 1024 * 6, NULL, 1, &ds18b20);

}

char data[16];
int16_t count = 0;
uint16_t loopDelay = 3; // sec.
void loop(void *params) {
  while (true) {
    count++;
    if ((count % 100) == true) {
      sprintf(data, "%d", count);
      mqttMessage msg = {"count", string(data)};
      // debug_tasks();
      xQueueSend(mqttQueue, &msg, (TickType_t)0);
    }
    vTaskDelay(loopDelay * 100 / portTICK_PERIOD_MS);
  }
}

void debug_tasks() {
  uint32_t freemem = xPortGetFreeHeapSize();
  ESP_LOGE(MAINTAG, "freemem %d", freemem);
  char debugBuff[512];
  vTaskList(debugBuff);
  ESP_LOGW(MAINTAG, "\n\t\tstate\tprior\tfree\tnumber\tcore  \n%s", debugBuff);
}
