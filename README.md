# BLE IoT Temperature Monitoring System

This is an implementation of IoT temperature monitoring system that connects multiple embedded devices (ESP32, nRF52840) to cloud services for monitoring, telemetry, and data processing.

# ARCHITECTURE

[BME280] --I2C--> [nRF52840] --BLE--> [ESP32] --MQTT--> [Mosquitto] -> [Telegraf] -> [InfluxDB] -> [Grafana]

# COMPONENTS

## Temperature measurement 

I used widely known BME280 sensor for temperature measurement. Sensor sends its readings over I2C to Nordic nRF52840 (Zephyr based). I used BLE protocol to broadcast temperature to IoT gateway.

## IoT Gateway (MQTT publisher)

For this purpose, I used ESP32. This powerful chip fetches the temperature readings over BLE, packs it by using MQTT and publishes it to "cloud" (my local server) over WiFi.

## "Cloud"

I used Linux Virtual Machine and Docker to setup Mosquitto (MQTT broker), InfluxDB (database), Telegraf (MQTT subscriber) and Grafana (visualisation). Docker Compose gives elegance and encapsulation.

## Challenges
- Memory alignment of nRF image wasn't good, because I used cheap Aliexpress clone. I had to reverse-engineer proper one, which I dug on GitHub.
- I2C readings weren't working when jumper wire was hanging on RST pin of nRF chip. Took me couple of hours to figure out.
- Setup of VSCode extensions for nRF and ESP32.
- MQTT support for ESP32 