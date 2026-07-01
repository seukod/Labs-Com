# Experimentación paso a paso — Grupo 6 · RSA-2048 + AES-128 (híbrido)

Guion para las slides de experimentación. Cada bloque = 1 slide sugerida:
**Objetivo → Procedimiento → Código clave → Resultado observado (Serial)**.

---

## Slide 0 · Montaje del experimento

**Objetivo:** tener el banco de pruebas listo.

**Procedimiento:**
1. 2 placas **ESP32 Dev Module** conectadas por USB.
2. Arduino IDE 2.3.x, ESP32 Arduino Core **3.x** (trae mbedTLS integrado).
3. Dos ventanas de IDE + dos Monitores Serial a **115200 baud**:
   - Izquierda = **RX** (`sketch_jun26b`) en `/dev/cu.usbserial-0001`
   - Derecha = **TX** (`tx_jun26b`) en `/dev/cu.usbserial-4`

**Resultado:** ambos monitores abiertos, listos para observar el intercambio.

---

## Slide 1 · Tarea 28 — Generar el par RSA-2048 en el ESP32

**Objetivo:** que el RX cree su propio par de claves RSA-2048 y publique la pública.

**Procedimiento:**
1. Flashear el RX. En el arranque llama a `mbedtls_rsa_gen_key(…, 2048, 65537)`.
2. Medimos el tiempo con `millis()` alrededor de la generación.
3. El RX imprime N y E (clave pública) por Serial para copiarlos al TX.

**Código clave (RX, `setup`):**
```cpp
uint32_t t0 = millis();
int ret = mbedtls_rsa_gen_key(&rsa, mbedtls_ctr_drbg_random, &drbg, 2048, 65537);
Serial.printf("Par RSA-2048 generado en %lu ms\n", millis() - t0);
imprimirPublica();   // imprime RX_N y RX_E en hex
```

**Resultado observado (Serial RX):**
```
Generando par RSA-2048... (puede tardar)
Par RSA-2048 generado en ~XXXXX ms      <-- anotar el valor real
===== CLAVE PUBLICA RSA-2048 (copiar al TX) =====
RX_N = "CDA7ADBA9A5129F52B6C752E0E26DCA5AA904664B22C5EFF66222F9EFE6928E5...";
RX_E = "010001";
```

> **Aprendizaje:** la generación es cara (~10–30 s) y ocurre **una sola vez** al
> arrancar. Por eso RSA no sirve para operaciones frecuentes.

---

## Slide 2 · Tarea 29 — Cifrar con la clave pública del receptor

**Objetivo:** demostrar que el TX cifra un payload corto (<200 B) con la
**pública del RX**, sin conocer la privada.

**Procedimiento:**
1. Copiar el `RX_N` que imprimió el RX y pegarlo en `tx_jun26b.ino` (`RX_N`).
2. El TX importa esa pública (`mbedtls_rsa_import`) y la valida.
3. Genera una **clave AES-128 aleatoria** (16 B, `esp_random()`) — ese es el
   "mensaje corto" — y la cifra con RSA → 256 B.

**Código clave (TX):**
```cpp
uint8_t claveAES[16];
rellenarAleatorio(claveAES, 16);                 // mensaje corto a cifrar
mbedtls_rsa_pkcs1_encrypt(&rsaPub, mbedtls_ctr_drbg_random, &drbg,
                          16, claveAES, claveCif); // -> 256 B cifrados
```

**Resultado observado (Serial TX):**
```
Transmisor listo
[TIEMPO] RSA-2048 cifrar clave AES (16 B): 12 ms
[TX] Clave AES (RSA) + mensaje (AES) enviados
```

> **Detalle:** los 256 B no caben en una trama ESP-NOW (máx. 250 B), así que se
> parten en **2 fragmentos de 128 B** y el RX los reensambla.

---

## Slide 3 · Tarea 30 — Medir tiempos (generación + cifrado/descifrado)

**Objetivo:** comparar cuantitativamente el costo de RSA vs AES en el ESP32.

**Procedimiento:** envolver cada operación con `millis()` (ms) o
`micros()` (µs) e imprimir. Dejar correr varios ciclos y anotar.

**Resultado observado (tabla medida):**

| Operación | Tiempo | Placa |
|---|---|---|
| Generación par RSA-2048 | ~10–30 s (1 vez) | RX |
| RSA-2048 cifrar clave AES (16 B) | **12 ms** | TX |
| RSA-2048 descifrar clave AES | **~227 ms** | RX |
| AES-128-CBC cifrar 32 B | **16 µs** | TX |
| AES-128-CBC descifrar 32 B | **~31 µs** | RX |

**Conclusión de la medición:**
Descifrar con RSA (~227 ms) es **≈ 7.300× más lento** que con AES (~31 µs).
→ RSA solo debe cifrar la clave (16 B), no el mensaje. Justifica el híbrido.

---

## Slide 4 · Tarea 31 — Esquema híbrido RSA + AES funcionando

**Objetivo:** demostrar el sistema completo: RSA cifra la clave AES, AES cifra
el mensaje real, y el RX recupera el texto.

**Procedimiento (cada ciclo, cada 2 s):**
1. TX genera clave AES-128 aleatoria nueva.
2. TX la cifra con RSA y la envía (2 fragmentos).
3. TX cifra el mensaje `"Hola mundo seguro"` con **AES-128-CBC** (IV aleatorio).
4. RX reensambla y **descifra la clave con RSA** (privada).
5. RX **descifra el mensaje con AES** y quita el padding PKCS7.

**Código clave (RX):**
```cpp
// 1) recuperar la clave AES con RSA (privada)
mbedtls_rsa_pkcs1_decrypt(&rsa, …, &olen, bufClaveCif, salida, 256);
// 2) descifrar el mensaje con AES
mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, pkt->payload, claro);
```

**Resultado observado (Serial RX):**
```
[OK] Clave AES recuperada via RSA
[TIEMPO] RSA-2048 descifrar clave AES: 227 ms
[TIEMPO] AES-128-CBC descifrar 32 B: 31 us
[RX] Mensaje: Hola mundo seguro
```

**✅ Demo completa:** el mensaje viaja cifrado (clave por RSA, texto por AES) y el
receptor lo reconstruye correctamente.

---

## Slide 5 · Experimentación extra — Depuración del known-error (opcional pero potente)

Muestra que **experimentaron** de verdad, no solo que copiaron código.

**Qué pasó:** al reiniciar el RX, el descifrado empezó a fallar:
```
Error descifrando clave AES (ret=-0x4100, len=245)
Llego MSG_DATOS pero aun no hay clave AES; descartado
```

**Diagnóstico:** `-0x4100 = MBEDTLS_ERR_RSA_INVALID_PADDING`. El RX **regenera su
par RSA en cada arranque**, así que el `RX_N` pegado en el TX era de un boot
anterior → clave pública y privada **no coincidían** → padding inválido.

**Solución aplicada:**
1. Resetear el RX y copiar el **nuevo** `RX_N`.
2. Pegarlo en el TX y reflashear **solo el TX**.
3. No volver a resetear el RX.

**Resultado:** vuelve a aparecer `[OK] Clave AES recuperada via RSA` y
`[RX] Mensaje: Hola mundo seguro`.

> **Lección de seguridad:** la clave pública debe corresponder exactamente a la
> privada; un desajuste rompe todo el canal. (En sistemas reales esto se resuelve
> con certificados/PKI, no copiando N a mano.)

---

## Orden recomendado de la demo en vivo

1. Flashear y arrancar **RX** → mostrar generación de par + tiempo + `RX_N`.
2. Copiar `RX_N` al **TX** → flashear TX.
3. Mostrar en paralelo: TX cifrando (RSA + AES) y RX descifrando → `Hola mundo seguro`.
4. Señalar los tiempos en pantalla (12 ms vs 16 µs, 227 ms vs 31 µs).
