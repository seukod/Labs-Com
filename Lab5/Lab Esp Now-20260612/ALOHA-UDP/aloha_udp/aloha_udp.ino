#include <WiFi.h>
#include <WiFiUdp.h>

// =====================================================================
//  CLIENTE ALOHA (capa de aplicacion) sobre Wi-Fi + UDP
//  Se conecta a la red "ALOHA-LAB" y transmite al servidor 192.168.4.1
//
//  Modelo: TRANSMIT -> esperar ACK -> si no llega -> BACKOFF aleatorio
//          -> reintentar (hasta MAX_REINTENTOS) -> descartar.
// =====================================================================

// ---- CONFIGURACION (ajusta segun tu laboratorio) ----
const char* SSID_RED   = "ALOHA-LAB";
const char* PASSWORD   = "12345678";          // <-- pon la clave si la red la pide
IPAddress   SERVER_IP(192, 168, 4, 1);
const uint16_t SERVER_PORT = 5000;    // <-- puerto del servidor (CONFIRMAR)
const uint16_t LOCAL_PORT  = 5001;    // puerto local para recibir el ACK

#define MI_GRUPO          6
#define TIMEOUT_ACK_MS    100         // cuanto esperar el ACK antes de asumir colision
#define BACKOFF_MIN_MS    20
#define BACKOFF_BASE_MS   50          // base para backoff exponencial
#define MAX_REINTENTOS    5

WiFiUDP udp;
uint32_t seq = 0;

// ---- Conexion a la red ----
void conectarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID_RED, PASSWORD);
  Serial.print("Conectando a ");
  Serial.print(SSID_RED);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP local: ");
  Serial.println(WiFi.localIP());
  udp.begin(LOCAL_PORT);
}

// ---- Espera un ACK del servidor durante 'timeout' ms ----
bool esperarAck(uint32_t timeout) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeout) {
    int sz = udp.parsePacket();
    if (sz > 0) {
      char buf[64];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n > 0) buf[n] = '\0';
      // Opcional: validar que el ACK corresponde a 'seq'
      return true;
    }
  }
  return false;   // sin respuesta => asumimos colision/perdida
}

// ---- Envio ALOHA de un paquete: devuelve true si fue confirmado ----
bool enviarAloha(uint32_t numeroSeq) {
  // Carga util. AJUSTA el formato al que espera tu servidor.
  char payload[32];
  snprintf(payload, sizeof(payload), "G%d SEQ %lu", MI_GRUPO, numeroSeq);

  for (int intento = 0; intento <= MAX_REINTENTOS; intento++) {
    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.write((const uint8_t*)payload, strlen(payload));
    udp.endPacket();
    Serial.printf("[TX] %s (intento %d)\n", payload, intento);

    if (esperarAck(TIMEOUT_ACK_MS)) {
      Serial.printf("[ACK] Seq %lu confirmado\n", numeroSeq);
      return true;
    }

    // Sin ACK -> backoff exponencial aleatorio antes de reintentar
    uint32_t ventana = BACKOFF_BASE_MS * (1UL << intento);   // 50,100,200,...
    uint32_t backoff = random(BACKOFF_MIN_MS, ventana);
    Serial.printf("[COLISION] Seq %lu sin ACK, backoff %lu ms\n", numeroSeq, backoff);
    delay(backoff);
  }

  Serial.printf("[DESCARTADO] Seq %lu tras %d reintentos\n", numeroSeq, MAX_REINTENTOS);
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  randomSeed(esp_random());   // semilla distinta por placa, clave para ALOHA
  conectarWifi();
}

void loop() {
  // Genera trafico: un paquete y luego espera un intervalo aleatorio (Actividad 6)
  enviarAloha(++seq);
  delay(random(1000, 5000));
}
