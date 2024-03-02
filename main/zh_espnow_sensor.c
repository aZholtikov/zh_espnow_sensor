#include "stdio.h"
#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "zh_espnow.h"
#include "zh_ds18b20.h"
#include "zh_dht.h"
#include "zh_config.h"

#define ZH_MESSAGE_TASK_PRIORITY 2
#define ZH_MESSAGE_STACK_SIZE 2048

static uint8_t s_ds18b20_pin = ZH_NOT_USED;
static uint8_t s_dht_pin = ZH_NOT_USED;
static uint8_t s_dht_sensor_type = ZH_DHT22;
static zh_dht_handle_t s_zh_dht_handle = {0};

static uint8_t s_gateway_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_gateway_is_available = false;

static TaskHandle_t s_ds18b20_attributes_message_task = {0};
static TaskHandle_t s_ds18b20_status_message_task = {0};
static TaskHandle_t s_dht_attributes_message_task = {0};
static TaskHandle_t s_dht_status_message_task = {0};

static const esp_partition_t *s_update_partition = NULL;
static esp_ota_handle_t s_update_handle = 0;
static uint16_t s_ota_message_part_number = 0;

static void s_zh_load_config(void);
static void s_zh_save_config(void);

static void s_zh_sensor_init(void);

static void s_zh_send_ds18b20_attributes_message_task(void *pvParameter);
static void s_zh_send_ds18b20_config_message(void);
static void s_zh_send_ds18b20_status_message_task(void *pvParameter);

static void s_zh_send_dht_attributes_message_task(void *pvParameter);
static void s_zh_send_dht_config_message(void);
static void s_zh_send_dht_status_message_task(void *pvParameter);

static void s_zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void s_zh_set_gateway_offline_status(void);

void app_main(void)
{
#if CONFIG_SENSOR_TYPE_DS18B20
    s_ds18b20_pin = CONFIG_DS18B20_PIN;
#endif
#if CONFIG_SENSOR_TYPE_DHT
    s_dht_pin = CONFIG_DHT_PIN;
#if CONFIG_DHT_TYPE_11
    s_dht_sensor_type = ZH_DHT11;
#endif
#if CONFIG_DHT_TYPE_22
    s_dht_sensor_type = ZH_DHT22;
#endif
#endif
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = {0};
    esp_ota_get_state_partition(running, &ota_state);
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    s_zh_load_config();
    s_zh_sensor_init();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B);
    esp_wifi_start();
    zh_espnow_init_config_t zh_espnow_init_config = ZH_ESPNOW_INIT_CONFIG_DEFAULT();
    zh_espnow_init(&zh_espnow_init_config);
    esp_event_handler_instance_register(ZH_ESPNOW, ESP_EVENT_ANY_ID, &s_zh_espnow_event_handler, NULL, NULL);
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

static void s_zh_load_config(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    uint8_t config_is_present = 0;
    if (nvs_get_u8(nvs_handle, "present", &config_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
        s_zh_save_config();
        return;
    }
    nvs_get_u8(nvs_handle, "ds18b20_pin", &s_ds18b20_pin);
    nvs_get_u8(nvs_handle, "dht_pin", &s_dht_pin);
    nvs_get_u8(nvs_handle, "dht_type", &s_dht_sensor_type);
    nvs_close(nvs_handle);
}

static void s_zh_save_config(void)
{
    nvs_handle_t nvs_handle = 0;
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "ds18b20_pin", s_ds18b20_pin);
    nvs_set_u8(nvs_handle, "dht_pin", s_dht_pin);
    nvs_set_u8(nvs_handle, "dht_type", s_dht_sensor_type);
    nvs_close(nvs_handle);
}

static void s_zh_sensor_init(void)
{
    if (s_ds18b20_pin != ZH_NOT_USED)
    {
        zh_onewire_init(s_ds18b20_pin);
    }
    if (s_dht_pin != ZH_NOT_USED)
    {
        s_zh_dht_handle = zh_dht_init((zh_dht_sensor_type_t)s_dht_sensor_type, s_dht_pin);
    }
}

static void s_zh_send_ds18b20_attributes_message_task(void *pvParameter)
{
    const esp_app_desc_t *app_info = esp_app_get_description();
    zh_attributes_message_t attributes_message = {0};
    attributes_message.chip_type = HACHT_ESP32;
    attributes_message.sensor_type = HAST_DS18B20;
    strcpy(attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    attributes_message.cpu_frequency = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
    attributes_message.reset_reason = (uint8_t)esp_reset_reason();
    strcpy(attributes_message.app_name, app_info->project_name);
    strcpy(attributes_message.app_version, app_info->version);
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_ATTRIBUTES;
    for (;;)
    {
        attributes_message.heap_size = esp_get_free_heap_size();
        attributes_message.min_heap_size = esp_get_minimum_free_heap_size();
        attributes_message.uptime = esp_timer_get_time() / 1000000;
        data.payload_data = (zh_payload_data_t)attributes_message;
        zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_ds18b20_config_message(void)
{
    zh_sensor_config_message_t sensor_config_message = {0};
    sensor_config_message.unique_id = 1;
    sensor_config_message.sensor_device_class = HASDC_TEMPERATURE;
    char *unit_of_measurement = "°C";
    strcpy(sensor_config_message.unit_of_measurement, unit_of_measurement);
    sensor_config_message.suggested_display_precision = 1;
    sensor_config_message.expire_after = 180;
    sensor_config_message.enabled_by_default = true;
    sensor_config_message.force_update = true;
    sensor_config_message.qos = 2;
    sensor_config_message.retain = true;
    zh_config_message_t config_message = {0};
    config_message = (zh_config_message_t)sensor_config_message;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_CONFIG;
    data.payload_data = (zh_payload_data_t)config_message;
    zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

static void s_zh_send_ds18b20_status_message_task(void *pvParameter)
{
    float temperature = 0;
    zh_sensor_status_message_t sensor_status_message = {0};
    sensor_status_message.sensor_type = HAST_DS18B20;
    zh_status_message_t status_message = {0};
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_STATE;
    for (;;)
    {
    ZH_DS18B20_READ:
        switch (zh_ds18b20_read_temp(NULL, &temperature))
        {
        case ESP_OK:
            sensor_status_message.temperature = temperature;
            break;
        case ESP_FAIL:
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            goto ZH_DS18B20_READ;
            break;
        case ESP_ERR_INVALID_CRC:
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            goto ZH_DS18B20_READ;
            break;
        default:
            break;
        }
        status_message = (zh_status_message_t)sensor_status_message;
        data.payload_data = (zh_payload_data_t)status_message;
        zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_dht_attributes_message_task(void *pvParameter)
{
    const esp_app_desc_t *app_info = esp_app_get_description();
    zh_attributes_message_t attributes_message = {0};
    attributes_message.chip_type = HACHT_ESP32;
    attributes_message.sensor_type = (s_dht_sensor_type == ZH_DHT11) ? HAST_DHT11 : HAST_DHT22;
    strcpy(attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    attributes_message.cpu_frequency = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
    attributes_message.reset_reason = (uint8_t)esp_reset_reason();
    strcpy(attributes_message.app_name, app_info->project_name);
    strcpy(attributes_message.app_version, app_info->version);
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_ATTRIBUTES;
    for (;;)
    {
        attributes_message.heap_size = esp_get_free_heap_size();
        attributes_message.min_heap_size = esp_get_minimum_free_heap_size();
        attributes_message.uptime = esp_timer_get_time() / 1000000;
        data.payload_data = (zh_payload_data_t)attributes_message;
        zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_send_dht_config_message(void)
{
    zh_sensor_config_message_t sensor_config_message = {0};
    sensor_config_message.unique_id = 1;
    sensor_config_message.sensor_device_class = HASDC_TEMPERATURE;
    char *unit_of_measurement = "°C";
    strcpy(sensor_config_message.unit_of_measurement, unit_of_measurement);
    sensor_config_message.suggested_display_precision = 1;
    sensor_config_message.expire_after = 180;
    sensor_config_message.enabled_by_default = true;
    sensor_config_message.force_update = true;
    sensor_config_message.qos = 2;
    sensor_config_message.retain = true;
    zh_config_message_t config_message = {0};
    config_message = (zh_config_message_t)sensor_config_message;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_CONFIG;
    data.payload_data = (zh_payload_data_t)config_message;
    zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
    sensor_config_message.unique_id = 2;
    sensor_config_message.sensor_device_class = HASDC_HUMIDITY;
    unit_of_measurement = "%";
    strcpy(sensor_config_message.unit_of_measurement, unit_of_measurement);
    config_message = (zh_config_message_t)sensor_config_message;
    data.payload_data = (zh_payload_data_t)config_message;
    zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

static void s_zh_send_dht_status_message_task(void *pvParameter)
{
    float humidity = 0;
    float temperature = 0;
    zh_sensor_status_message_t sensor_status_message = {0};
    sensor_status_message.sensor_type = (s_dht_sensor_type == ZH_DHT11) ? HAST_DHT11 : HAST_DHT22;
    zh_status_message_t status_message = {0};
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_STATE;
    for (;;)
    {
    ZH_DHT_READ:
        switch (zh_dht_read(&s_zh_dht_handle, &humidity, &temperature))
        {
        case ESP_OK:
            sensor_status_message.humidity = humidity;
            sensor_status_message.temperature = temperature;
            break;
        case ESP_ERR_INVALID_RESPONSE:
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            goto ZH_DHT_READ;
            break;
        case ESP_ERR_TIMEOUT:
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            goto ZH_DHT_READ;
            break;
        case ESP_ERR_INVALID_CRC:
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            goto ZH_DHT_READ;
            break;
        default:
            break;
        }
        status_message = (zh_status_message_t)sensor_status_message;
        data.payload_data = (zh_payload_data_t)status_message;
        zh_espnow_send(s_gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void s_zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    const esp_app_desc_t *app_info = esp_app_get_description();
    zh_espnow_data_t data_in = {0};
    zh_espnow_data_t data_out = {0};
    zh_espnow_ota_message_t espnow_ota_message = {0};
    data_out.device_type = ZHDT_SENSOR;
    espnow_ota_message.chip_type = HACHT_ESP32;
    data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
    switch (event_id)
    {
    case ZH_ESPNOW_ON_RECV_EVENT:;
        zh_espnow_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t))
        {
            goto ZH_ESPNOW_EVENT_HANDLER_EXIT;
        }
        memcpy(&data_in, recv_data->data, recv_data->data_len);
        switch (data_in.device_type)
        {
        case ZHDT_GATEWAY:
            switch (data_in.payload_type)
            {
            case ZHPT_KEEP_ALIVE:
                if (data_in.payload_data.keep_alive_message.online_status == ZH_ONLINE)
                {
                    if (s_gateway_is_available == false)
                    {
                        s_gateway_is_available = true;
                        memcpy(s_gateway_mac, recv_data->mac_addr, ESP_NOW_ETH_ALEN);
                        if (s_ds18b20_pin != ZH_NOT_USED)
                        {
                            s_zh_send_ds18b20_config_message();
                            xTaskCreatePinnedToCore(&s_zh_send_ds18b20_status_message_task, "s_zh_send_ds18b20_status_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_ds18b20_status_message_task, tskNO_AFFINITY);
                            xTaskCreatePinnedToCore(&s_zh_send_ds18b20_attributes_message_task, "s_zh_send_ds18b20_attributes_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_ds18b20_attributes_message_task, tskNO_AFFINITY);
                        }
                        if (s_dht_pin != ZH_NOT_USED)
                        {
                            s_zh_send_dht_config_message();
                            xTaskCreatePinnedToCore(&s_zh_send_dht_status_message_task, "s_zh_send_dht_status_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_dht_status_message_task, tskNO_AFFINITY);
                            xTaskCreatePinnedToCore(&s_zh_send_dht_attributes_message_task, "s_zh_send_dht_attributes_message_task", ZH_MESSAGE_STACK_SIZE, NULL, ZH_MESSAGE_TASK_PRIORITY, &s_dht_attributes_message_task, tskNO_AFFINITY);
                        }
                    }
                }
                else
                {
                    if (s_gateway_is_available == true)
                    {
                        s_zh_set_gateway_offline_status();
                    }
                }
                break;
            case ZHPT_UPDATE:
                s_update_partition = esp_ota_get_next_update_partition(NULL);
                strcpy(espnow_ota_message.app_name, app_info->project_name);
                strcpy(espnow_ota_message.app_version, app_info->version);
                data_out.payload_type = ZHPT_UPDATE;
                data_out.payload_data = (zh_payload_data_t)espnow_ota_message;
                zh_espnow_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_BEGIN:
                esp_ota_begin(s_update_partition, OTA_SIZE_UNKNOWN, &s_update_handle);
                s_ota_message_part_number = 1;
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_espnow_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_PROGRESS:
                if (s_ota_message_part_number == data_in.payload_data.espnow_ota_message.part)
                {
                    ++s_ota_message_part_number;
                    esp_ota_write(s_update_handle, (const void *)data_in.payload_data.espnow_ota_message.data, data_in.payload_data.espnow_ota_message.data_len);
                }
                data_out.payload_type = ZHPT_UPDATE_PROGRESS;
                zh_espnow_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_ERROR:
                esp_ota_end(s_update_handle);
                break;
            case ZHPT_UPDATE_END:
                if (esp_ota_end(s_update_handle) != ESP_OK)
                {
                    data_out.payload_type = ZHPT_UPDATE_FAIL;
                    zh_espnow_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                    break;
                }
                esp_ota_set_boot_partition(s_update_partition);
                data_out.payload_type = ZHPT_UPDATE_SUCCESS;
                zh_espnow_send(s_gateway_mac, (uint8_t *)&data_out, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_RESTART:
                esp_restart();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    ZH_ESPNOW_EVENT_HANDLER_EXIT:
        free(recv_data->data);
        break;
    case ZH_ESPNOW_ON_SEND_EVENT:;
        zh_espnow_event_on_send_t *send_data = event_data;
        if (send_data->status == ZH_ESPNOW_SEND_FAIL && s_gateway_is_available == true)
        {
            s_zh_set_gateway_offline_status();
        }
        break;
    default:
        break;
    }
}

static void s_zh_set_gateway_offline_status(void)
{
    s_gateway_is_available = false;
    if (s_ds18b20_pin != ZH_NOT_USED)
    {
        vTaskDelete(s_ds18b20_attributes_message_task);
        vTaskDelete(s_ds18b20_status_message_task);
    }
    if (s_dht_pin != ZH_NOT_USED)
    {
        vTaskDelete(s_dht_attributes_message_task);
        vTaskDelete(s_dht_status_message_task);
    }
}