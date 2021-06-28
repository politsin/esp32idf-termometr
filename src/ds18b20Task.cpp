#include "ds18b20Task.h"
#include <freertos/queue.h>
// ds18b20
#include "mqttTask.h"
#include <ds18x20.h>
static const gpio_num_t OUTER_GPIO = GPIO_NUM_5;
static const gpio_num_t INNER_GPIO = GPIO_NUM_18;
static const uint8_t ADDR_COUNT = 1;
static const uint8_t RESCAN_INTERVAL = 4;
static const uint32_t DS_DELAY_MS = 3000;
static const char *DSTAG = "ds18x20";

using std::string;
using std::to_string;

void ds18b20Task(void *pvParam) {

  gpio_set_pull_mode(OUTER_GPIO, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(INNER_GPIO, GPIO_PULLUP_ONLY);

  esp_err_t res;
  float temperature;
  while (true) {
    // Scan.
    ESP_LOGI(DSTAG, "Scan...");
    ds18x20_addr_t addr_outer = scanOneAddr(OUTER_GPIO);
    ds18x20_addr_t addr_inner = scanOneAddr(INNER_GPIO);

    // Measuring.
    for (int i = 0; i < RESCAN_INTERVAL; i++) {
      ESP_LOGI(DSTAG, "Measuring...");
      // Outer.
      if (addr_outer != DS18X20_ANY) {
        res = ds18x20_measure_and_read(OUTER_GPIO, addr_outer, &temperature);
        if (res != ESP_OK) {
          ESP_LOGE(DSTAG, "Could not read from sensor %08x%08x: %d (%s)",
                   (uint32_t)(addr_outer >> 32), (uint32_t)addr_outer, res,
                   esp_err_to_name(res));
        } else {
          string temp = temperatureToSting(temperature);
          mqttMessage msg = {string("To"), temp};
          ESP_LOGI(DSTAG, "To= %s°C", temp.c_str());
          xQueueSend(mqttQueue, &msg, (TickType_t)0);
        }
      }
      // Inner.
      if (addr_inner != DS18X20_ANY) {
        res = ds18x20_measure_and_read(INNER_GPIO, addr_inner, &temperature);
        if (res != ESP_OK) {
          ESP_LOGE(DSTAG, "Could not read from sensor %08x%08x: %d (%s)",
                   (uint32_t)(addr_inner >> 32), (uint32_t)addr_inner, res,
                   esp_err_to_name(res));
        } else {
          string temp = temperatureToSting(temperature);
          mqttMessage msg = {string("Ti"), temp};
          ESP_LOGI(DSTAG, "Ti= %s°C", temp.c_str());
          xQueueSend(mqttQueue, &msg, (TickType_t)0);
        }
      }
      vTaskDelay(pdMS_TO_TICKS(DS_DELAY_MS));
    }
  }
}

// Addr_t To String.
ds18x20_addr_t scanOneAddr(gpio_num_t addr) {
  uint8_t debug_level = 1;
  size_t counter = 0;
  ds18x20_addr_t addrs[ADDR_COUNT];
  esp_err_t res = ds18x20_scan_devices(addr, addrs, ADDR_COUNT, &counter);
  if (res != ESP_OK) {
    ds18x20_addr_t any = DS18X20_ANY;
    ESP_LOGE(DSTAG, "Sensors scan error %d (%s)", res, esp_err_to_name(res));
    return any;
  }
  ds18x20_addr_t address = addrs[0];
  if (debug_level > 0) {
    string name = addrsToSting(address);
    ESP_LOGI(DSTAG, "Found: %s", name.c_str());
  }
  return address;
}

// Addr_t To String.
string addrsToSting(ds18x20_addr_t addrs) {
  char data[128];
  string type = "DS18S20";
  if ((addrs & 0xff) == DS18B20_FAMILY_ID) {
    type = "DS18B20";
  }
  sprintf(data, "%08x%08x|%s", (uint32_t)(addrs >> 32), (uint32_t)addrs,
          type.c_str());
  string address = string(data);
  return address;
}

// Float To String.
string temperatureToSting(float temperature) {
  char data[16];
  sprintf(data, "%.3f", temperature);
  string metric = string(data);
  return metric;
}
