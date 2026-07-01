#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

//----------------------------------------------------
// Clave compartida
//----------------------------------------------------
const uint8_t CLAVE[16] = {
  0x4C,0xAB,0x12,0xF3,
  0x9E,0x71,0x3D,0x08,
  0xCC,0x55,0xA2,0x6B,
  0x1F,0xE9,0x84,0x37
};

// <<< CAMBIO >>>
// Reemplazar por la MAC del ESP32 receptor
uint8_t macReceptor[] = {0x8C, 0x94, 0xDF, 0x8F, 0x31, 0x84};

// <<< CAMBIO >>>
// Número de secuencia
uint32_t secuencia = 0;

//----------------------------------------------------
// Estructura
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
// <<< CAMBIO >>>
// Callback de envío
//----------------------------------------------------
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.print("Estado envio: ");

  if(status==ESP_NOW_SEND_SUCCESS)
    Serial.println("OK");
  else
    Serial.println("ERROR");
}

//----------------------------------------------------
void enviarSeguro(const char *mensaje)
{
  PaqueteSeguro pkt;

  // <<< CAMBIO >>>
  // Verificar tamaño
  if(strlen(mensaje)>200){
    Serial.println("Mensaje demasiado largo");
    return;
  }

  pkt.longitud=strlen(mensaje);

  // <<< CAMBIO >>>
  pkt.id=++secuencia;

  xorCifrar(
      (uint8_t*)mensaje,
      pkt.payload,
      pkt.longitud,
      CLAVE,
      sizeof(CLAVE)
  );

  esp_now_send(macReceptor,(uint8_t*)&pkt,sizeof(pkt));

  Serial.printf("[TX #%u] %s\n",
                pkt.id,
                mensaje);
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
  esp_now_register_send_cb(onSent);

  // <<< CAMBIO >>>
  esp_now_peer_info_t peerInfo={};

  memcpy(peerInfo.peer_addr,
         macReceptor,
         6);

  peerInfo.channel=0;

  // <<< CAMBIO >>>
  // El cifrado lo hacemos manualmente
  peerInfo.encrypt=false;

  if(esp_now_add_peer(&peerInfo)!=ESP_OK){
    Serial.println("No se pudo agregar peer");
    return;
  }

  Serial.println("Transmisor listo");
}

void loop()
{
  enviarSeguro("Hola desde ESP32 6");

  delay(5000);
}