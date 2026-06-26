// ============================================================
//  RX - RECEPTOR  (cifrado hibrido RSA-2048 + AES-128 / ESP-NOW)
// ------------------------------------------------------------
//  - Al arrancar genera su par RSA-2048 e imprime la PUBLICA
//    (N y E en hex) por Serial. Copia esos valores en el TX.
//  - Recibe la clave AES cifrada con RSA en 2 fragmentos (porque
//    256 B no caben en una trama ESP-NOW de 250 B), la reensambla
//    y la descifra con su PRIVADA.
//  - Recibe el mensaje cifrado con AES-128-CBC y lo descifra.
// ============================================================
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

#include "mbedtls/rsa.h"
#include "mbedtls/aes.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/version.h"

//----------------------------------------------------
enum TipoMsg : uint8_t { MSG_CLAVE_AES = 1, MSG_DATOS = 2 };

typedef struct __attribute__((packed)) {
  uint8_t  tipo;          // TipoMsg
  uint8_t  frag;          // indice de fragmento de la clave RSA (0 o 1)
  uint8_t  totalFrag;     // total de fragmentos (2)
  uint8_t  iv[16];        // IV de AES (solo en MSG_DATOS)
  uint16_t longitud;      // bytes validos en payload
  uint8_t  payload[200];  // 128 B de clave RSA cifrada, o mensaje AES-CBC
} Paquete;                // ~223 bytes -> cabe en ESP-NOW

//----------------------------------------------------
mbedtls_rsa_context      rsa;       // par propio (priv + pub)
mbedtls_entropy_context  entropia;
mbedtls_ctr_drbg_context drbg;

uint8_t  claveAES[16];              // recuperada via RSA
bool     claveAESlista = false;

uint8_t  bufClaveCif[256];          // reensamblado de los 2 fragmentos
bool     fragRecibido[2] = {false, false};

//----------------------------------------------------
// Quita el padding PKCS7. Devuelve la longitud real o -1 si invalido.
int quitarPKCS7(uint8_t *datos, size_t len)
{
  if(len == 0 || (len % 16) != 0) return -1;
  uint8_t pad = datos[len - 1];
  if(pad == 0 || pad > 16 || pad > len) return -1;
  for(size_t i = len - pad; i < len; i++)
    if(datos[i] != pad) return -1;
  return (int)(len - pad);
}

//----------------------------------------------------
void imprimirPublica()
{
  mbedtls_mpi N, E;
  mbedtls_mpi_init(&N);
  mbedtls_mpi_init(&E);

  // Exportar N y E de forma portable entre versiones de mbedTLS
  mbedtls_rsa_export(&rsa, &N, NULL, NULL, NULL, &E);

  char buf[600];
  size_t olen;

  Serial.println("\n===== CLAVE PUBLICA RSA-2048 (copiar al TX) =====");

  mbedtls_mpi_write_string(&N, 16, buf, sizeof(buf), &olen);
  Serial.print("RX_N = \"");
  Serial.print(buf);
  Serial.println("\";");

  mbedtls_mpi_write_string(&E, 16, buf, sizeof(buf), &olen);
  Serial.print("RX_E = \"");
  Serial.print(buf);
  Serial.println("\";");

  Serial.println("================================================\n");

  mbedtls_mpi_free(&N);
  mbedtls_mpi_free(&E);
}

//----------------------------------------------------
void recuperarClaveAES()
{
  size_t olen = 0;
  uint8_t salida[256];

  int ret =
#if MBEDTLS_VERSION_MAJOR >= 3
    mbedtls_rsa_pkcs1_decrypt(&rsa, mbedtls_ctr_drbg_random, &drbg,
                              &olen, bufClaveCif, salida, sizeof(salida));
#else
    mbedtls_rsa_pkcs1_decrypt(&rsa, mbedtls_ctr_drbg_random, &drbg,
                              MBEDTLS_RSA_PRIVATE, &olen,
                              bufClaveCif, salida, sizeof(salida));
#endif

  if(ret != 0 || olen != 16){
    Serial.printf("Error descifrando clave AES (ret=-0x%04X, len=%u)\n",
                  -ret, (unsigned)olen);
    claveAESlista = false;
    return;
  }

  memcpy(claveAES, salida, 16);
  claveAESlista = true;
  Serial.println("[OK] Clave AES recuperada via RSA");
}

//----------------------------------------------------
void procesarDatos(const Paquete *pkt)
{
  if(!claveAESlista){
    Serial.println("Llego MSG_DATOS pero aun no hay clave AES; descartado");
    return;
  }
  if(pkt->longitud == 0 || (pkt->longitud % 16) != 0 ||
     pkt->longitud > sizeof(pkt->payload)){
    Serial.println("Longitud AES invalida");
    return;
  }

  uint8_t iv[16];
  memcpy(iv, pkt->iv, 16);            // crypt_cbc modifica el IV: usar copia

  uint8_t claro[200];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, claveAES, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, pkt->longitud,
                        iv, pkt->payload, claro);
  mbedtls_aes_free(&aes);

  int n = quitarPKCS7(claro, pkt->longitud);
  if(n < 0){
    Serial.println("Padding PKCS7 invalido (clave AES desfasada?)");
    return;
  }
  claro[n] = '\0';
  Serial.printf("[RX] Mensaje: %s\n", claro);
}

//----------------------------------------------------
void onRecibido(const esp_now_recv_info_t *info,
                const uint8_t *datos,
                int len)
{
  if(len != sizeof(Paquete)){
    Serial.println("Paquete invalido (tamano)");
    return;
  }
  const Paquete *pkt = (const Paquete*)datos;

  switch(pkt->tipo){
    case MSG_CLAVE_AES:
      if(pkt->frag > 1 || pkt->longitud != 128) return;
      memcpy(&bufClaveCif[pkt->frag * 128], pkt->payload, 128);
      fragRecibido[pkt->frag] = true;
      if(fragRecibido[0] && fragRecibido[1]){
        recuperarClaveAES();
        fragRecibido[0] = fragRecibido[1] = false;  // listo para el siguiente
      }
      break;

    case MSG_DATOS:
      procesarDatos(pkt);
      break;

    default:
      break;
  }
}

//----------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(1000);

  // ---- Prueba de Serial: si NO ves esto, el problema es el puerto/baud/USB-CDC ----
  for(int i = 0; i < 5; i++){
    Serial.printf(">>> Arrancando RX... %d\n", i);
    Serial.flush();
    delay(200);
  }

  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){
    Serial.println("Error ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(onRecibido);

  // RNG
  mbedtls_entropy_init(&entropia);
  mbedtls_ctr_drbg_init(&drbg);
  const char *pers = "rx_esp_now_rsa";
  if(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropia,
                           (const uint8_t*)pers, strlen(pers)) != 0){
    Serial.println("Error sembrando DRBG");
    return;
  }

  // Generar par RSA-2048 (puede tardar ~10-30 s)
  mbedtls_rsa_init(&rsa);
#if MBEDTLS_VERSION_MAJOR < 3
  // En mbedTLS 2.x el padding se fija aqui (init tiene otra firma)
  mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
#else
  mbedtls_rsa_set_padding(&rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
#endif

  Serial.println("Generando par RSA-2048... (puede tardar)");
  uint32_t t0 = millis();
  int ret = mbedtls_rsa_gen_key(&rsa, mbedtls_ctr_drbg_random, &drbg,
                                2048, 65537);
  if(ret != 0){
    Serial.printf("Error generando RSA (ret=-0x%04X)\n", -ret);
    return;
  }
  Serial.printf("Par RSA-2048 generado en %lu ms\n",
                (unsigned long)(millis() - t0));

  imprimirPublica();
  Serial.println("Receptor listo");
}

void loop()
{
}
