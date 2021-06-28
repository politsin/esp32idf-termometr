#pragma once
// Log.
#define TAG "LOG"
#include <esp_log.h>
#include <iostream>
using std::string;

void debug_tasks();
void loop(void *params);
