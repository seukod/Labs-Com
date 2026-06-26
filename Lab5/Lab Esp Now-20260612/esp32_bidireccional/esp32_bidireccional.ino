#include <WiFi.h>
#include <esp_now.h>

// =====================================================================
//  ACTIVIDAD 11 - COMUNICACION BIDIRECCIONAL ESP-NOW
//  Compatible con Arduino-ESP32 core 3.x (IDF 5.x)
//
//  >>> LO UNICO QUE CAMBIAS ENTRE LOS DOS ESP32 SON ESTAS 2 LINEAS <<<
//
//  En ESP32-A:  MI_GRUPO = 1  y  macPeer = MAC de B (8C:94:DF:61:3A:A8)
//  En ESP32-B:  MI_GRUPO = 2  y  macPeer = MAC de A (8C:94:DF:4C:B6:3C)
// =====================================================================

#define MI_GRUPO       6
uint8_t macPeer[] = {0x8C,0x94,0xDF,0X4C,0XBE,0X2C}; // MAC del OTRO ESP32

#define INTERVALO_MS   3000   // periodo de transmision

// ---- Estructura de datos (igual en ambos nodos) ----
typedef struct {
  uint8_t  grupo;
  uint32_t secuencia;
  uint32_t timestamp;
} Paquete;

// ---- Variables compartidas entre el callback y el loop ----
// El callback de recepcion corre en el contexto de la tarea Wi-Fi:
// NO se imprime ni se hace nada bloqueante dentro de el.
// Solo se copia el dato y se levanta una bandera.
volatile bool hayPaquete = false;
Paquete       bufferRx;            // ultimo paquete recibido
uint32_t      seqEnvio = 0;

// ---- Callback de RECEPCION (firma core 3.x) ----
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(Paquete)) {
    memcpy((void *)&bufferRx, data, sizeof(Paquete));
    hayPaquete = true;
  }
}

// ---- Callback de ENVIO (firma core 3.x: wifi_tx_info_t) ----
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // En la actividad bidireccional solo nos interesa que salga.
  // (En ALOHA este status sera nuestra senal de ACK / colision.)
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.println();
  Serial.print("Soy Grupo ");
  Serial.print(MI_GRUPO);
  Serial.print("  MAC propia: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error ESP-NOW");
    return;
  }

  // Ambos callbacks
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  // Registrar el peer (el otro dispositivo)
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macPeer, 6);
  peer.channel = 0;        // 0 = canal actual del Wi-Fi
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Error al agregar peer");
    return;
  }
}

void loop() {
  // 1) Procesar lo recibido FUERA del callback
  if (hayPaquete) {
    hayPaquete = false;
    Paquete p;
    memcpy(&p, (const void *)&bufferRx, sizeof(Paquete)); // copia local
    Serial.print("[RX] Recibido de Grupo ");
    Serial.print(p.grupo);
    Serial.print(" Seq ");
    Serial.println(p.secuencia);
  }

  // 2) Transmitir periodicamente sin bloquear (millis, no delay)
  static uint32_t ultimoEnvio = 0;
  uint32_t ahora = millis();
  if (ahora - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = ahora;

    Paquete tx;
    tx.grupo     = MI_GRUPO;
    tx.secuencia = ++seqEnvio;
    tx.timestamp = ahora;

    esp_now_send(macPeer, (uint8_t *)&tx, sizeof(tx));
    Serial.print("[TX] Enviando: Grupo ");
    Serial.print(tx.grupo);
    Serial.print(" Seq ");
    Serial.println(tx.secuencia);
  }
}
