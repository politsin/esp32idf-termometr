#include "iot.h"
// mqtt.
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <cstring>
#include <esp_log.h>
using std::strcpy;
using std::string;
// MQTT.
// App.
static const char *IOT = "IOT";
#include "esp32mqtt.h"
EspMQTT mqtt;

Iot::Iot() {
  uint16_t debugLevel = 0;
  if (debugLevel) {
    mqtt.test = true;
    mqtt.debugLevel = debugLevel;
    mqtt.setAvailabilityInterval(5);
  }
  mqtt.setWiFi(WIFI_NAME, WIFI_PASS, WIFI_HOST);
  mqtt.setMqtt(MQTT_SERVER, MQTT_USER, MQTT_PASS);
  mqtt.setCommonTopics(MQTT_DEVICE, MQTT_REGISTRY);

  mqtt.start(true);
}

void Iot::start(std::function<void(string param, string value)> cBack) {
  this->callbackFunction = cBack;
  mqtt.setCallback(cBack);
  ESP_LOGI(IOT, "Startup..");
  // this->initIot = init;
  // if (this->initIot) {
  // }
}

/**
 * MQTT MESSAGE EVENT.
 *
 * TODO:
 * if ((char)payload[0] != '{') {JSON}
 * if ((char)param.at(0) == '$') {eapp.app(param, message);return;}
 * if (length >= 1024) {publishState("$error", "Message is too big");
 * uint8_t pos = mqtt.cmdTopicLength;
 * if ((char)topic[pos] == '$') { eapp.appInterrupt(topic, payload, len);
 */
void Iot::onMqttMessage(esp_mqtt_event_t *event) {
  string topic = string(event->topic, event->topic_len);
  string payload = string(event->data, event->data_len);
  // string param = topic.substr(mqtt.cmdTopicLength);
  // this->callbackFunction(param, payload);
}

void Iot::publishData(string data) {
  // mqtt.publish(mqtt.dataTopic, 0, true, data.c_str());
}

void Iot::publishState(string key, string value) {
  // string topic = string(mqtt.stateTopic) + "/" + key;
  // mqtt.publish(topic.c_str(), 1, true, value.c_str());
}

void Iot::publishEvent(string key, string metric) {
  string topic = mqtt.deviceEventsTopic.name + key;
  mqtt.publish(topic.c_str(), 0, true, metric.c_str());
}

void Iot::publishEvent(char *key, uint16_t metric) {
  char message[16];
  itoa(metric, message, 10);
  char topic[255];
  strcpy(topic, mqtt.deviceEventsTopic.name.c_str());
  strcat(topic, key);
  mqtt.publish(topic, 0, true, message);
}

void Iot::publishEvent(string key, uint16_t metric) {
  char message[16];
  itoa(metric, message, 10);
  string topic = mqtt.deviceEventsTopic.name + key;
  mqtt.publish(topic.c_str(), 0, true, message);
}

void Iot::publishEvent(string key, float metric) {
  if (metric > 0) {
    this->publishEvent(key, metric, false);
  }
}

void Iot::publishEvent(string key, float metric, bool force) {
  if (metric > 0 || force) {
    char message[16];
    itoa(metric, message, 10);
    string topic = mqtt.deviceEventsTopic.name + key;
    mqtt.publish(topic.c_str(), 0, true, message);
  }
}
