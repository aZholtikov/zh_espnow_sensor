# ESP-NOW sensor

ESP-NOW based sensor for ESP32 ESP-IDF and ESP8266 RTOS SDK.

## Tested on

1. ESP8266 RTOS_SDK v3.4
2. ESP32 ESP-IDF v5.2

## Features

1. Supported 1-wire sensors:
    1. [DS18B20](https://github.com/aZholtikov/zh_ds18b20)
    2. [DHT11/DHT22/AM2302/AM2320](https://github.com/aZholtikov/zh_dht)
2. Supported I2C sensors:
    1. [AM2320](https://github.com/aZholtikov/zh_dht)
    2. [BH1750](https://github.com/aZholtikov/zh_bh1750)
    3. [AHT20/AHT21](https://github.com/aZholtikov/zh_aht)
3. Optional support sensor power management (for 1-wire sensors only).
4. Automatically adds sensor configuration to Home Assistan via MQTT discovery as a sensor.
5. Update firmware from HTTPS server via ESP-NOW.
6. Direct or mesh work mode.
7. External or battery powering.

## Notes

1. Work mode must be same with [gateway](https://github.com/aZholtikov/zh_gateway) work mode.
2. ESP-NOW mesh network based on the [zh_network](https://github.com/aZholtikov/zh_network).
3. For initial settings use "menuconfig -> ZH ESP-NOW Sensor Configuration". After first boot all settings will be stored in NVS memory for prevente change during OTA firmware update.
4. Only one sensor can be used at a time.
5. To restart the sensor, send the "restart" command to the root topic of the sensor (example - "homeassistant/espnow_sensor/24-62-AB-F9-1F-A8").
6. To update the sensor firmware, send the "update" command to the root topic of the sensor (example - "homeassistant/espnow_sensor/70-03-9F-44-BE-F7"). The update path should be like as "https://your_server/zh_espnow_sensor_esp32.bin" (for ESP32) or "https://your_server/zh_espnow_sensor_esp8266.app1.bin + https://your_server/zh_espnow_sensor_esp8266.app2.bin" (for ESP8266). Average update time is up to some minutes. The online status of the update is displayed in the root sensor topic.
7. To change initial settings of the sensor (except work mode and power selection GPIO), send the X1,X2,X3,X4,X5,X6 command to the hardware topic of the sensor (example - "homeassistant/espnow_sensor/70-03-9F-44-BE-F7/hardware"). The configuration will only be accepted if it does not cause errors. The current configuration status is displayed in the configuration topic of the sensor (example - "homeassistant/espnow_sensor/70-03-9F-44-BE-F7/config").

MQTT configuration message should filled according to the template "X1,X2,X3,X4,X5,X6". Where:

1. X1 - Sensor type. 1 for DS18B20, 2 for AHT, 8 for DHT, 9 for BH1750.
2. X2 - Sensor GPIO number 1 (main pin for 1-wire sensors, SDA pin for I2C sensors). 0 - 48 (according to the module used), 255 if not used.
3. X3 - Sensor GPIO number 2 (SCL pin for I2C sensors). 0 - 48 (according to the module used), 255 if not used.
4. X4 - Power GPIO number (if using sensor power control). 0 - 48 (according to the module used), 255 if not used.
5. X5 - Measurement frequency (sleep time on battery power). 1 - 65536 seconds.
6. X6 - Battery powered. 0 if false, 1 if true.

## Build and flash

Run the following command to firmware build and flash module:

```text
cd your_projects_folder
git clone --recurse-submodules https://github.com/aZholtikov/zh_espnow_sensor.git
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

1. A gateway is required. For details see [zh_gateway](https://github.com/aZholtikov/zh_gateway).
2. Use the "make ota" command instead of "make" to prepare 2 bin files for OTA update (for ESP8266).
3. For ESP8266 battery powering mode GPIO16 must be connected to RST.
4. Updates and setting changes are only available in external power mode. To temporarily disable battery mode, enable "menuconfig -> ZH ESP-NOW Sensor Configuration -> Enable power mode selection at startup" and set the "Power selection GPIO number". Connecting this GPIO to GND during startup will temporarily disable the battery mode.

Thanks to [Rajeev Tandon](https://github.com/rajtan) for some good advice.

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.
