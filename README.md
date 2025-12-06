# esp32-cam-terrarium-monitoring
Terrarium environmental monitoring and control using ESP32-CAM + ESP32 node, DHT22, soil sensor, relays and servo.

# Terrarium Environmental Monitoring and Control using ESP32-CAM and ESP32 (IoT)

This project implements an IoT-based **terrarium monitoring and control system** using an **ESP32-CAM** as the main controller + web server and a secondary **ESP32 node** to drive actuators (light, fan, water pump).  
The ESP32-CAM streams video, hosts a web dashboard, reads **temperature/humidity** from a DHT22 sensor and **soil moisture** from an analog sensor, and automatically controls actuators through UART commands to the secondary ESP32.

## Features

- **Live video streaming** from ESP32-CAM:
  - MJPEG stream on `/stream` for real-time monitoring of the terrarium.
- **Web dashboard (ESP32-CAM HTTP server)**:
  - Single-page interface (HTML stored in `index_html.h`).
  - Shows live temperature, humidity and soil moisture.
  - Allows manual control of relays (light, fan, pump) via buttons.
  - Allows adjusting environment presets and threshold values from the browser.
- **Environmental sensing**:
  - Temperature & humidity via **DHT22**.
  - Soil moisture via analog sensor on `SOIL_PIN`, mapped to a percentage (0–100%).
- **Configurable thresholds with non-volatile storage**:
  - Thresholds for temperature, humidity and soil moisture are saved using **Preferences** (NVS).
  - Environment profile name (e.g. “Tropical”, “Desert”) is also stored in flash.
- **Automatic control loop** (every few seconds):
  - Compares current temperature and soil moisture with thresholds.
  - Sends commands like `RELAY1_ON`, `RELAY2_ON`, `RELAY3_ON` over UART to the ESP32 node to:
    - Turn **light** ON/OFF.
    - Turn **fan** ON/OFF.
    - Turn **water pump** ON/OFF for soil moisture.
- **Servo control**:
  - A small servo (e.g. SG90) is driven by ESP32-CAM PWM to a given angle.
  - Web UI can send commands to move the servo (e.g. to drop food or open a small door).
- **Secondary ESP32 node**:
  - Receives simple text commands via UART (`RELAY1_ON`, `RELAY2_OFF`, etc.).
  - Drives three GPIO outputs connected to relays for **light**, **fan** and **pump**.
  - Prints the received commands to the USB serial monitor for debugging.

## Hardware

- **Main controller**: ESP32-CAM module (camera + Wi-Fi)
- **Secondary node**: ESP32 DevKit (or similar)
- **Sensors**:
  - DHT22 – temperature & humidity
  - Soil moisture sensor (analog)
- **Actuators**:
  - Relay for **light** (RELAY1)
  - Relay for **fan** (RELAY2)
  - Relay for **water pump** (RELAY3)
  - Servo motor (feeding door or similar)
- **Power**:
  - 5 V supply for relays, sensors and ESP32 boards (with appropriate regulators)
- Other: wiring, breadboard or PCB, terrarium enclosure with light/fan/pump.

Typical pin usage (can be adjusted):

- ESP32-CAM:
  - DHT22: `GPIO14`
  - Soil sensor analog: `GPIO15`
  - Servo: `GPIO13` (PWM via LEDC)
  - UART2 TX to ESP32 node: e.g. `GPIO2` or `GPIO16` (depending on your wiring)
- ESP32 node:
  - UART RX from ESP32-CAM: `GPIO16`
  - Light relay: `GPIO5`
  - Fan relay: `GPIO18`
  - Pump relay: `GPIO19`

## Software & Tools

- **Framework**: Arduino core for ESP32
- **Main libraries (ESP32-CAM sketch)**:
  - `esp_camera.h` – camera driver
  - `WiFi.h` – Wi-Fi connection
  - `WebServer.h` and `esp_http_server.h` – HTTP server and MJPEG stream
  - `DHT.h` – DHT22 sensor
  - `Preferences.h` – store thresholds and environment profile in flash
- **Secondary node**:
  - `HardwareSerial.h` – UART communication
- IDE: Arduino IDE, PlatformIO or VS Code with Arduino/ESP32 extensions

> ⚠️ Tip: Before pushing to GitHub, replace real Wi-Fi SSID/password in the code by placeholders or move them to a separate `secrets.h` that is not committed.
Code Overview
## Getting the Web UI Source (index_html.h)

For the full web dashboard source code (`index_html.h`), please contact me directly:

- Email: letronghoang0604@gmail.com  
- Zalo: 0903240604  

I will be happy to share the `index_html.h` file and instructions on how to integrate it into the project.
