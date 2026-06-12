#include <WiFi.h>
#include <esp_now.h>

typedef struct
{
  uint8_t grupo;
  uint32_t secuencia;
  uint32_t timestamp;
} Paquete;

void OnDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *incomingData,
  int len)
{
  Paquete p;

  memcpy(&p,
         incomingData,
         sizeof(p));

  Serial.print("Grupo ");

  Serial.print(p.grupo);

  Serial.print(" Seq ");

  Serial.print(p.secuencia);

  Serial.print(" Tiempo ");

  Serial.println(p.timestamp);
}
void setup()
{
  Serial.begin(115200);
  Serial.println("VERSION RX SIMPLE");
  delay(1000);  // ← punto y coma agregado

  WiFi.mode(WIFI_STA);
  delay(100);   // ← espera que el stack WiFi esté listo

  Serial.println();
  Serial.println("RECEPTOR");
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  if(esp_now_init() != ESP_OK)
  {
    Serial.println("Error ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}


void loop()
{
}
