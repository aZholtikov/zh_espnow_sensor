# ESP-NOW sensor

ESP-NOW based sensor for ESP32 ESP-IDF.

There are two branches - for ESP8266 family and for ESP32 family. Please use the appropriate one.

## Tested on

1. ESP32 ESP-IDF v5.1.0

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
5. To update the sensor firmware, send the "update" command to the root topic of the sensor (example - "homeassistant/espnow_sensor/70-03-9F-44-BE-F7"). The update path should be like as "https://your_server/zh_espnow_sensor_esp32.bin". The time and success of the update depends on the load on WiFi channel 1. Average update time is up to five minutes. The online status of the update is displayed in the root sensor topic.
6. Only one sensor can be used at a time.

## Build and flash

Run the following command to firmware build and flash module:

```text
cd your_projects_folder
bash <(curl -Ls http://git.zh.com.ru/alexey.zholtikov/zh_espnow_sensor/raw/branch/esp32/install.sh)
cd zh_espnow_sensor
idf.py menuconfig
idf.py all
idf.py -p (PORT) flash
```

## Attention

1. A gateway is required. For details see [zh_gateway](http://git.zh.com.ru/alexey.zholtikov/zh_gateway).

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.
