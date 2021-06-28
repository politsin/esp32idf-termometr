#pragma once
#include <ds18x20.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <iostream>
using std::string;

extern TaskHandle_t ds18b20;
void ds18b20Task(void *pvParam);

ds18x20_addr_t scanOneAddr(gpio_num_t addr);
string addrsToSting(ds18x20_addr_t addrs);
string temperatureToSting(float temperature);
