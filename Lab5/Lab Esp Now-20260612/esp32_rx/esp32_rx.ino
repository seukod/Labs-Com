#include <WiFi.h>
#include <esp_now.h>

typedef struct
{
  uint8_t grupo;
  uint32_t secuencia;
  uint32_t timestamp;
} Paquete;

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len)
{
  Paquete p;
  memcpy(&p, incomingData, sizeof(p));

  // 'static' hace que la variable conserve su valor en la siguiente ejecución de la función
  static uint32_t timestampAnterior = 0; 
  uint32_t diffTiempo = 0;

  // Calculamos la diferencia si no es el primer paquete que recibimos
  if (timestampAnterior != 0) {
    diffTiempo = p.timestamp - timestampAnterior;
  }

  // Imprimir los datos recibidos
  Serial.print("Grupo ");
  Serial.print(p.grupo);
  Serial.print(" Seq ");
  Serial.print(p.secuencia);
  Serial.print(" Tiempo ");
  Serial.print(p.timestamp);
  
  // Imprimir el Time Diff
  Serial.print(" | Diff: ");
  if (timestampAnterior == 0) {
    Serial.println("--- ms (Primer paquete)");
  } else {
    Serial.print(diffTiempo);
    Serial.println(" ms");
  }

  // Guardar el timestamp actual para el cálculo del próximo paquete
  timestampAnterior = p.timestamp;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("VERSION RX SIMPLE");
  delay(1000); 

  WiFi.mode(WIFI_STA);
  delay(100); 

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
  // El loop se queda vacío porque ESP-NOW funciona por interrupciones (callbacks)
}