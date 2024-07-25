#include "zh_espnow_sensor.h"

sensor_config_t sensor_main_config = {0};

void app_main(void)
{
    sensor_config_t *sensor_config = &sensor_main_config;
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    zh_load_config(sensor_config);
    zh_sensor_init(sensor_config);
    uint8_t power_selection_pin = zh_load_power_selection_pin();
    if (power_selection_pin != ZH_NOT_USED && sensor_config->hardware_config.battery_power == true)
    {
        gpio_config_t config = {0};
        config.intr_type = GPIO_INTR_DISABLE;
        config.mode = GPIO_MODE_INPUT;
        config.pin_bit_mask = (1ULL << power_selection_pin);
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&config);
        if (gpio_get_level(power_selection_pin) == 0)
        {
            sensor_config->hardware_config.battery_power = false;
        }
    }
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_mode(WIFI_MODE_STA);
#ifdef CONFIG_IDF_TARGET_ESP8266
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
#else
    esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
#endif
    esp_wifi_start();
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    zh_espnow_init_config_t espnow_init_config = ZH_ESPNOW_INIT_CONFIG_DEFAULT();
    zh_espnow_init(&espnow_init_config);
#else
    zh_network_init_config_t network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
    zh_network_init(&network_init_config);
#endif
#ifdef CONFIG_IDF_TARGET_ESP8266
    esp_event_handler_register(ZH_EVENT, ESP_EVENT_ANY_ID, &zh_espnow_event_handler, sensor_config);
    if (sensor_config->hardware_config.battery_power == true)
    {
        zh_sensor_deep_sleep(sensor_config);
    }
#else
    esp_event_handler_instance_register(ZH_EVENT, ESP_EVENT_ANY_ID, &zh_espnow_event_handler, sensor_config, NULL);
    if (sensor_config->hardware_config.battery_power == true)
    {
        zh_sensor_deep_sleep(sensor_config);
    }
    else
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_ota_img_states_t ota_state = {0};
        esp_ota_get_state_partition(running, &ota_state);
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            vTaskDelay(60000 / portTICK_PERIOD_MS);
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
#endif
}

void zh_load_config(sensor_config_t *sensor_config)
{
    nvs_handle_t nvs_handle = {0};
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    uint8_t config_is_present = {0};
    if (nvs_get_u8(nvs_handle, "present", &config_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
    SETUP_INITIAL_SETTINGS:
#ifdef CONFIG_SENSOR_TYPE_DS18B20
        sensor_config->hardware_config.sensor_type = HAST_DS18B20;
#elif CONFIG_SENSOR_TYPE_DHT
        sensor_config->hardware_config.sensor_type = HAST_DHT;
#elif CONFIG_SENSOR_TYPE_AHT
        sensor_config->hardware_config.sensor_type = HAST_AHT;
#elif CONFIG_SENSOR_TYPE_BH1750
        sensor_config->hardware_config.sensor_type = HAST_BH1750;
#else
        sensor_config->hardware_config.sensor_type = HAST_NONE;
#endif
#ifdef CONFIG_MEASUREMENT_FREQUENCY
        sensor_config->hardware_config.measurement_frequency = CONFIG_MEASUREMENT_FREQUENCY;
#else
        sensor_config->hardware_config.measurement_frequency = 300;
#endif
#ifdef CONFIG_SENSOR_PIN_1
        sensor_config->hardware_config.sensor_pin_1 = CONFIG_SENSOR_PIN_1;
#else
        sensor_config->hardware_config.sensor_pin_1 = ZH_NOT_USED;
#endif
#ifdef CONFIG_SENSOR_PIN_2
        sensor_config->hardware_config.sensor_pin_2 = CONFIG_SENSOR_PIN_2;
#else
        sensor_config->hardware_config.sensor_pin_2 = ZH_NOT_USED;
#endif
#ifdef CONFIG_POWER_CONTROL_PIN
        sensor_config->hardware_config.power_pin = CONFIG_POWER_CONTROL_PIN;
#else
        sensor_config->hardware_config.power_pin = ZH_NOT_USED;
#endif
#ifdef CONFIG_BATTERY_POWERED
        sensor_config->hardware_config.battery_power = true;
#else
        sensor_config->hardware_config.battery_power = false;
#endif
        zh_save_config(sensor_config);
        return;
    }
    esp_err_t err = ESP_OK;
    err += nvs_get_u8(nvs_handle, "sensor_type", (uint8_t *)&sensor_config->hardware_config.sensor_type);
    err += nvs_get_u8(nvs_handle, "sensor_pin_1", &sensor_config->hardware_config.sensor_pin_1);
    err += nvs_get_u8(nvs_handle, "sensor_pin_2", &sensor_config->hardware_config.sensor_pin_2);
    err += nvs_get_u8(nvs_handle, "power_pin", &sensor_config->hardware_config.power_pin);
    err += nvs_get_u16(nvs_handle, "frequency", &sensor_config->hardware_config.measurement_frequency);
    err += nvs_get_u8(nvs_handle, "battery_power", (uint8_t *)&sensor_config->hardware_config.battery_power);
    nvs_close(nvs_handle);
    if (err != ESP_OK)
    {
        goto SETUP_INITIAL_SETTINGS;
    }
}

void zh_save_config(const sensor_config_t *sensor_config)
{
    nvs_handle_t nvs_handle = {0};
    nvs_open("config", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "sensor_type", sensor_config->hardware_config.sensor_type);
    nvs_set_u8(nvs_handle, "sensor_pin_1", sensor_config->hardware_config.sensor_pin_1);
    nvs_set_u8(nvs_handle, "sensor_pin_2", sensor_config->hardware_config.sensor_pin_2);
    nvs_set_u8(nvs_handle, "power_pin", sensor_config->hardware_config.power_pin);
    nvs_set_u16(nvs_handle, "frequency", sensor_config->hardware_config.measurement_frequency);
    nvs_set_u8(nvs_handle, "battery_power", sensor_config->hardware_config.battery_power);
    nvs_close(nvs_handle);
}

uint8_t zh_load_power_selection_pin(void)
{
    uint8_t power_selection_pin = {0};
    nvs_handle_t nvs_handle = {0};
    nvs_open("selection_pin", NVS_READWRITE, &nvs_handle);
    uint8_t config_is_present = {0};
    if (nvs_get_u8(nvs_handle, "present", &config_is_present) == ESP_ERR_NVS_NOT_FOUND)
    {
        nvs_set_u8(nvs_handle, "present", 0xFE);
        nvs_close(nvs_handle);
#ifdef CONFIG_POWER_MODE_USING
        power_selection_pin = CONFIG_POWER_MODE_PIN;
#else
        uint8_t power_selection_pin = ZH_NOT_USED;
#endif

        zh_save_power_selection_pin(&power_selection_pin);
        return power_selection_pin;
    }
    nvs_get_u8(nvs_handle, "mode_pin", &power_selection_pin);
    nvs_close(nvs_handle);
    return power_selection_pin;
}

void zh_save_power_selection_pin(const uint8_t *power_selection_pin)
{
    nvs_handle_t nvs_handle = {0};
    nvs_open("selection_pin", NVS_READWRITE, &nvs_handle);
    nvs_set_u8(nvs_handle, "mode_pin", *power_selection_pin);
    nvs_close(nvs_handle);
}

void zh_sensor_init(sensor_config_t *sensor_config)
{
    if (sensor_config->hardware_config.power_pin != ZH_NOT_USED)
    {
        gpio_config_t config = {0};
        config.intr_type = GPIO_INTR_DISABLE;
        config.mode = GPIO_MODE_OUTPUT;
        config.pin_bit_mask = (1ULL << sensor_config->hardware_config.power_pin);
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        if (gpio_config(&config) != ESP_OK)
        {
            sensor_config->hardware_config.power_pin = ZH_NOT_USED;
        }
        else
        {
            gpio_set_level(sensor_config->hardware_config.power_pin, 0);
        }
    }
    if (sensor_config->hardware_config.sensor_type != HAST_NONE && sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_pin_2 != ZH_NOT_USED)
    {
#ifdef CONFIG_IDF_TARGET_ESP8266
        i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = sensor_config->hardware_config.sensor_pin_1,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = sensor_config->hardware_config.sensor_pin_2,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
        };
        i2c_driver_install(I2C_PORT, i2c_config.mode);
        i2c_param_config(I2C_PORT, &i2c_config);
#else
        i2c_master_bus_config_t i2c_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = I2C_PORT,
            .scl_io_num = sensor_config->hardware_config.sensor_pin_2,
            .sda_io_num = sensor_config->hardware_config.sensor_pin_1,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        i2c_new_master_bus(&i2c_bus_config, &sensor_config->i2c_bus_handle);
#endif
    }
    if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED)
    {
        switch (sensor_config->hardware_config.sensor_type)
        {
        case HAST_DS18B20:
            if (zh_onewire_init(sensor_config->hardware_config.sensor_pin_1) != ESP_OK)
            {
                goto ZH_SENSOR_ERROR;
            }
            break;
        case HAST_DHT:;
            zh_dht_init_config_t dht_init_config = ZH_DHT_INIT_CONFIG_DEFAULT();
            if (sensor_config->hardware_config.sensor_pin_2 == ZH_NOT_USED)
            {
                dht_init_config.sensor_pin = sensor_config->hardware_config.sensor_pin_1;
            }
            else
            {
#ifdef CONFIG_IDF_TARGET_ESP8266
                dht_init_config.i2c_port = I2C_PORT;
#else
                dht_init_config.i2c_handle = sensor_config->i2c_bus_handle;
#endif
            }
            if (zh_dht_init(&dht_init_config) != ESP_OK)
            {
                goto ZH_SENSOR_ERROR;
            }
            break;
        case HAST_BH1750:;
            zh_bh1750_init_config_t bh1750_init_config = ZH_BH1750_INIT_CONFIG_DEFAULT();
            bh1750_init_config.auto_adjust = true;
#ifdef CONFIG_IDF_TARGET_ESP8266
            bh1750_init_config.i2c_port = I2C_PORT;
#else
            bh1750_init_config.i2c_handle = sensor_config->i2c_bus_handle;
#endif
            if (zh_bh1750_init(&bh1750_init_config) != ESP_OK)
            {
                goto ZH_SENSOR_ERROR;
            }
            break;
        case HAST_BMP280: // For future development.
            break;
        case HAST_BME280: // For future development.
            break;
        case HAST_BME680: // For future development.
            break;
        case HAST_AHT:;
            zh_aht_init_config_t aht_init_config = ZH_AHT_INIT_CONFIG_DEFAULT();
#ifdef CONFIG_IDF_TARGET_ESP8266
            aht_init_config.i2c_port = I2C_PORT;
#else
            aht_init_config.i2c_handle = sensor_config->i2c_bus_handle;
#endif
            if (zh_aht_init(&aht_init_config) != ESP_OK)
            {
                goto ZH_SENSOR_ERROR;
            }
            break;
        case HAST_SHT: // For future development.
            break;
        case HAST_HTU: // For future development.
            break;
        case HAST_HDC1080: // For future development.
            break;
        default:
        ZH_SENSOR_ERROR:
            sensor_config->hardware_config.sensor_type = HAST_NONE;
            sensor_config->hardware_config.sensor_pin_1 = ZH_NOT_USED;
            sensor_config->hardware_config.sensor_pin_2 = ZH_NOT_USED;
            break;
        }
    }
}

void zh_sensor_deep_sleep(sensor_config_t *sensor_config)
{
    uint8_t gateway[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(sensor_config->gateway_mac, gateway, 6);
#ifndef CONFIG_IDF_TARGET_ESP8266
    esp_sleep_enable_timer_wakeup(sensor_config->hardware_config.measurement_frequency * 1000000);
#endif
    uint8_t required_message_quantity = 2;
    zh_send_sensor_hardware_config_message(sensor_config);
    zh_send_sensor_keep_alive_message_task(sensor_config);
    if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
    {
        required_message_quantity += zh_send_sensor_config_message(sensor_config);
        zh_send_sensor_status_message_task(sensor_config);
        zh_send_sensor_attributes_message_task(sensor_config);
    }
    while (sensor_config->sent_message_quantity < (required_message_quantity + 2))
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
#ifdef CONFIG_IDF_TARGET_ESP8266
    esp_deep_sleep(sensor_config->hardware_config.measurement_frequency * 1000000);
#else
    esp_deep_sleep_start();
#endif
}

void zh_send_sensor_hardware_config_message(const sensor_config_t *sensor_config)
{
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_HARDWARE;
    data.payload_data.config_message.sensor_hardware_config_message.sensor_type = sensor_config->hardware_config.sensor_type;
    data.payload_data.config_message.sensor_hardware_config_message.sensor_pin_1 = sensor_config->hardware_config.sensor_pin_1;
    data.payload_data.config_message.sensor_hardware_config_message.sensor_pin_2 = sensor_config->hardware_config.sensor_pin_2;
    data.payload_data.config_message.sensor_hardware_config_message.power_pin = sensor_config->hardware_config.power_pin;
    data.payload_data.config_message.sensor_hardware_config_message.measurement_frequency = sensor_config->hardware_config.measurement_frequency;
    data.payload_data.config_message.sensor_hardware_config_message.battery_power = sensor_config->hardware_config.battery_power;
    zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
}

void zh_send_sensor_attributes_message_task(void *pvParameter)
{
    sensor_config_t *sensor_config = pvParameter;
    const esp_app_desc_t *app_info = get_app_description();
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_ATTRIBUTES;
    data.payload_data.attributes_message.chip_type = ZH_CHIP_TYPE;
    data.payload_data.attributes_message.sensor_type = sensor_config->hardware_config.sensor_type;
    strcpy(data.payload_data.attributes_message.flash_size, CONFIG_ESPTOOLPY_FLASHSIZE);
    data.payload_data.attributes_message.cpu_frequency = ZH_CPU_FREQUENCY;
    data.payload_data.attributes_message.reset_reason = (uint8_t)esp_reset_reason();
    strcpy(data.payload_data.attributes_message.app_name, app_info->project_name);
    strcpy(data.payload_data.attributes_message.app_version, app_info->version);
    for (;;)
    {
        data.payload_data.attributes_message.heap_size = esp_get_free_heap_size();
        data.payload_data.attributes_message.min_heap_size = esp_get_minimum_free_heap_size();
        data.payload_data.attributes_message.uptime = esp_timer_get_time() / 1000000;
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        if (sensor_config->hardware_config.battery_power == true)
        {
            return;
        }
        vTaskDelay(ZH_SENSOR_ATTRIBUTES_MESSAGE_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

uint8_t zh_send_sensor_config_message(const sensor_config_t *sensor_config)
{
    uint8_t messages_quantity = 0;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_CONFIG;
    data.payload_data.config_message.sensor_config_message.suggested_display_precision = 1;
    data.payload_data.config_message.sensor_config_message.expire_after = sensor_config->hardware_config.measurement_frequency * 1.5; // + 50% just in case.
    data.payload_data.config_message.sensor_config_message.enabled_by_default = true;
    data.payload_data.config_message.sensor_config_message.force_update = true;
    data.payload_data.config_message.sensor_config_message.qos = 2;
    data.payload_data.config_message.sensor_config_message.retain = true;
    char *unit_of_measurement = NULL;
    data.payload_data.config_message.sensor_config_message.unique_id = 1;
    data.payload_data.config_message.sensor_config_message.sensor_device_class = HASDC_VOLTAGE;
    unit_of_measurement = "V";
    strcpy(data.payload_data.config_message.sensor_config_message.unit_of_measurement, unit_of_measurement);
    zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
    ++messages_quantity;
    switch (sensor_config->hardware_config.sensor_type)
    {
    case HAST_DS18B20:
        data.payload_data.config_message.sensor_config_message.unique_id = 2;
        data.payload_data.config_message.sensor_config_message.sensor_device_class = HASDC_TEMPERATURE;
        unit_of_measurement = "°C";
        strcpy(data.payload_data.config_message.sensor_config_message.unit_of_measurement, unit_of_measurement);
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        ++messages_quantity;
        break;
    case HAST_DHT:
    case HAST_AHT:
    case HAST_SHT:     // For future development.
    case HAST_HTU:     // For future development.
    case HAST_HDC1080: // For future development.
        data.payload_data.config_message.sensor_config_message.unique_id = 2;
        data.payload_data.config_message.sensor_config_message.sensor_device_class = HASDC_TEMPERATURE;
        unit_of_measurement = "°C";
        strcpy(data.payload_data.config_message.sensor_config_message.unit_of_measurement, unit_of_measurement);
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        ++messages_quantity;
        data.payload_data.config_message.sensor_config_message.unique_id = 3;
        data.payload_data.config_message.sensor_config_message.sensor_device_class = HASDC_HUMIDITY;
        unit_of_measurement = "%";
        strcpy(data.payload_data.config_message.sensor_config_message.unit_of_measurement, unit_of_measurement);
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        ++messages_quantity;
        break;
    case HAST_BH1750:
        data.payload_data.config_message.sensor_config_message.unique_id = 2;
        data.payload_data.config_message.sensor_config_message.sensor_device_class = HASDC_ILLUMINANCE;
        unit_of_measurement = "lx";
        strcpy(data.payload_data.config_message.sensor_config_message.unit_of_measurement, unit_of_measurement);
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        ++messages_quantity;
        break;
    case HAST_BMP280: // For future development.
        break;
    case HAST_BME280: // For future development.
        break;
    case HAST_BME680: // For future development.
        break;
    default:
        break;
    }
    return messages_quantity;
}

void zh_send_sensor_status_message_task(void *pvParameter)
{
    sensor_config_t *sensor_config = pvParameter;
    float humidity = 0.0;
    float temperature = 0.0;
    float illuminance = 0.0;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    for (;;)
    {
        if (sensor_config->hardware_config.power_pin != ZH_NOT_USED && sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_pin_2 == ZH_NOT_USED)
        {
            gpio_set_level(sensor_config->hardware_config.power_pin, 1);
            switch (sensor_config->hardware_config.sensor_type)
            {
            case HAST_DS18B20:
                vTaskDelay(DS18B20_POWER_STABILIZATION_PERIOD / portTICK_PERIOD_MS);
                break;
            case HAST_DHT:
                vTaskDelay(DHT_POWER_STABILIZATION_PERIOD / portTICK_PERIOD_MS);
                break;
            default:
                gpio_set_level(sensor_config->hardware_config.power_pin, 0);
                break;
            }
        }
        uint8_t attempts = 0;
    READ_SENSOR:;
        esp_err_t err = ESP_OK;
        switch (sensor_config->hardware_config.sensor_type)
        {
        case HAST_DS18B20:
            err = zh_ds18b20_read(NULL, &temperature);
            if (err == ESP_OK)
            {
                data.payload_data.status_message.sensor_status_message.temperature = temperature;
                data.payload_data.status_message.sensor_status_message.voltage = 3.3; // For future development.
            }
            break;
        case HAST_DHT:
            err = zh_dht_read(&humidity, &temperature);
            if (err == ESP_OK)
            {
                data.payload_data.status_message.sensor_status_message.humidity = humidity;
                data.payload_data.status_message.sensor_status_message.temperature = temperature;
                data.payload_data.status_message.sensor_status_message.voltage = 3.3; // For future development.
            }
            break;
        case HAST_BH1750:
            err = zh_bh1750_read(&illuminance);
            if (err == ESP_OK)
            {
                data.payload_data.status_message.sensor_status_message.illuminance = illuminance;
                data.payload_data.status_message.sensor_status_message.voltage = 3.3; // For future development.
            }
            break;
        case HAST_BMP280: // For future development.
            break;
        case HAST_BME280: // For future development.
            break;
        case HAST_BME680: // For future development.
            break;
        case HAST_AHT:
            err = zh_aht_read(&humidity, &temperature);
            if (err == ESP_OK)
            {
                data.payload_data.status_message.sensor_status_message.humidity = humidity;
                data.payload_data.status_message.sensor_status_message.temperature = temperature;
                data.payload_data.status_message.sensor_status_message.voltage = 3.3; // For future development.
            }
            break;
        case HAST_SHT: // For future development.
            break;
        case HAST_HTU: // For future development.
            break;
        case HAST_HDC1080: // For future development.
            break;
        default:
            break;
        }
        if (err == ESP_OK)
        {
            data.payload_type = ZHPT_STATE;
            data.payload_data.status_message.sensor_status_message.sensor_type = sensor_config->hardware_config.sensor_type;
            zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        }
        else
        {
            if (++attempts < ZH_SENSOR_READ_MAXIMUM_RETRY)
            {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                goto READ_SENSOR;
            }
            data.payload_type = ZHPT_ERROR;
            char *message = (char *)heap_caps_malloc(150, MALLOC_CAP_8BIT);
            memset(message, 0, 150);
            sprintf(message, "Sensor %s reading error. Error - %s.", zh_get_sensor_type_value_name(sensor_config->hardware_config.sensor_type), esp_err_to_name(err));
            strcpy(data.payload_data.status_message.error_message.message, message);
            zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
            heap_caps_free(message);
        }
        if (gpio_get_level(sensor_config->hardware_config.power_pin) == 1)
        {
            gpio_set_level(sensor_config->hardware_config.power_pin, 0);
        }
        if (sensor_config->hardware_config.battery_power == true)
        {
            return;
        }
        vTaskDelay(sensor_config->hardware_config.measurement_frequency * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void zh_send_sensor_keep_alive_message_task(void *pvParameter)
{
    sensor_config_t *sensor_config = pvParameter;
    zh_espnow_data_t data = {0};
    data.device_type = ZHDT_SENSOR;
    data.payload_type = ZHPT_KEEP_ALIVE;
    data.payload_data.keep_alive_message.online_status = ZH_ONLINE;
    data.payload_data.keep_alive_message.message_frequency = ZH_SENSOR_KEEP_ALIVE_MESSAGE_FREQUENCY;
    for (;;)
    {
        zh_send_message(sensor_config->gateway_mac, (uint8_t *)&data, sizeof(zh_espnow_data_t));
        if (sensor_config->hardware_config.battery_power == true)
        {
            return;
        }
        vTaskDelay(ZH_SENSOR_KEEP_ALIVE_MESSAGE_FREQUENCY * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void zh_espnow_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    sensor_config_t *sensor_config = arg;
    switch (event_id)
    {
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    case ZH_ESPNOW_ON_RECV_EVENT:;
        zh_espnow_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t) || sensor_config->hardware_config.battery_power == true)
        {
            goto ZH_ESPNOW_EVENT_HANDLER_EXIT;
        }
#else
    case ZH_NETWORK_ON_RECV_EVENT:;
        zh_network_event_on_recv_t *recv_data = event_data;
        if (recv_data->data_len != sizeof(zh_espnow_data_t) || sensor_config->hardware_config.battery_power == true)
        {
            goto ZH_NETWORK_EVENT_HANDLER_EXIT;
        }
#endif
        zh_espnow_data_t *data = (zh_espnow_data_t *)recv_data->data;
        switch (data->device_type)
        {
        case ZHDT_GATEWAY:
            switch (data->payload_type)
            {
            case ZHPT_KEEP_ALIVE:
                if (data->payload_data.keep_alive_message.online_status == ZH_ONLINE)
                {
                    if (sensor_config->gateway_is_available == false)
                    {
                        sensor_config->gateway_is_available = true;
                        memcpy(sensor_config->gateway_mac, recv_data->mac_addr, 6);
                        zh_send_sensor_hardware_config_message(sensor_config);
                        if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                        {
                            zh_send_sensor_config_message(sensor_config);
                            if (sensor_config->is_first_connection == false)
                            {
                                xTaskCreatePinnedToCore(&zh_send_sensor_status_message_task, "NULL", ZH_MESSAGE_STACK_SIZE, sensor_config, ZH_MESSAGE_TASK_PRIORITY, (TaskHandle_t *)&sensor_config->status_message_task, tskNO_AFFINITY);
                                xTaskCreatePinnedToCore(&zh_send_sensor_attributes_message_task, "NULL", ZH_MESSAGE_STACK_SIZE, sensor_config, ZH_MESSAGE_TASK_PRIORITY, (TaskHandle_t *)&sensor_config->attributes_message_task, tskNO_AFFINITY);
                                xTaskCreatePinnedToCore(&zh_send_sensor_keep_alive_message_task, "NULL", ZH_MESSAGE_STACK_SIZE, sensor_config, ZH_MESSAGE_TASK_PRIORITY, (TaskHandle_t *)&sensor_config->keep_alive_message_task, tskNO_AFFINITY);
                                sensor_config->is_first_connection = true;
                            }
                            else
                            {
                                vTaskResume(sensor_config->status_message_task);
                                vTaskResume(sensor_config->attributes_message_task);
                                vTaskResume(sensor_config->keep_alive_message_task);
                            }
                        }
                    }
                }
                else
                {
                    if (sensor_config->gateway_is_available == true)
                    {
                        sensor_config->gateway_is_available = false;
                        if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                        {
                            vTaskSuspend(sensor_config->status_message_task);
                            vTaskSuspend(sensor_config->attributes_message_task);
                            vTaskSuspend(sensor_config->keep_alive_message_task);
                        }
                    }
                }
                break;
            case ZHPT_HARDWARE:
                sensor_config->hardware_config.sensor_type = data->payload_data.config_message.sensor_hardware_config_message.sensor_type;
                sensor_config->hardware_config.sensor_pin_1 = data->payload_data.config_message.sensor_hardware_config_message.sensor_pin_1;
                sensor_config->hardware_config.sensor_pin_2 = data->payload_data.config_message.sensor_hardware_config_message.sensor_pin_2;
                sensor_config->hardware_config.power_pin = data->payload_data.config_message.sensor_hardware_config_message.power_pin;
                sensor_config->hardware_config.measurement_frequency = data->payload_data.config_message.sensor_hardware_config_message.measurement_frequency;
                sensor_config->hardware_config.battery_power = data->payload_data.config_message.sensor_hardware_config_message.battery_power;
                zh_save_config(sensor_config);
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskDelete(sensor_config->status_message_task);
                    vTaskDelete(sensor_config->attributes_message_task);
                    vTaskDelete(sensor_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_SENSOR_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_UPDATE:;
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskSuspend(sensor_config->status_message_task);
                    vTaskSuspend(sensor_config->attributes_message_task);
                    vTaskSuspend(sensor_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_SENSOR_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                const esp_app_desc_t *app_info = get_app_description();
                sensor_config->update_partition = esp_ota_get_next_update_partition(NULL);
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_version, app_info->version);
#ifdef CONFIG_IDF_TARGET_ESP8266
                char *app_name = (char *)heap_caps_malloc(strlen(app_info->project_name) + 6, MALLOC_CAP_8BIT);
                memset(app_name, 0, strlen(app_info->project_name) + 6);
                sprintf(app_name, "%s.app%d", app_info->project_name, sensor_config->update_partition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0 + 1);
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_name, app_name);
                heap_caps_free(app_name);
#else
                strcpy(data->payload_data.ota_message.espnow_ota_data.app_name, app_info->project_name);
#endif
                data->payload_type = ZHPT_UPDATE;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_BEGIN:
#ifdef CONFIG_IDF_TARGET_ESP8266
                esp_ota_begin(sensor_config->update_partition, OTA_SIZE_UNKNOWN, &sensor_config->update_handle);
#else
                esp_ota_begin(sensor_config->update_partition, OTA_SIZE_UNKNOWN, (esp_ota_handle_t *)&sensor_config->update_handle);
#endif
                sensor_config->ota_message_part_number = 1;
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_PROGRESS:
                if (sensor_config->ota_message_part_number == data->payload_data.ota_message.espnow_ota_message.part)
                {
                    ++sensor_config->ota_message_part_number;
                    esp_ota_write(sensor_config->update_handle, (const void *)data->payload_data.ota_message.espnow_ota_message.data, data->payload_data.ota_message.espnow_ota_message.data_len);
                }
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_UPDATE_PROGRESS;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                break;
            case ZHPT_UPDATE_ERROR:
                esp_ota_end(sensor_config->update_handle);
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskResume(sensor_config->status_message_task);
                    vTaskResume(sensor_config->attributes_message_task);
                    vTaskResume(sensor_config->keep_alive_message_task);
                }
                break;
            case ZHPT_UPDATE_END:
                if (esp_ota_end(sensor_config->update_handle) != ESP_OK)
                {
                    data->device_type = ZHDT_SENSOR;
                    data->payload_type = ZHPT_UPDATE_FAIL;
                    zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                    if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                    {
                        vTaskResume(sensor_config->status_message_task);
                        vTaskResume(sensor_config->attributes_message_task);
                        vTaskResume(sensor_config->keep_alive_message_task);
                    }
                    break;
                }
                esp_ota_set_boot_partition(sensor_config->update_partition);
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_UPDATE_SUCCESS;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            case ZHPT_RESTART:
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskDelete(sensor_config->status_message_task);
                    vTaskDelete(sensor_config->attributes_message_task);
                    vTaskDelete(sensor_config->keep_alive_message_task);
                }
                data->device_type = ZHDT_SENSOR;
                data->payload_type = ZHPT_KEEP_ALIVE;
                data->payload_data.keep_alive_message.online_status = ZH_OFFLINE;
                data->payload_data.keep_alive_message.message_frequency = ZH_SENSOR_KEEP_ALIVE_MESSAGE_FREQUENCY;
                zh_send_message(sensor_config->gateway_mac, (uint8_t *)data, sizeof(zh_espnow_data_t));
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                esp_restart();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
#ifdef CONFIG_NETWORK_TYPE_DIRECT
    ZH_ESPNOW_EVENT_HANDLER_EXIT:
        heap_caps_free(recv_data->data);
        break;
    case ZH_ESPNOW_ON_SEND_EVENT:;
        zh_espnow_event_on_send_t *send_data = event_data;
        if (sensor_config->hardware_config.battery_power == false)
        {
            if (send_data->status == ZH_ESPNOW_SEND_FAIL && sensor_config->gateway_is_available == true)
            {
                sensor_config->gateway_is_available = false;
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskSuspend(sensor_config->status_message_task);
                    vTaskSuspend(sensor_config->attributes_message_task);
                    vTaskSuspend(sensor_config->keep_alive_message_task);
                }
            }
        }
        else
        {
            ++sensor_config->sent_message_quantity;
        }
        break;
#else
    ZH_NETWORK_EVENT_HANDLER_EXIT:
        heap_caps_free(recv_data->data);
        break;
    case ZH_NETWORK_ON_SEND_EVENT:;
        zh_network_event_on_send_t *send_data = event_data;
        if (sensor_config->hardware_config.battery_power == false)
        {
            if (send_data->status == ZH_NETWORK_SEND_FAIL && sensor_config->gateway_is_available == true)
            {
                sensor_config->gateway_is_available = false;
                if (sensor_config->hardware_config.sensor_pin_1 != ZH_NOT_USED && sensor_config->hardware_config.sensor_type != HAST_NONE)
                {
                    vTaskSuspend(sensor_config->status_message_task);
                    vTaskSuspend(sensor_config->attributes_message_task);
                    vTaskSuspend(sensor_config->keep_alive_message_task);
                }
            }
        }
        else
        {
            ++sensor_config->sent_message_quantity;
        }
        break;
#endif
    default:
        break;
    }
}