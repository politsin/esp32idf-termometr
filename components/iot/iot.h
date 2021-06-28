#ifndef iot_h
#define iot_h
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "mqtt_client.h"
#include <functional>
#include <stdio.h>
#include <string>
using std::string;

class Iot {
public:
  Iot();
  bool initIot = false;
  void start(std::function<void(string param, string value)> cBack);

  // Send.
  void publishData(string data);
  void publishState(string key, string value);
  void publishEvent(char *key, uint16_t metric);
  void publishEvent(string key, uint16_t metric);
  void publishEvent(string key, string metric);
  void publishEvent(string key, float metric);
  void publishEvent(string key, float metric, bool force);
  // Callbacks.
  // void callback(char *topic, char *payload, uint16_t length);
  void onMqttMessage(esp_mqtt_event_t *event);
  // Timers.
  // MqttClient.

private:
  std::function<void(string param, string value)> callbackFunction;
  // setup:
  void setup();
};
// extern Iot iot;

#endif /* !iot_h */
