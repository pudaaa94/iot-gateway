# BLE IoT Temperature Monitoring System

This is an implementation of IoT temperature monitoring system that connects multiple embedded devices (ESP32, nRF52840) to cloud services for monitoring, telemetry, and data processing.

# ARCHITECTURE

[BME280] --I2C--> [nRF52840] --BLE--> [ESP32] --MQTT--> [Mosquitto] -> [Telegraf] -> [InfluxDB] -> [Grafana]

# COMPONENTS

## Temperature measurement 

The system uses a BME280 sensor for temperature measurement via I2C. Sensor sends its readings over I2C to Nordic nRF52840 (Zephyr based). I used BLE protocol to broadcast temperature to IoT gateway.

## IoT Gateway (MQTT publisher)

For this purpose, I used ESP32. This powerful chip fetches the temperature readings over BLE, packs it by using MQTT and publishes it to "cloud" (my local server) over WiFi.

## "Cloud"

I used Linux Virtual Machine and Docker Compose to setup the cloud stack, including Mosquitto (MQTT broker), Telegraf (MQTT consumer), InfluxDB (time-series database), and Grafana (visualization).

## Challenges
- Handling memory layout issues on low-cost nRF52840 board
- I2C readings weren't working when jumper wire was hanging on RST pin of nRF chip. Took me couple of hours to figure out.
- Setting up ESP-IDF and Zephyr development environments in VSCode