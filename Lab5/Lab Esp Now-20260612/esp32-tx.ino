#include <WiFi.h>
#include <esp_now.h>

#define ID_GRUPO 1

typedef struct
{
  uint8_t grupo;
  uint32_t secuencia;
  uint32_t timestamp;
} Paquete;

Paquete paquete;

// CAMBIAR POR LA MAC DEL RX
uint8_t receptor[] =
{
  0x8C,0x94,0xDF,0x8F,0x31,0x84
};

unsigned long proximoEnvio;

void OnDataSent(const wifi_tx_info_t *info,
                esp_now_send_status_t status)
{
  Serial.print("Estado envio: ");

  if(status == ESP_NOW_SEND_SUCCESS)
    Serial.println("OK");
  else
    Serial.println("FALLO");
}

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.println("TRANSMISOR");

  if(esp_now_init() != ESP_OK)
  {
    Serial.println("Error ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};

  memcpy(peerInfo.peer_addr,
         receptor,
         6);

  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if(esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Error Peer");
    return;
  }

  randomSeed(micros());

  proximoEnvio =
      millis() + random(1000,5000);
}

void loop()
{
  if(millis() >= proximoEnvio)
  {
    paquete.grupo = ID_GRUPO;
    paquete.secuencia++;
    paquete.timestamp = millis();

    esp_now_send(
      receptor,
      (uint8_t *)&paquete,
      sizeof(paquete)
    );

    Serial.print("Grupo ");
    Serial.print(paquete.grupo);

    Serial.print(" Seq ");
    Serial.println(paquete.secuencia);

    proximoEnvio =
      millis() + random(1000,5000);
  }
}
