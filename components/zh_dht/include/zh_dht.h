/**
 * @file
 * Header file for the zh_dht component.
 *
 */

#pragma once

#include "stdint.h"
#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif
	/**
	 * @brief Enumeration of supported sensor types.
	 *
	 */
	typedef enum
	{
		ZH_DHT11, ///< Sensor type DHT11
		ZH_DHT22  ///< Sensor type DHT22 or AM2302
	} __attribute__((packed)) zh_dht_sensor_type_t;

	/**
	 * @brief Unique handle of the sensor.
	 *
	 */
	typedef struct
	{
		uint8_t sensor_pin;				  ///< Sensor GPIO connection
		zh_dht_sensor_type_t sensor_type; ///< Sensor type
	} __attribute__((packed)) zh_dht_handle_t;

	/**
	 * @brief      Initialize DHT sensor.
	 *
	 * @param[in]  sensor_type  Sensor type (ZH_DHT11 or ZH_DHT22).
	 * @param[in]  sensor_pin   Sensor connection gpio.
	 *
	 * @return
	 *              - Handle of the sensor.
	 */
	zh_dht_handle_t zh_dht_init(const zh_dht_sensor_type_t sensor_type, const uint8_t sensor_pin);

	/**
	 * @brief      Read DHT sensor.
	 *
	 * @param[in]   dht_handle   Pointer for handle of the sensor.
	 * @param[out]  humidity     Pointer for DHT sensor reading data of humidity.
	 * @param[out]  temperature  Pointer for DHT sensor reading data of temperature.
	 *
	 * @return
	 *				- ESP_OK if read was success
	 *				- ESP_ERR_INVALID_ARG if parameter error
	 *				- ESP_ERR_INVALID_RESPONSE if the bus is busy
	 *				- ESP_ERR_TIMEOUT if operation timeout
	 *				- ESP_ERR_INVALID_CRC if check CRC is fail
	 */
	esp_err_t zh_dht_read(const zh_dht_handle_t *dht_handle, float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif