#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Fundamental: enciende el radio en modo Estación
  delay(100);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}