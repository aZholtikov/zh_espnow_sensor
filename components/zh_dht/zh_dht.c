/**
 * @file
 * The main code of the zh_dht component.
 *
 */

#include "zh_dht.h"

/// \cond
#define BIT_1_TRANSFER_MAX_DURATION 75			// Signal "1" high time.
#define BIT_0_TRANSFER_MAX_DURATION 30			// Signal "0" high time.
#define DATA_BIT_START_TRANSFER_MAX_DURATION 55 // Signal "0", "1" low time.
#define RESPONSE_MAX_DURATION 85				// Response to low time. Response to high time.
#define MASTER_RELEASE_MAX_DURATION 200			// Bus master has released time.
#define DATA_SIZE 40

#ifdef CONFIG_IDF_TARGET_ESP8266
#define esp_delay_us(x) os_delay_us(x)
#else
#define esp_delay_us(x) esp_rom_delay_us(x)
#endif
/// \endcond

static const char *TAG = "zh_dht";

static esp_err_t _read_bit(const zh_dht_handle_t *dht_handle, bool *bit);

zh_dht_handle_t zh_dht_init(const zh_dht_sensor_type_t sensor_type, const uint8_t sensor_pin)
{
	ESP_LOGI(TAG, "DHT initialization begin.");
	zh_dht_handle_t zh_dht_handle = {
		.sensor_type = sensor_type,
		.sensor_pin = sensor_pin};
	gpio_config_t config = {0};
	config.intr_type = GPIO_INTR_DISABLE;
	config.mode = GPIO_MODE_INPUT;
	config.pin_bit_mask = (1ULL << sensor_pin);
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.pull_up_en = GPIO_PULLUP_ENABLE;
	if (gpio_config(&config) != ESP_OK)
	{
		zh_dht_handle.sensor_pin = 0xFF;
		ESP_LOGE(TAG, "DHT initialization fail. Incorrect GPIO number.");
		return zh_dht_handle;
	}
	ESP_LOGI(TAG, "DHT initialization success.");
	return zh_dht_handle;
}

esp_err_t zh_dht_read(const zh_dht_handle_t *dht_handle, float *humidity, float *temperature)
{
	ESP_LOGI(TAG, "DHT read begin.");
	if (dht_handle == NULL || humidity == NULL || temperature == NULL || dht_handle->sensor_pin == 0xFF)
	{
		ESP_LOGE(TAG, "DHT read fail. Invalid argument.");
		return ESP_ERR_INVALID_ARG;
	}
	if (gpio_get_level(dht_handle->sensor_pin) != 1)
	{
		ESP_LOGE(TAG, "DHT read fail. Bus is busy.");
		return ESP_ERR_INVALID_RESPONSE;
	}
	gpio_set_direction(dht_handle->sensor_pin, GPIO_MODE_OUTPUT);
	gpio_set_level(dht_handle->sensor_pin, 0);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	gpio_set_level(dht_handle->sensor_pin, 1);
	gpio_set_direction(dht_handle->sensor_pin, GPIO_MODE_INPUT);
	uint8_t time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > MASTER_RELEASE_MAX_DURATION)
		{
			ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 0)
	{
		if (time > RESPONSE_MAX_DURATION)
		{
			ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > RESPONSE_MAX_DURATION)
		{
			ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	uint8_t dht_data[DATA_SIZE / 8] = {0};
	uint8_t byte_index = 0;
	uint8_t bit_index = 7;
	for (uint8_t i = 0; i < 40; ++i)
	{
		bool bit = 0;
		if (_read_bit(dht_handle, &bit) != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
			return ESP_ERR_TIMEOUT;
		}
		dht_data[byte_index] |= (bit << bit_index);
		if (bit_index == 0)
		{
			bit_index = 7;
			++byte_index;
		}
		else
		{
			--bit_index;
		}
	}
	if (dht_data[4] != ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3])))
	{
		ESP_LOGE(TAG, "DHT read fail. Invalid CRC.");
		return ESP_ERR_INVALID_CRC;
	}
	*humidity = (dht_data[0] << 8 | dht_data[1]) / 10.0;
	if (dht_handle->sensor_type == ZH_DHT22)
	{
		*temperature = ((dht_data[2] & 0b01111111) << 8 | dht_data[3]) / 10.0;
		if ((dht_data[2] & 0b10000000) != 0)
		{
			*temperature *= -1;
		}
	}
	else
	{
		*temperature = (dht_data[2] << 8 | dht_data[3]) / 10.0;
	}
	ESP_LOGI(TAG, "DHT read success.");
	return ESP_OK;
}

static esp_err_t _read_bit(const zh_dht_handle_t *dht_handle, bool *bit)
{
	uint8_t time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 0)
	{
		if (time > DATA_BIT_START_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > BIT_1_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	*bit = (time > BIT_0_TRANSFER_MAX_DURATION) ? 1 : 0;
	return ESP_OK;
}