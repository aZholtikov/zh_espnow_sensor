| Supported devices | ESP8266 | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C2 | ESP32-C3 | ESP32-C6 |
| ----------------- | ------- | ------| -------- | -------- | -------- | -------- | -------- |

# ESP-NOW sensor

ESP-NOW based sensor for ESP32 ESP-IDF and ESP8266 RTOS SDK.

## Tested on

1. ESP8266 RTOS_SDK v3.4
2. ESP32 ESP-IDF v5.2

## Features

1. Supported sensors:
    1. DS18B20
    2. DHT11/22
2. Automatically adds sensor configuration to Home Assistan via MQTT discovery as a sensor or binary_sensor.
3. Update firmware from HTTPS server via ESP-NOW.
4. Direct or mesh work mode.

## Notes

1. Work mode must be same with [gateway](http://git.zh.com.ru/alexey.zholtikov/zh_gateway) work mode.
2. ESP-NOW mesh network based on the [zh_network](http://git.zh.com.ru/alexey.zholtikov/zh_network).
3. For initial settings use "menuconfig -> ZH ESP-NOW Sensor Configuration". After first boot all settings will be stored in NVS memory for prevente change during OTA firmware update.
4. To restart the sensor, send the "restart" command to the root topic of the sensor (example - "homeassistant/espnow_sensor/24-62-AB-F9-1F-A8").
5. To update the sensor firmware, send the "update" command to the root topic of the sensor (example - "homeassistant/espnow_sensor/70-03-9F-44-BE-F7"). The update path should be like as "https://your_server/zh_espnow_sensor_esp32.bin" (for ESP32) or "https://your_server/zh_espnow_sensor_esp8266.app1.bin + https://your_server/zh_espnow_sensor_esp8266.app2.bin" (for ESP8266). The time and success of the update depends on the load on WiFi channel 1. Average update time is up to five minutes. The online status of the update is displayed in the root sensor topic.
6. Only one sensor can be used at a time.

## Build and flash

Run the following command to firmware build and flash module:

```text
cd your_projects_folder
git clone http://git.zh.com.ru/alexey.zholtikov/zh_espnow_sensor.git
cd zh_espnow_sensor
git submodule init
git submodule update --remote
cd zh_espnow_sensor
```

For ESP32 family:

```text
idf.py set-target (TARGET) // Optional
idf.py menuconfig
idf.py build
idf.py flash
```

For ESP8266 family:

```text
make menuconfig
make
make flash
```

## Attention

1. A gateway is required. For details see [zh_gateway](http://git.zh.com.ru/alexey.zholtikov/zh_gateway).
2. Use the "make ota" command instead of "make" to prepare 2 bin files for OTA update (for ESP8266).

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.
