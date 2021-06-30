#ifndef Mqtt_h
#define Mqtt_h
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "mqtt_client.h"
#include <functional>
#include <stdio.h>
#include <string>
using std::string;

// TODO:
#define LED GPIO_NUM_22
#define LED_LOW false
#define LED_HIGH true
// %CHIPID% are last 3 bytes of MAC address
#define CHIP "%02X:%02X:%02X"
#define CHIP2STR(mac) mac[3], mac[4], mac[5]

typedef enum {
  MQTT_QOS_0 = 0,
  MQTT_QOS_1 = 1,
  MQTT_QOS_2,
} mqtt_qos_t;

typedef struct {
string name;
mqtt_qos_t qos;
} mqtt_topic_t;

class EspMQTT {
public:
bool test = false;
bool online = false;
bool wifiConnected = false;
int8_t debugLevel = 0;
// App.
char device[255];
char registry[255];

// Set.
char WiFiSsid[255];
char WiFiPass[255];
char WiFiHost[255];
// Config.
char mqttHost[255];
char mqttUser[255];
char mqttPass[255];
char mqttDevice[255];
const uint16_t mqttPort = 8883;

char ip[255];
// Topics.
// Info.
char availabilityTopic[255];
char ipTopic[255];
char testTopic[255];
// Iot Core.
char topicDevice[255];
char topicRegistry[255];
mqtt_topic_t deviceStateTopic;
mqtt_topic_t deviceEventsTopic;
mqtt_topic_t deviceConfigTopic;
mqtt_topic_t deviceCommandsTopic;
mqtt_topic_t registryStateTopic;
mqtt_topic_t registryEventsTopic;
mqtt_topic_t registryConfigTopic;
mqtt_topic_t registryCommandsTopic;

// Cmd & Data

// start
void start(bool init = true);
void setDebugLevel(uint8_t debugLevel);
void setOta(bool ota);
// WiFi setters.
void setWiFi(string ssid, string pass, string host);
void setMqtt(string server, string user, string pass);
void setCommonTopics(string device, string registry);
// Mqtt setters.
void setstringValue(char *name, char *value);
void addSubsribeTopic(string topic);
// Loop.
void setAvailabilityInterval(uint16_t se, bool onSetup = false);

// Callbacks.
// void callback(char *topic, char *payload, uint16_t length);
void setCallback(std::function<void(string param, string value)> cBack);
// Timers.
// MqttClient.
void mqttClientSetup(bool proxy = true);
void connect();
uint32_t subscribe(mqtt_topic_t topic);
uint32_t publish(const char *topic, uint8_t qos, bool retain,
                const char *payload = nullptr);

// Online.
void mqttTests();
void setOnline();
void setOffline();
void mqttConnect();
void publishAvailability();

/*
* @brief Event handler registered to receive MQTT events
*
*  This function is called by the MQTT client event loop.
*
* @param handler_args user data registered to the event.
* @param base Event base for the handler(always MQTT Base in this example).
* @param event_id The id for the received event.
* @param event_data The data for the event, esp_mqtt_event_handle_t.
*/
  static void mqttEvent(void *arg, esp_event_base_t base, int32_t event_id,
                        void *event_data);
  static void wifiEvent(void *arg, esp_event_base_t base, int32_t event_id,
                        void *event_data);
  static void connectToWifi();
  static void connectToMqtt();
  static void availabilityTime();
  // Events.
  void onWifiDisconnect();
  void onWifiConnect(esp_ip4_addr_t * ip);
  void onMqttConnect(bool sessionPresent);
  void onMqttDisconnect(void *event_data);
  void onMqttSubscribe(uint32_t packetId);
  void onMqttUnsubscribe(uint32_t packetId);
  void onMqttMessage(esp_mqtt_event_t * event);
  void onMqttPublish(uint32_t packetId);

  private : bool initMqtt = true;
  uint32_t availabilityInterval = 30000;
  void callbackParceJson(string message);
  std::function<void(string param, string value)>
      callbackFunction;
  esp_mqtt_client_handle_t mqttClient;
  // setup:
  void setup();
  void setupNvs();
  void setupWifi();
  void setupMqtt();
  void setupTimers();
  void logErrorIfNonzero(const char *message, int error_code);
};
extern EspMQTT mqtt;

#endif /* !Mqtt_h */
