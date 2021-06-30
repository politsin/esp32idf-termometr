#include "esp32mqtt.h"
// mqtt.
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include <cstring>
#include <esp_log.h>
using std::strcpy;
using std::string;
#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"
// MQTT.
// Ticker.
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;
TimerHandle_t mqttAvailabilityTimer;
// App.
static const char *MQLIB = "MQTT/LIB";
static const char *MQPROTO = "MQTT/PROTO";

void EspMQTT::setWiFi(string ssid, string pass, string host) {
  strcpy(this->WiFiSsid, ssid.c_str());
  strcpy(this->WiFiPass, pass.c_str());
  strcpy(this->WiFiHost, host.c_str());
};

void EspMQTT::setMqtt(string server, string user, string pass) {
  uint8_t mac[6];
  char device[255];
  esp_efuse_mac_get_default(mac);
  uint16_t rand = esp_random() >> 2;
  snprintf(device, 255, "ESP32-" CHIP "-%02x", CHIP2STR(mac), rand);
  strcpy(this->mqttDevice, device);
  strcpy(this->mqttHost, server.c_str());
  strcpy(this->mqttUser, user.c_str());
  strcpy(this->mqttPass, pass.c_str());
};

void EspMQTT::setCommonTopics(string device, string registry) {
  // Root: /home/[sensor/switch/light]/{name}
  string d = string("$devices/") + device;
  string r = string("$registries/") + registry;
  strcpy(this->topicDevice, d.c_str());
  strcpy(this->topicRegistry, r.c_str());

  // Iot-core.
  this->deviceStateTopic = {
      .name = d + string("/state/"),
      .qos = MQTT_QOS_0,
  };
  this->deviceEventsTopic = {
      .name = d + string("/events/"),
      .qos = MQTT_QOS_0,
  };
  this->deviceConfigTopic = {
      .name = d + string("/config/*"),
      .qos = MQTT_QOS_0,
  };
  this->deviceCommandsTopic = {
      .name = d + string("/commands/*"),
      .qos = MQTT_QOS_0,
  };
  this->registryStateTopic = {
      .name = r + string("/state/"),
      .qos = MQTT_QOS_0,
  };
  this->registryEventsTopic = {
      .name = r + string("/events/"),
      .qos = MQTT_QOS_0,
  };
  this->registryConfigTopic = {
      .name = r + string("/config/*"),
      .qos = MQTT_QOS_0,
  };
  this->registryCommandsTopic = {
      .name = r + string("/commands/*"),
      .qos = MQTT_QOS_0,
  };
  // Info.
  string availability = this->deviceStateTopic.name + "availability";
  string ip = availability + string("/$ip");
  strcpy(this->availabilityTopic, availability.c_str());
  strcpy(this->ipTopic, ip.c_str());
};

void EspMQTT::start(bool init) {
  this->initMqtt = init;
  if (this->initMqtt) {
    this->online = false;
    gpio_pad_select_gpio(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, LED_LOW);
    ESP_LOGI(MQLIB, "Startup..");
    ESP_LOGI(MQLIB, "IDF version: %s", esp_get_idf_version());
    ESP_LOGI(MQLIB, "Free memory: %d bytes", esp_get_free_heap_size());
    esp_log_level_set(MQLIB, ESP_LOG_WARN);
    esp_log_level_set(MQPROTO, ESP_LOG_WARN);
    if (this->debugLevel >= 2) {
      esp_log_level_set(MQLIB, ESP_LOG_INFO);
      esp_log_level_set(MQPROTO, ESP_LOG_INFO);
      esp_log_level_set("*", ESP_LOG_INFO);
      esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
      esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
      esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
      esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
      esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
      esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    }
    setupTimers();
    setupNvs();
    setupWifi();
    setupMqtt();
    connectToWifi();
  }
}
void EspMQTT::setupTimers() {
  mqttReconnectTimer =
      xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                   reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer =
      xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0,
                   reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  mqttAvailabilityTimer = xTimerCreate(
      "availabilityTimer", pdMS_TO_TICKS(this->availabilityInterval), pdTRUE,
      (void *)0, reinterpret_cast<TimerCallbackFunction_t>(availabilityTime));
}
void EspMQTT::setupNvs() {
  esp_err_t err = nvs_flash_init();
  esp_err_t esp_err_nvs_no_free_pages = 0x1100 + 0x0d;
  if (err == esp_err_nvs_no_free_pages) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}
void EspMQTT::setupWifi() {
  ESP_LOGI(MQPROTO, "Wifi configure");
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEvent, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEvent, NULL, &instance_got_ip));
  // wifi_sta_config_t sta = {WIFI_NAME, WIFI_PASS};
  wifi_sta_config_t sta;
  memset((void *)&sta, 0, sizeof(sta));
  memcpy(sta.ssid, WIFI_NAME, sizeof(WIFI_NAME));
  memcpy(sta.password, WIFI_PASS, sizeof(WIFI_PASS));
  wifi_config_t wifi_config = {.sta = sta};

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_LOGI(MQPROTO, "Wifi configure finished");
}

// MQTT.
void EspMQTT::setupMqtt() {
  // -D MQTT_ROOT=\"home/sensor\"
  ESP_LOGI(MQPROTO, "MQTT configure");
  // mqttClient.setWill(availabilityTopic, 0, true, "offline");
  //   .uri = "mqtt://mqtt.eclipse.org:1884",
  //   lwt_topic : this->availabilityTopic,
  //   lwt_msg : "offline"
  esp_mqtt_client_config_t mqtt_cfg = {
      .host = this->mqttHost,
      .port = this->mqttPort,
      .client_id = this->mqttDevice,
      .username = this->mqttUser,
      .password = this->mqttPass,
      .transport = MQTT_TRANSPORT_OVER_SSL,
  };
  this->mqttClient = esp_mqtt_client_init(&mqtt_cfg);
  // The last argument to pass data to the event handler.
  esp_mqtt_event_id_t event = (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID;
  esp_mqtt_client_register_event(this->mqttClient, event, mqttEvent, NULL);
  ESP_LOGI(MQPROTO, "MQTT configure finished");
}
void EspMQTT::mqttConnect() {
  ESP_LOGW(MQPROTO, "MQTT Connecting ...");
  esp_mqtt_client_start(this->mqttClient);
}
// Static/proxy functions:
void EspMQTT::connectToWifi() {
  ESP_LOGW(MQPROTO, "Wi-Fi Connecting ...");
  ESP_ERROR_CHECK(esp_wifi_start());
}
// Static/proxy functions:
void EspMQTT::connectToMqtt() { mqtt.mqttConnect(); }
// Static/proxy functions:
uint32_t EspMQTT::subscribe(mqtt_topic_t topic) {
  uint32_t msg_id = esp_mqtt_client_subscribe(this->mqttClient,
                                              topic.name.c_str(), topic.qos);
  ESP_LOGW(MQLIB, "subscribe %s | %d", topic.name.c_str(), msg_id);
  return msg_id;
}
uint32_t EspMQTT::publish(const char *topic, uint8_t qos, bool retain,
                          const char *payload) {
  if (this->online) {
    uint32_t msg_id = esp_mqtt_client_publish(this->mqttClient, topic, payload,
                                              0, qos, retain);
    ESP_LOGI(MQLIB, "%s [%s] q:%d/r:%d| %d", topic, payload, qos, retain,
             msg_id);
    return msg_id;
  } else {
    ESP_LOGE(MQLIB, "OFFLINE!! cannot publish [%s]", topic);
  }
  return 0;
}

// Static wifiEvent proxy.
void EspMQTT::wifiEvent(void *arg, esp_event_base_t base, int32_t event_id,
                        void *event_data) {
  if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  }
  if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_ip4_addr_t *ip = &event->ip_info.ip;
    ESP_ERROR_CHECK(esp_netif_set_hostname(event->esp_netif, mqtt.WiFiHost));
    mqtt.onWifiConnect(ip);
  }
  if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    mqtt.onWifiDisconnect();
  }
}
// WiFi event: connect.
void EspMQTT::onWifiConnect(esp_ip4_addr_t *ip) {
  mqtt.wifiConnected = true;
  char ip_addr[255];
  snprintf(ip_addr, 255, IPSTR, IP2STR(ip));
  strcpy(mqtt.ip, ip_addr);
  ESP_LOGW(MQPROTO, "Connected to Wi-Fi. IP:%s", mqtt.ip);
  connectToMqtt();
}
// WiFi event: disconnect.
void EspMQTT::onWifiDisconnect() {
  mqtt.wifiConnected = false;
  mqtt.setOffline();
  xTimerStop(mqttAvailabilityTimer, 0);
  xTimerStop(mqttReconnectTimer, 0);
  xTimerStart(wifiReconnectTimer, 0);
  ESP_LOGW(MQPROTO, "Disconnected from Wi-Fi.");
}
// MQTT event / static.
void EspMQTT::mqttEvent(void *arg, esp_event_base_t base, int32_t event_id,
                        void *event_data) {
  ESP_LOGD(MQLIB, "Event dispatched from event loop base=%s, event_id=%d", base,
           event_id);
  // esp_mqtt_client_handle_t client = event->client;
  esp_mqtt_event_t *event = (esp_mqtt_event_t *)event_data;
  uint32_t msg_id = (uint32_t)event->msg_id;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    mqtt.onMqttConnect(event->session_present);
    break;
  case MQTT_EVENT_DISCONNECTED:
    mqtt.onMqttDisconnect(event_data);
    break;
  case MQTT_EVENT_SUBSCRIBED: {
    mqtt.onMqttSubscribe(msg_id);
    break;
  }
  case MQTT_EVENT_UNSUBSCRIBED:
    mqtt.onMqttUnsubscribe(msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    mqtt.onMqttPublish(msg_id);
    break;
  case MQTT_EVENT_DATA: {
    mqtt.onMqttMessage(event);
    break;
  }
  case MQTT_EVENT_ERROR:
    ESP_LOGI(MQLIB, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      mqtt.logErrorIfNonzero("reported from esp-tls",
                             event->error_handle->esp_tls_last_esp_err);
      mqtt.logErrorIfNonzero("reported from tls stack",
                             event->error_handle->esp_tls_stack_err);
      mqtt.logErrorIfNonzero("captured as transport's socket errno",
                             event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(MQPROTO, "Last errno string (%s)",
               strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(MQPROTO, "Other event id:%d", event->event_id);
    break;
  }
}
// Helper funcion / MQTT_EVENT_ERROR / TCP_TRANSPORT.
void EspMQTT::logErrorIfNonzero(const char *message, int error_code) {
  if (error_code != 0) {
    ESP_LOGE(MQPROTO, "Last error %s: 0x%x", message, error_code);
  }
}
// Mqtt event Connect.
void EspMQTT::onMqttConnect(bool sessionPresent) {
  this->setOnline();
  gpio_set_level(LED, LED_HIGH);
  xTimerStart(mqttAvailabilityTimer, 0);
  ESP_LOGW(MQPROTO, "MQTT connected, Session present:%d", sessionPresent);
}
void EspMQTT::setOnline() {
  this->online = true;
  this->publishAvailability();
  this->subscribe(this->deviceConfigTopic);
  this->subscribe(this->deviceCommandsTopic);
  this->subscribe(this->registryConfigTopic);
  this->subscribe(this->registryCommandsTopic);
  ESP_LOGW(MQPROTO, "ONLINE!");
  if (this->debugLevel >= 1) {
    this->mqttTests();
  }
}
void EspMQTT::setOffline() {
  this->online = false;
  gpio_set_level(LED, LED_LOW);
}

void EspMQTT::mqttTests() {
  if (this->test) {
    ESP_LOGI(MQLIB, "mqttTests");
    mqtt_topic_t testTopic = {
        .name = this->topicDevice + string("/test/test1"),
        .qos = MQTT_QOS_0,
    };
    const char *topic = testTopic.name.c_str();
    ESP_LOGE(MQLIB, "--== Statr Tests == --");
    ESP_LOGE(MQLIB, "TOPIC: %s", topic);
    uint32_t packetIdSub = this->subscribe(testTopic);
    ESP_LOGE(MQLIB, "T0:  Subscribing at QoS 2, packetId: %d", packetIdSub);
    this->publish(topic, 0, true, "test 1");
    ESP_LOGE(MQLIB, "T1:  Publishing at QoS 0");
    uint32_t packetIdPub1 = this->publish(topic, 1, true, "test 2");
    ESP_LOGE(MQLIB, "Т2:  Publishing at QoS 1, packetId: %d", packetIdPub1);
    uint32_t packetIdPub2 = this->publish(topic, 2, true, "test 3");
    ESP_LOGE(MQLIB, "Т3:  Publishing at QoS 2, packetId: %d", packetIdPub2);
    ESP_LOGE(MQLIB, "--== End Tests Inits == --");
  }
}

void EspMQTT::setAvailabilityInterval(uint16_t sec, bool onSetup) {
  uint32_t ms = (uint32_t)sec * 1000;
  this->availabilityInterval = ms;
  if (onSetup) {
    xTimerStop(mqttAvailabilityTimer, 0);
    xTimerChangePeriod(mqttAvailabilityTimer, pdMS_TO_TICKS(ms), 0);
  }
  if (this->online) {
    xTimerStart(mqttAvailabilityTimer, 0);
  }
  if (this->debugLevel >= 2) {
    ESP_LOGI(MQLIB, "Availability Interval=%d ms", ms);
  }
}

void EspMQTT::onMqttDisconnect(void *event_data) {
  this->setOffline();
  if (mqtt.wifiConnected) {
    xTimerStop(mqttAvailabilityTimer, 0);
    xTimerStart(mqttReconnectTimer, 0);
  }
  ESP_LOGE(MQPROTO, "Disconnected from MQTT");
}

void EspMQTT::onMqttSubscribe(uint32_t packetId) {
  ESP_LOGW(MQLIB, "Subscribed: packetId=%d", packetId);
}

void EspMQTT::onMqttUnsubscribe(uint32_t packetId) {
  ESP_LOGW(MQLIB, "Unsubscribed: packetId=%d", packetId);
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

void EspMQTT::onMqttMessage(esp_mqtt_event_t *event) {
  string topic = string(event->topic, event->topic_len);
  string payload = string(event->data, event->data_len);
  string param = topic.substr(strlen(mqtt.topicDevice));
  this->callbackFunction(param, payload);
  ESP_LOGW(MQLIB, "MQTT [%s] %s=%s", topic.c_str(), param.c_str(),
           payload.c_str());
  if (mqtt.test) {
    ESP_LOGI(MQLIB, "Publish received.");
    ESP_LOGI(MQLIB, "\ttopic: %s", topic.c_str());
    ESP_LOGI(MQLIB, "\tmessage: %d", event->msg_id);
    // ESP_LOGI(MQLIB, "\tQoS=%d \t| dup=%d   \t| retain=%d", prop.qos, prop.dup,
    //          prop.retain);
    // ESP_LOGI(MQLIB, "\tlen=%d \t| index=%d \t| total=%d", len, index, total);
  }
}

void EspMQTT::onMqttPublish(uint32_t packetId) {
  // ESP_LOGI(MQLIB, "Publish acknowledged. packetId:%i", packetId);
  // ESP_LOGI(MQLIB, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
}

void EspMQTT::setCallback(
    std::function<void(string param, string value)> cBack) {
  this->callbackFunction = cBack;
};

void EspMQTT::setDebugLevel(uint8_t debugLevel) {
  this->debugLevel = debugLevel;
  if (this->debugLevel) {
    ESP_LOGI(MQLIB, "debugLevel > ON %d", debugLevel);
  }
}

void EspMQTT::availabilityTime() { mqtt.publishAvailability(); }

void EspMQTT::publishAvailability() {
  this->publish(ipTopic, 0, true, this->ip);
  this->publish(availabilityTopic, 0, true, "online");
  ESP_LOGI(MQLIB, "[Publish Availability] at %s", ip);
}
