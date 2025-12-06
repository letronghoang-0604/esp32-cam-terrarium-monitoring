#include <HardwareSerial.h>

HardwareSerial SerialNode(1);  // Use UART1

#define RXD1 16   // Receive from ESP32-CAM TX
#define TXD1 -1   // TX not used

#define LIGHT_PIN 5   // GPIO5 - Light relay
#define FAN_PIN   18  // GPIO18 - Fan relay
#define PUMP_PIN  19  // GPIO19 - Pump relay

String inputString = "";

void setup() {
  Serial.begin(115200);  // USB debug
  SerialNode.begin(9600, SERIAL_8N1, RXD1, TXD1);  // UART to receive data from ESP32-CAM

  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  // Relays are assumed active LOW: HIGH = OFF, LOW = ON
  digitalWrite(LIGHT_PIN, HIGH);
  digitalWrite(FAN_PIN, HIGH);
  digitalWrite(PUMP_PIN, HIGH);

  Serial.println("✅ ESP32 DevKit is ready.");
}

void loop() {
  while (SerialNode.available()) {
    char ch = SerialNode.read();

    // ✅ Option 1: filter out non-printable characters
    if ((ch >= 32 && ch <= 126) || ch == '\n') {
      inputString += ch;
    }

    // When a newline is received => process the command
    if (ch == '\n') {
      inputString.trim();

      // ✅ Option 2: remove any remaining invalid characters in the string
      String cleanedCmd = "";
      for (int i = 0; i < inputString.length(); i++) {
        char c = inputString.charAt(i);
        if (c >= 32 && c <= 126) cleanedCmd += c;
      }

      handleCommand(cleanedCmd);
      inputString = "";
    }
  }
}

void handleCommand(String cmd) {
  Serial.print("📥 Received command: ");
  Serial.println(cmd);

  if (cmd == "RELAY1_ON") {
    digitalWrite(LIGHT_PIN, LOW);
    Serial.println("💡 Light ON");
  } else if (cmd == "RELAY1_OFF") {
    digitalWrite(LIGHT_PIN, HIGH);
    Serial.println("💡 Light OFF");

  } else if (cmd == "RELAY2_ON") {
    digitalWrite(FAN_PIN, LOW);
    Serial.println("🌀 Fan ON");
  } else if (cmd == "RELAY2_OFF") {
    digitalWrite(FAN_PIN, HIGH);
    Serial.println("🌀 Fan OFF");

  } else if (cmd == "RELAY3_ON") {
    digitalWrite(PUMP_PIN, LOW);
    Serial.println("💧 Pump ON");
  } else if (cmd == "RELAY3_OFF") {
    digitalWrite(PUMP_PIN, HIGH);
    Serial.println("💧 Pump OFF");

  } else {
    Serial.println("⚠️ Invalid command");
  }
}
