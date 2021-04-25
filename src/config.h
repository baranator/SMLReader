#ifndef CONFIG_H
#define CONFIG_H

#include "Arduino.h"
#include "Sensor.h"

const char *VERSION = "2.1.6";

// Modifying the config version will probably cause a loss of the existig configuration.
// Be careful!
const char *CONFIG_VERSION = "1.0.2";

const char *WIFI_AP_SSID = "SMLReader";
const char *WIFI_AP_DEFAULT_PASSWORD = "";

// static SensorConfig SENSOR_CONFIGS[] = {
//     {.sml_in_pin = D2,
//      .s0_out_pin = D3,
//      .name = "1",
//      .numeric_only = false,
//      .status_led_enabled = true,
//      .status_led_inverted = true,
//      .status_led_pin = LED_BUILTIN,
//      .interval = 0
//     }};

const uint8_t NUM_OF_SENSORS = 2;// sizeof(SENSOR_CONFIGS) / sizeof(SensorConfig);

#endif