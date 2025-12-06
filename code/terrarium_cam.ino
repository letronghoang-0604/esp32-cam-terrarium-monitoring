// File: ESP32-CAM.ino (Web interface on port 80, MJPEG stream on port 81)

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "index_html.h" // Split web UI into a separate file
#include "esp_http_server.h"
#include <DHT.h>
#include <Preferences.h>

Preferences prefs;
int thres_temp = 0;
int thres_humi = 0;
int thres_soil = 0;
String current_env = "";
unsigned long lastSoilRead = 0;
int lastSoilPercent = 0;

#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#include "camera_pins.h"

// WiFi configuration
const char* ssid = "D906_TEC";
const char* password = "D906_TEC";
// Static IP configuration (must be valid: each value from 0 to 255)
IPAddress local_IP(172, 16, 36, 200);  // Desired static IP address
IPAddress gateway(172, 16, 36, 1);     // Router gateway address
IPAddress subnet(255, 255, 255, 0);    // Subnet mask

// ================= PIN CONFIGURATION =================
#define DHTPIN     14        // DHT22 data pin
#define SOIL_PIN   15        // Analog pin for soil moisture sensor
#define SERVO_PIN  13
#define SERVO_CHANNEL 3      // Use PWM channel 3 to avoid conflict with camera
int soil_raw = 0;
int soil_percent = 0;

#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
int servo_pos = 90; // Default angle
unsigned long lastAutoCheck = 0;
bool autoLightOn = false;
bool autoFanOn = false;
bool autoPumpOn = false;

WebServer server(80);
httpd_handle_t stream_httpd = NULL;

void startCameraServer();


esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }
    char buf[64];
    size_t len = snprintf(buf, 64,
      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    res = httpd_resp_send_chunk(req, buf, len);
    res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    res = httpd_resp_send_chunk(req, "\r\n", 2);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  Serial.begin(115200);
  // Initialize UART2: TX = GPIO2, RX not used
  Serial2.begin(9600, SERIAL_8N1, -1, 2); // RX = -1 (not used), TX = GPIO2

  prefs.begin("env_cfg", true);  // true = read-only
  thres_temp = prefs.getInt("temp", 0);
  thres_humi = prefs.getInt("humi", 0);
  thres_soil = prefs.getInt("soil", 0);
  current_env = prefs.getString("env", "");
  prefs.end();


  Serial.printf("Reloaded thresholds | Environment: %s | Temp: %d | Humi: %d | Soil: %d\n",
    current_env.c_str(), thres_temp, thres_humi, thres_soil);

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("⚠️ Static IP configuration failed!");
  }

  soil_raw = analogRead(SOIL_PIN);
  soil_percent = 87;
  Serial.printf("Soil: raw = %d → %d%%\n", soil_raw, soil_percent);

  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi connected");
  Serial.print("📡 Static IP: ");
  Serial.println(WiFi.localIP());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  dht.begin();
  // Detach flash function if GPIO13 was previously used for flash
  pinMode(SERVO_PIN, OUTPUT);
  digitalWrite(SERVO_PIN, LOW);
  ledcDetachPin(SERVO_PIN);   // Very important if GPIO13 was used by flash

  // Configure PWM for servo
  ledcSetup(SERVO_CHANNEL, 50, 16);         // 50 Hz, 16-bit resolution
  ledcAttachPin(SERVO_PIN, SERVO_CHANNEL);  // Attach PWM channel to servo pin

  // Move servo to initial angle (90°)
  int duty = map(servo_pos, 0, 180, 1638, 7864);  // 0.5 ms to 2.4 ms
  ledcWrite(SERVO_CHANNEL, duty);
  Serial.println("Servo initialized to 90°");

  delay(500);                     // Wait for servo to move (optional, 0.5 s)


  server.on("/", HTTP_GET, []() {
    String html(index_html);
    html.replace("%IP%", WiFi.localIP().toString());
    server.send(200, "text/html", html);
  });

  // Handle servo control commands from web
  server.on("/cmd", HTTP_GET, []() {
    String act = server.arg("act");
    if (act == "rotate_left") {
      servo_pos = constrain(servo_pos - 15, 0, 180);
    } else if (act == "rotate_right") {
      servo_pos = constrain(servo_pos + 15, 0, 180);
    }

    int duty = map(servo_pos, 0, 180, 1638, 7864);  // PWM for SG90: 0.5 ms to 2.4 ms
    ledcWrite(SERVO_CHANNEL, duty);
    Serial.printf("Servo moved to %d°\n", servo_pos);

    server.send(200, "text/plain", "OK");
  });


  // Adjust and save thresholds
  server.on("/setenv", HTTP_GET, []() {
    thres_temp = server.arg("t").toInt();
    thres_humi = server.arg("h").toInt();
    thres_soil = server.arg("s").toInt();
    current_env = server.arg("env");

    // Write to flash (NVS)
    prefs.begin("env_cfg", false);  // false = write mode
    prefs.putInt("temp", thres_temp);
    prefs.putInt("humi", thres_humi);
    prefs.putInt("soil", thres_soil);
    prefs.putString("env", current_env);
    prefs.end();

    Serial.printf("Thresholds saved | Environment: %s | Temp: %d | Humi: %d | Soil: %d\n",
      current_env.c_str(), thres_temp, thres_humi, thres_soil);
    server.send(200, "text/plain", "Thresholds saved");
  });

  // Send sensor data to web UI
  server.on("/getdata", HTTP_GET, []() {
    float t = dht.readTemperature();       // Read temperature from DHT22
    float h = dht.readHumidity();          // Read humidity from DHT22

    String json = "{";
    json += "\"temp\":" + String(t, 1) + ",";
    json += "\"humi\":" + String(h, 1) + ",";
    json += "\"soil\":" + String(soil_percent);
    json += "}";

    Serial.printf("DHT22 | Temp: %.1f°C | Humi: %.1f%% | Soil Moisture: %d%%\n", t, h, soil_percent);

    Serial.println("JSON data sent to web:");
    Serial.println(json);

    server.send(200, "application/json", json);
  });

  server.on("/cmd", HTTP_GET, []() {
    String act = server.arg("act");
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    Serial.print("🔁 Web requested URI: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not found");
  });

  server.on("/relay", HTTP_GET, []() {
    int id = server.arg("id").toInt();
    String state = server.arg("state");  // "on" or "off"

    if (id >= 1 && id <= 3 && (state == "on" || state == "off")) {
      // Send over UART2: e.g. "RELAY1_ON\n"
      String uartCmd = "RELAY" + String(id) + "_" + (state == "on" ? "ON" : "OFF") + "\n";
      Serial2.print(uartCmd);
      Serial.printf("📤 UART sent: %s", uartCmd.c_str());
      server.send(200, "text/plain", "UART sent");
    } else {
      server.send(400, "text/plain", "Invalid ID or state");
    }
  });

  server.begin();
  startCameraServer();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastAutoCheck > 3000) {  // every 3 seconds
    lastAutoCheck = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int soil = analogRead(SOIL_PIN);
    int soil_percent = map(soil, 0, 4095, 0, 100);

    Serial.printf("🌡️ Auto check | Temp: %.1f°C | Humi: %.1f%% | Soil: %d%%\n", t, h, soil_percent);

    // Control light and fan (RELAY1, RELAY2) based on low/high temperature
    if (t < thres_temp - 2) {
      if (!autoLightOn) {
        Serial2.print("RELAY1_ON\n");
        Serial.println("🔆 Auto TURN ON light");
        autoLightOn = true;
      }
    } else if (t > thres_temp + 2) {
      if (autoLightOn) {
        Serial2.print("RELAY2_ON\n");
        Serial.println("🔆 Auto TURN ON fan");
        autoLightOn = false;
      }
    } else if (thres_temp - 2 < t < thres_temp + 2) {
      if (autoLightOn) {
        Serial2.print("RELAY1_OFF\n");
        Serial2.print("RELAY2_OFF\n");
        Serial.println("🔆 Auto TURN OFF light and fan");
        autoLightOn = false;
      }
    }

    // Control fan (RELAY2) when air humidity is high
    if (h > thres_humi + 5) {
      if (!autoFanOn) {
        Serial2.print("RELAY2_ON\n");
        Serial.println("💨 Auto TURN ON fan (humidity high)");
        autoFanOn = true;
      }
    } else if (h <= thres_humi) {
      if (autoFanOn) {
        Serial2.print("RELAY2_OFF\n");
        Serial.println("💨 Auto TURN OFF fan (humidity normal)");
        autoFanOn = false;
      }
    }

    // Control pump (RELAY3) when soil is dry
    if (soil_percent < thres_soil - 10) {
      if (!autoPumpOn) {
        Serial2.print("RELAY3_ON\n");
        Serial.println("💧 Auto TURN ON pump (soil dry)");
        autoPumpOn = true;
      }
    } else if (soil_percent >= thres_soil) {
      if (autoPumpOn) {
        Serial2.print("RELAY3_OFF\n");
        Serial.println("💧 Auto TURN OFF pump (soil OK)");
        autoPumpOn = false;
      }
    }
  }
}
