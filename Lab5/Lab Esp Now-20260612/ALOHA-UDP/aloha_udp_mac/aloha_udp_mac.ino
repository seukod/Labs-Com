#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>        // <-- necesario para fijar la MAC

// =====================================================================
//  CLIENTE ALOHA (capa de aplicacion) sobre Wi-Fi + UDP
//  Se conecta a "ALOHA-LAB" con una MAC personalizada y envia a 192.168.4.1
// =====================================================================

// ---- CONFIGURACION ----
const char* SSID_RED   = "ALOHA-LAB";
const char* PASSWORD   = "12345678";
IPAddress   SERVER_IP(192, 168, 4, 1);
const uint16_t SERVER_PORT = 5000;    // <-- puerto del servidor (CONFIRMAR)
const uint16_t LOCAL_PORT  = 5001;

// MAC con la que se presentara este ESP32 al conectarse
uint8_t MI_MAC[] = {0x8C, 0x94, 0xDF, 0x4C, 0xBE, 0x2C};

#define MI_GRUPO          6
#define TIMEOUT_ACK_MS    100
#define BACKOFF_MIN_MS    20
#define BACKOFF_BASE_MS   50
#define MAX_REINTENTOS    5

WiFiUDP udp;
uint32_t seq = 0;

void conectarWifi() {
  WiFi.mode(WIFI_STA);

  // Fijar la MAC ANTES de begin() para que el AP la vea al asociarse.
  esp_err_t r = esp_wifi_set_mac(WIFI_IF_STA, MI_MAC);
  if (r != ESP_OK) {
    Serial.printf("Aviso: no se pudo fijar la MAC (err 0x%X)\n", r);
  }

  WiFi.begin(SSID_RED, PASSWORD);
  Serial.print("Conectando a ");
  Serial.print(SSID_RED);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("MAC en uso: ");
  Serial.println(WiFi.macAddress());     // verifica que quedo la personalizada
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
  udp.begin(LOCAL_PORT);
}

bool esperarAck(uint32_t timeout) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeout) {
    int sz = udp.parsePacket();
    if (sz > 0) {
      char buf[64];
      int n = udp.read(buf, sizeof(buf) - 1);
      if (n > 0) buf[n] = '\0';
      return true;
    }
  }
  return false;
}

bool enviarAloha(uint32_t numeroSeq) {
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

    uint32_t ventana = BACKOFF_BASE_MS * (1UL << intento);
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
  randomSeed(esp_random());
  conectarWifi();
}

void loop() {
  enviarAloha(++seq);
  delay(random(1000, 5000));
}
