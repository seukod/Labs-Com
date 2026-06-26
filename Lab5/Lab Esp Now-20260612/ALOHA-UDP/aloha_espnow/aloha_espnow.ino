#include <WiFi.h>
#include <esp_now.h>

// =====================================================================
//  NODO ALOHA sobre ESP-NOW (por MAC)
//  Modelo: TRANSMIT -> esperar ACK -> si FALLO/timeout -> BACKOFF
//          aleatorio -> reintentar (hasta MAX_REINTENTOS) -> descartar.
//  El ACK es el status del callback de envio:
//     ESP_NOW_SEND_SUCCESS = entregado (hubo ACK 802.11)
//     ESP_NOW_SEND_FAIL    = sin ACK   = colision/perdida
// =====================================================================

#define ID_GRUPO          6

#define TIMEOUT_ACK_MS    100     // espera maxima del resultado del callback
#define BACKOFF_MIN_MS    20
#define BACKOFF_BASE_MS   50      // base del backoff exponencial: 50,100,200...
#define MAX_REINTENTOS    5

typedef struct {
  uint8_t  grupo;
  uint32_t secuencia;
  uint32_t timestamp;
} Paquete;
Paquete paquete;

// CAMBIAR POR LA MAC DEL RX (todos los nodos ALOHA apuntan al mismo RX)
uint8_t receptor[] = { 0x8C, 0x94, 0xDF, 0x4C, 0xBE, 0x24 };

unsigned long proximoEnvio;

// Estadisticas para el informe
uint32_t entregados  = 0;
uint32_t descartados = 0;

// ---- Banderas callback <-> loop ----
volatile bool resultadoListo = false;
volatile esp_now_send_status_t ultimoStatus;

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  ultimoStatus   = status;     // guardamos el resultado
  resultadoListo = true;       // avisamos al loop (nuestro "ACK / NACK")
}
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Paquete)) return;
  Paquete r;
  memcpy(&r, data, sizeof(r));
  Serial.printf("[RX] de Grupo %d Seq %lu\n", r.grupo, r.secuencia);
}

// ---- Envio ALOHA de un dato: devuelve true si se confirmo ----
bool enviarAloha() {
  paquete.grupo     = ID_GRUPO;
  paquete.secuencia++;                 // un nuevo dato (los reintentos reusan el seq)
  uint32_t seqActual = paquete.secuencia;

  for (int intento = 0; intento <= MAX_REINTENTOS; intento++) {
    paquete.timestamp = millis();
    resultadoListo = false;

    esp_now_send(receptor, (uint8_t *)&paquete, sizeof(paquete));
    Serial.printf("[TX] Grupo %d Seq %lu (intento %d)\n", paquete.grupo, seqActual, intento);

    // Esperar el resultado del callback. delay(1) cede CPU a la tarea Wi-Fi
    // para que el callback pueda ejecutarse.
    uint32_t t0 = millis();
    while (!resultadoListo && (millis() - t0 < TIMEOUT_ACK_MS)) {
      delay(1);
    }

    if (resultadoListo && ultimoStatus == ESP_NOW_SEND_SUCCESS) {
      Serial.printf("[ACK] Seq %lu confirmado\n", seqActual);
      entregados++;
      return true;
    }

    // Sin ACK -> colision/perdida -> backoff exponencial aleatorio
    uint32_t ventana = BACKOFF_BASE_MS * (1UL << intento);   // 50,100,200,400...
    uint32_t backoff = random(BACKOFF_MIN_MS, ventana);
    Serial.printf("[COLISION] Seq %lu sin ACK, backoff %lu ms\n", seqActual, backoff);
    delay(backoff);
  }

  Serial.printf("[DESCARTADO] Seq %lu tras %d reintentos\n", seqActual, MAX_REINTENTOS);
  descartados++;
  return false;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println("NODO ALOHA (ESP-NOW)");

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receptor, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error Peer");
    return;
  }

  randomSeed(esp_random());            // semilla distinta por placa: clave en ALOHA
  proximoEnvio = millis() + random(1000, 5000);
}

void loop() {
  if (millis() >= proximoEnvio) {
    enviarAloha();
    Serial.printf("    (entregados=%lu  descartados=%lu)\n", entregados, descartados);
    proximoEnvio = millis() + random(1000, 5000);   // trafico aleatorio
  }
}
