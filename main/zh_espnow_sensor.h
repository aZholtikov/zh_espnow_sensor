#pragma once

#include "stdio.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_sleep.h"
#include "zh_ds18b20.h"
#include "zh_dht.h"
#include "zh_config.h"

#ifdef CONFIG_NETWORK_TYPE_DIRECT
#include "zh_espnow.h"
#define zh_send_message(a, b, c) zh_espnow_send(a, b, c)
#define ZH_EVENT ZH_ESPNOW
#else
#include "zh_network.h"
#define zh_send_message(a, b, c) zh_network_send(a, b, c)
#define ZH_EVENT ZH_NETWORK
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ZH_CHIP_TYPE HACHT_ESP8266
#elif CONFIG_IDF_TARGET_ESP32
#define ZH_CHIP_TYPE HACHT_ESP32
#elif CONFIG_IDF_TARGET_ESP32S2
#define ZH_CHIP_TYPE HACHT_ESP32S2
#elif CONFIG_IDF_TARGET_ESP32S3
#define ZH_CHIP_TYPE HACHT_ESP32S3
#elif CONFIG_IDF_TARGET_ESP32C2
#define ZH_CHIP_TYPE HACHT_ESP32C2
#elif CONFIG_IDF_TARGET_ESP32C3
#define ZH_CHIP_TYPE HACHT_ESP32C3
#elif CONFIG_IDF_TARGET_ESP32C6
#define ZH_CHIP_TYPE HACHT_ESP32C6
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ZH_CPU_FREQUENCY CONFIG_ESP8266_DEFAULT_CPU_FREQ_MHZ;
#define get_app_description() esp_ota_get_app_description()
#else
#define ZH_CPU_FREQUENCY CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
#define get_app_description() esp_app_get_description()
#endif

#define ZH_SENSOR_ATTRIBUTES_MESSAGE_FREQUENCY 60 // Frequency of transmission of keep alive messages to the gateway (in seconds).

#define ZH_MESSAGE_TASK_PRIORITY 2 // Prioritize the task of sending messages to the gateway.
#define ZH_MESSAGE_STACK_SIZE 2048 // The stack size of the task of sending messages to the gateway.

typedef struct // Structure of data exchange between tasks, functions and event handlers.
{
    struct // Storage structure of sensor hardware configuration data.
    {
        ha_sensor_type_t sensor_type;   // Sensor types.
        uint8_t sensor_pin_1;           // Sensor GPIO number 1. @note Main pin for 1-wire sensors, SDA pin for I2C sensors.
        uint8_t sensor_pin_2;           // Sensor GPIO number 2. @note SCL pin for I2C sensors.
        uint8_t power_pin;              // Power GPIO number (if used sensor power control).
        uint16_t measurement_frequency; // Measurement frequency (sleep time on battery powering).
        bool battery_power;             // Battery powered. @note Battery powering (true) / external powering (false).
    } hardware_config;
    volatile bool gateway_is_available;      // Gateway availability status flag. @note Used to control the tasks when the gateway connection is established / lost. Used only when external powered.
    uint8_t gateway_mac[6];                  // Gateway MAC address. @note Used only when external powered.
    uint8_t sent_message_quantity;           // System counter for the number of sended messages. @note Used only when powered by battery.
    zh_dht_handle_t dht_handle;              // Unique DTH11/22 sensor handle.
    TaskHandle_t attributes_message_task;    // Unique task handle for zh_send_sensor_attributes_message_task(). @note Used only when external powered.
    TaskHandle_t status_message_task;        // Unique task handle for zh_send_sensor_status_message_task(). @note Used only when external powered.
    const esp_partition_t *update_partition; // Unique handle for next OTA update partition. @note Used only when external powered.
    esp_ota_handle_t update_handle;          // Unique handle for OTA functions. @note Used only when external powered.
    uint16_t ota_message_part_number;        // System counter for the number of received OTA messages. @note Used only when external powered.
} sensor_config_t;

/**
 * @brief Function for loading the sensor hardware configuration from NVS memory.
 *
 * @param[out] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_load_config(sensor_config_t *sensor_config);

/**
 * @brief Function for saving the sensor hardware configuration to NVS memory.
 *
 * @param[in] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_save_config(const sensor_config_t *sensor_config);

/**
 * @brief Function for loading the power selection GPIO number from NVS memory.
 *
 * @return Power selection GPIO number
 */
uint8_t zh_load_power_selection_pin(void);

/**
 * @brief Function for saving the power selection GPIO number to NVS memory.
 *
 * @param[in] power_selection_pin Power selection GPIO number
 */
void zh_save_power_selection_pin(const uint8_t *power_selection_pin);

/**
 * @brief Function for GPIO and sensor initialization.
 *
 * @param[in,out] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_sensor_init(sensor_config_t *sensor_config);

/**
 * @brief Function for sending sensor data to the gateway and putting the module into deep sleep.
 *
 * @note Used only when powered by battery.
 *
 * @param[in,out] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_sensor_deep_sleep(sensor_config_t *sensor_config);

/**
 * @brief Function for prepare the hardware configuration message and sending it to the gateway.
 *
 * @param[in] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_sensor_hardware_config_message(const sensor_config_t *sensor_config);

/**
 * @brief Task for prepare the attributes message and sending it to the gateway.
 *
 * @param[in] pvParameter Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_sensor_attributes_message_task(void *pvParameter);

/**
 * @brief Function for prepare the configuration message and sending it to the gateway.
 *
 * @param[in] sensor_config Pointer to the structure of data exchange between tasks, functions and event handlers.
 *
 * @return Number of messages to send
 */
uint8_t zh_send_sensor_config_message(const sensor_config_t *sensor_config);

/**
 * @brief Task for prepare the status message and sending it to the gateway.
 *
 * @param[in] pvParameter Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_send_sensor_status_message_task(void *pvParameter);

/**
 * @brief Function for ESP-NOW event processing.
 *
 * @param[in,out] arg Pointer to the structure of data exchange between tasks, functions and event handlers.
 */
void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);