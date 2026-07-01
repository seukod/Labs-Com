#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

//----------------------------------------------------
const uint8_t CLAVE[16] = {
  0x4C,0xAB,0x12,0xF3,
  0x9E,0x71,0x3D,0x08,
  0xCC,0x55,0xA2,0x6B,
  0x1F,0xE9,0x84,0x37
};

//----------------------------------------------------
typedef struct __attribute__((packed)) {
  uint8_t payload[200];
  uint8_t longitud;
  uint32_t id;
} PaqueteSeguro;

//----------------------------------------------------
void xorCifrar(const uint8_t *entrada,
               uint8_t *salida,
               size_t lon,
               const uint8_t *clave,
               size_t lonClave)
{
  for(size_t i=0;i<lon;i++)
    salida[i]=entrada[i]^clave[i%lonClave];
}

//----------------------------------------------------
void onRecibido(const esp_now_recv_info_t *info,
                const uint8_t *datos,
                int len)
{
  // <<< CAMBIO >>>
  // Validar tamaño
  if(len!=sizeof(PaqueteSeguro)){
    Serial.println("Paquete invalido");
    return;
  }

  PaqueteSeguro *pkt=(PaqueteSeguro*)datos;

  uint8_t claro[201]={0};

  xorCifrar(
      pkt->payload,
      claro,
      pkt->longitud,
      CLAVE,
      sizeof(CLAVE)
  );

  Serial.printf("[RX #%u] %s\n",
                pkt->id,
                claro);
}

//----------------------------------------------------
void setup()
{
  Serial.begin(115200);

  // <<< CAMBIO >>>
  WiFi.mode(WIFI_STA);

  // <<< CAMBIO >>>
  if(esp_now_init()!=ESP_OK){
    Serial.println("Error ESP-NOW");
    return;
  }

  // <<< CAMBIO >>>
  esp_now_register_recv_cb(onRecibido);

  Serial.println("Receptor listo");
}

void loop()
{
}