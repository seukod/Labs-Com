// ============================================================
//  TX - TRANSMISOR  (cifrado hibrido RSA-2048 + AES-128 / ESP-NOW)
// ------------------------------------------------------------
//  - Tiene la PUBLICA del RX pre-cargada (copiada del Serial del RX).
//  - Cada ciclo:
//      1) genera una clave AES-128 aleatoria con esp_random()
//      2) la cifra con RSA-2048 (256 B) y la envia en 2 fragmentos
//      3) cifra el mensaje con AES-128-CBC (IV aleatorio) y lo envia
// ============================================================
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>
#include <esp_system.h>          // esp_random()

#include "mbedtls/rsa.h"
#include "mbedtls/aes.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"
#include "mbedtls/version.h"

//----------------------------------------------------
// >>> PEGAR AQUI la PUBLICA que imprime el RX por Serial <<<
const char* RX_N = "PEGAR_MODULO_N_EN_HEX";
const char* RX_E = "10001";      // 65537

//----------------------------------------------------
enum TipoMsg : uint8_t { MSG_CLAVE_AES = 1, MSG_DATOS = 2 };

typedef struct __attribute__((packed)) {
  uint8_t  tipo;
  uint8_t  frag;
  uint8_t  totalFrag;
  uint8_t  iv[16];
  uint16_t longitud;
  uint8_t  payload[200];
} Paquete;

//----------------------------------------------------
mbedtls_rsa_context      rsaPub;     // solo la publica del RX
mbedtls_entropy_context  entropia;
mbedtls_ctr_drbg_context drbg;

uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

const char *MENSAJE = "Hola mundo seguro";

//----------------------------------------------------
void rellenarAleatorio(uint8_t *buf, size_t n)
{
  for(size_t i = 0; i < n; i += 4){
    uint32_t r = esp_random();
    for(size_t j = 0; j < 4 && (i + j) < n; j++)
      buf[i + j] = (r >> (8 * j)) & 0xFF;
  }
}

//----------------------------------------------------
bool enviarClaveAES(const uint8_t *claveAES)
{
  uint8_t claveCif[256];
  int ret =
#if MBEDTLS_VERSION_MAJOR >= 3
    mbedtls_rsa_pkcs1_encrypt(&rsaPub, mbedtls_ctr_drbg_random, &drbg,
                              16, claveAES, claveCif);
#else
    mbedtls_rsa_pkcs1_encrypt(&rsaPub, mbedtls_ctr_drbg_random, &drbg,
                              MBEDTLS_RSA_PUBLIC, 16, claveAES, claveCif);
#endif
  if(ret != 0){
    Serial.printf("Error cifrando clave AES (ret=-0x%04X)\n", -ret);
    return false;
  }

  // 256 B no caben en una trama -> 2 fragmentos de 128 B
  for(uint8_t f = 0; f < 2; f++){
    Paquete pkt = {};
    pkt.tipo      = MSG_CLAVE_AES;
    pkt.frag      = f;
    pkt.totalFrag = 2;
    pkt.longitud  = 128;
    memcpy(pkt.payload, &claveCif[f * 128], 128);
    if(esp_now_send(broadcast, (uint8_t*)&pkt, sizeof(pkt)) != ESP_OK){
      Serial.println("Fallo envio fragmento clave");
      return false;
    }
    delay(20);
  }
  return true;
}

//----------------------------------------------------
bool enviarMensaje(const uint8_t *claveAES, const char *texto)
{
  size_t lon = strlen(texto);
  // Padding PKCS7 a multiplo de 16
  uint8_t pad = 16 - (lon % 16);          // si lon%16==0 -> pad=16
  size_t total = lon + pad;
  if(total > 200){
    Serial.println("Mensaje demasiado largo");
    return false;
  }

  uint8_t claro[200];
  memcpy(claro, texto, lon);
  for(size_t i = lon; i < total; i++) claro[i] = pad;

  Paquete pkt = {};
  pkt.tipo     = MSG_DATOS;
  pkt.longitud = total;
  rellenarAleatorio(pkt.iv, 16);

  uint8_t iv[16];
  memcpy(iv, pkt.iv, 16);                  // crypt_cbc modifica el IV

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, claveAES, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, total,
                        iv, claro, pkt.payload);
  mbedtls_aes_free(&aes);

  if(esp_now_send(broadcast, (uint8_t*)&pkt, sizeof(pkt)) != ESP_OK){
    Serial.println("Fallo envio mensaje");
    return false;
  }
  return true;
}

//----------------------------------------------------
void setup()
{
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_STA);

  if(esp_now_init() != ESP_OK){
    Serial.println("Error ESP-NOW");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcast, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if(esp_now_add_peer(&peer) != ESP_OK){
    Serial.println("Error agregando peer broadcast");
    return;
  }

  // RNG
  mbedtls_entropy_init(&entropia);
  mbedtls_ctr_drbg_init(&drbg);
  const char *pers = "tx_esp_now_rsa";
  if(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropia,
                           (const uint8_t*)pers, strlen(pers)) != 0){
    Serial.println("Error sembrando DRBG");
    return;
  }

  // Importar la publica del RX
  mbedtls_rsa_init(&rsaPub);
  mbedtls_rsa_set_padding(&rsaPub, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);

  mbedtls_mpi N, E;
  mbedtls_mpi_init(&N);
  mbedtls_mpi_init(&E);
  if(mbedtls_mpi_read_string(&N, 16, RX_N) != 0 ||
     mbedtls_mpi_read_string(&E, 16, RX_E) != 0){
    Serial.println("Error leyendo RX_N / RX_E (revisa que esten pegados)");
    return;
  }
  if(mbedtls_rsa_import(&rsaPub, &N, NULL, NULL, NULL, &E) != 0 ||
     mbedtls_rsa_complete(&rsaPub) != 0 ||
     mbedtls_rsa_check_pubkey(&rsaPub) != 0){
    Serial.println("Clave publica invalida");
    return;
  }
  mbedtls_mpi_free(&N);
  mbedtls_mpi_free(&E);

  Serial.println("Transmisor listo");
}

//----------------------------------------------------
void loop()
{
  // 1) Clave AES-128 aleatoria nueva en cada ciclo
  uint8_t claveAES[16];
  rellenarAleatorio(claveAES, 16);

  // 2) Enviar la clave AES cifrada con RSA (2 fragmentos)
  if(enviarClaveAES(claveAES)){
    delay(50);
    // 3) Enviar el mensaje cifrado con AES-128-CBC
    if(enviarMensaje(claveAES, MENSAJE))
      Serial.println("[TX] Clave AES (RSA) + mensaje (AES) enviados");
  }

  delay(2000);
}
