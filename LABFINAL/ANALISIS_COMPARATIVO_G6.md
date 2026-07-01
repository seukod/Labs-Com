# Análisis comparativo — Grupo 6 · RSA-2048 + AES-128 (híbrido) vs XOR

Laboratorio base: cifrado **XOR** con clave compartida de 16 bytes.
Mecanismo del Grupo 6: **cifrado híbrido** = RSA-2048 cifra una clave AES-128
aleatoria + AES-128-CBC cifra el mensaje.

Todos los tiempos son **medidos en el ESP32** (ESP32 Dev Module, mbedTLS del core 3.x).

---

## 0. Tabla de tiempos medidos (ESP32)

| Operación                          | Tiempo medido | Dónde |
|------------------------------------|---------------|-------|
| Generación del par RSA-2048        | ~10–30 s (1 sola vez) | RX (arranque) |
| RSA-2048 **cifrar** clave AES (16 B) | **12 ms**   | TX |
| RSA-2048 **descifrar** clave AES     | **~227 ms** | RX |
| AES-128-CBC **cifrar** 32 B          | **16 µs**   | TX |
| AES-128-CBC **descifrar** 32 B       | **~31 µs**  | RX |

**Dato clave:** descifrar con RSA (~227 ms) es **≈ 7.300× más lento** que
descifrar con AES (~31 µs). Por eso RSA se usa **solo para la clave** (16 B) y
AES para el mensaje. Ese número justifica todo el diseño híbrido.

> Si el mensaje real (p. ej. 32 B) se cifrara **directamente con RSA**, cada
> mensaje costaría ~227 ms de descifrado en vez de ~31 µs → el sistema pasaría
> de manejar miles de mensajes/seg a solo ~4 mensajes/seg.

---

## 1. Eje A — Known-plaintext attack (ataque con texto conocido)

**Pregunta:** si el atacante conoce el mensaje y su versión cifrada, ¿puede
romper el cifrado?

### XOR (base) — VULNERABLE (total)

El XOR es lineal: `C = M ⊕ K`. Si el atacante conoce `M` y captura `C`:

```
K = M ⊕ C          ← recupera la clave con UNA sola operación XOR
```

**Ejemplo numérico (1 byte):**
- Clave real: `K = 0x37`
- Mensaje conocido: `M = 'H' = 0x48`  →  `C = 0x48 ⊕ 0x37 = 0x7F`
- Ataque: `K = M ⊕ C = 0x48 ⊕ 0x7F = 0x37`  ✅ clave recuperada

Como la clave del lab tiene solo 16 bytes y se **repite** cíclicamente, basta
conocer **16 bytes** de texto plano para recuperar la clave **completa** y
descifrar todo lo demás. Tiempo del ataque: **microsegundos**.

### RSA + AES (híbrido) — NO vulnerable

- **AES no es lineal.** Conocer pares (M, C) no revela la clave: recuperarla
  exige romper AES por fuerza bruta (2¹²⁸, ver Eje C). No existe un "M ⊕ C = K".
- **Clave AES nueva por mensaje** (generada con `esp_random()`) + **IV aleatorio
  por mensaje** → el mismo texto plano produce un cifrado distinto cada vez, así
  que no hay patrón que explotar.
- **RSA:** la clave pública es pública **por diseño**; conocer la clave AES y su
  cifrado RSA no da la clave privada (eso equivale a factorizar N de 2048 bits).

**Veredicto:** XOR se rompe en microsegundos; el híbrido resiste porque AES es
no lineal y las claves/IV cambian en cada mensaje.

---

## 2. Eje B — Reutilización de clave

**Pregunta:** ¿qué pasa si se usa la misma clave dos veces?

### XOR (base) — VULNERABLE (fuga directa)

El XOR del lab usa **siempre la misma clave fija** de 16 bytes → es un
"many-time pad": la clave se reutiliza en *todos* los mensajes. Con dos cifrados
capturados:

```
C1 = M1 ⊕ K
C2 = M2 ⊕ K
C1 ⊕ C2 = (M1 ⊕ K) ⊕ (M2 ⊕ K) = M1 ⊕ M2     ← la clave DESAPARECE
```

**Ejemplo numérico (1 byte, misma clave K = 0x37):**
- `M1 = 'H' = 0x48` → `C1 = 0x7F`
- `M2 = 'i' = 0x69` → `C2 = 0x5E`
- `C1 ⊕ C2 = 0x7F ⊕ 0x5E = 0x21`  =  `M1 ⊕ M2 = 0x48 ⊕ 0x69 = 0x21` ✅

El resultado `M1 ⊕ M2` filtra la relación entre los mensajes; con análisis de
frecuencia / texto probable se separan ambos plaintexts sin conocer la clave.

### RSA + AES (híbrido) — NO aplica

- Cada mensaje usa una **clave AES nueva** + un **IV aleatorio nuevo** → nunca
  se reutiliza clave. El escenario simplemente no ocurre.
- **Aunque** se reutilizara la clave AES: en CBC con IV distinto, dos mensajes
  iguales cifran distinto, y como AES es no lineal, `C1 ⊕ C2` **no cancela** la
  clave ni revela `M1 ⊕ M2`. El ataque del "two-time pad" no funciona contra AES.

**Veredicto:** en XOR reutilizar la clave filtra información de los mensajes; en
el híbrido no hay reutilización, y aunque la hubiera, AES no colapsa como el XOR.

---

## 3. Eje C — Fuerza bruta (espacio de claves)

**Pregunta:** ¿cuántas claves posibles hay y cuánto tardaría probarlas todas?

| Mecanismo | Espacio de claves | Nº aproximado |
|-----------|-------------------|---------------|
| César (referencia enunciado) | 2⁸ | 256 |
| XOR 1 byte | 2⁸ | 256 |
| XOR 16 bytes (lab) | 2¹²⁸ | 3,4 × 10³⁸ |
| **AES-128 (híbrido)** | 2¹²⁸ | 3,4 × 10³⁸ |
| **RSA-2048 (híbrido)** | factorizar N de 2048 bits (~2¹¹² esfuerzo GNFS) | — |

### Cálculo de tiempo para 2¹²⁸ (AES-128)

`2¹²⁸ = 3,4 × 10³⁸` claves. Suponiendo un atacante **muy** optimista que prueba
**10¹⁸ claves/segundo** (mil millones de GPUs a mil millones de claves/s cada
una):

```
t = 3,4 × 10³⁸ / 10¹⁸ = 3,4 × 10²⁰ s ≈ 1,1 × 10¹³ años
```

≈ **10 billones de años**, unas **~780 veces la edad del universo**
(13,8 × 10⁹ años). Fuerza bruta = **inviable**.

### El matiz importante (XOR vs AES con el MISMO 2¹²⁸)

El XOR de 16 bytes **también** tiene 2¹²⁸ claves, pero **eso no lo hace seguro**:
nadie lo ataca por fuerza bruta porque el known-plaintext (Eje A) lo rompe en
microsegundos. → **Tamaño de clave grande ≠ seguridad** si el algoritmo es
lineal. AES tiene el mismo tamaño de clave pero *sin* atajos lineales.

### RSA-2048

Su seguridad no es el tamaño de clave sino la **dificultad de factorizar** N.
Referencia histórica: **RSA-768** (232 dígitos) fue factorizado en 2009 tras
~2.000 CPU-años. **RSA-2048** está muy por encima de toda capacidad pública
actual. Amenaza futura: el **algoritmo de Shor** en un computador cuántico
suficientemente grande factorizaría N (no afecta a AES de la misma forma → AES
solo pierde "medio" tamaño de clave con Grover: AES-128 → ~2⁶⁴, aún alto).

---

## 4. Tabla resumen (para slide)

| Eje | XOR (base) | RSA-2048 + AES-128 (Grupo 6) |
|-----|------------|------------------------------|
| **Known-plaintext** | ❌ Se rompe en µs (`K = M ⊕ C`) | ✅ Resiste: AES no lineal + clave/IV nuevos por mensaje |
| **Reutilización de clave** | ❌ `C1⊕C2 = M1⊕M2` filtra los textos | ✅ Clave AES + IV nuevos por mensaje; AES no colapsa |
| **Fuerza bruta** | 2¹²⁸ pero **irrelevante** (cae por Eje A) | AES 2¹²⁸ ≈ 10¹³ años + RSA-2048 no factorizable hoy |
| **Confidencialidad real** | Nula en la práctica | Estándar mundial (TLS/HTTPS usan el mismo esquema) |
| **Costo (ESP32)** | ~µs, clave compartida a mano | RSA 12/227 ms + AES 16/31 µs; intercambio de clave automático |

---

## 5. Conclusión (1 frase para cierre)

El XOR es correcto matemáticamente pero **lineal**: cae ante known-plaintext y
reutilización de clave en microsegundos. El esquema híbrido RSA+AES resuelve los
tres ejes: **AES** (no lineal, clave e IV frescos por mensaje) da
confidencialidad robusta y rápida (µs), y **RSA** distribuye la clave AES sin
canal seguro previo, al costo de ~227 ms por ser asimétrico — por eso se cifra
con RSA **solo** la clave (16 B) y no el mensaje. Es exactamente el esquema que
usan HTTPS/TLS y PGP.

---

## 6. Preguntas de análisis — exposición (Grupo 6)

### 6.1 · Si RSA-2048 tarda 800 ms por operación, ¿cuántos mensajes/seg puede manejar?

`1 operación / 0,8 s = 1,25 mensajes por segundo`. Y si cada mensaje exige
cifrar **y** descifrar, baja a `1 / (0,8 + 0,8) ≈ 0,6 mensajes/seg`.

Con **nuestros tiempos medidos** el descifrado real fue ~227 ms →
`1 / 0,227 ≈ 4,4 mensajes/seg`. En cualquier caso es **ridículamente lento**.

Comparación demoledora: AES descifra en ~31 µs →
`1 / 0,000031 ≈ 32.000 mensajes/seg`. Es decir, AES maneja unos **7.000×** más
mensajes por segundo. **Conclusión:** RSA no puede cifrar cada mensaje; por eso
solo cifra la clave AES (una vez) y AES hace el trabajo pesado.

### 6.2 · ¿Por qué Shor (computación cuántica) amenaza a RSA pero no a AES?

- **RSA se basa en un problema matemático con estructura**: factorizar el módulo
  N (equivalente al logaritmo discreto). El **algoritmo de Shor** resuelve la
  factorización en **tiempo polinómico** en un computador cuántico grande →
  recupera la clave privada a partir de la pública → RSA-2048 queda **roto de
  raíz**, no solo debilitado.
- **AES es simétrico y no tiene esa estructura explotable**: su seguridad viene
  de que no hay atajo mejor que probar claves. Lo único que aporta lo cuántico es
  el **algoritmo de Grover**, que da una aceleración solo **cuadrática**: reduce
  2¹²⁸ a un esfuerzo efectivo de 2⁶⁴. Eso **no rompe** AES; equivale a "perder la
  mitad de los bits". Se compensa **duplicando la clave**: AES-256 → 2¹²⁸
  efectivos, sigue seguro.

**Resumen:** cuántica → RSA se cae por completo (Shor), AES solo necesita clave
más larga (Grover). Por eso la criptografía post-cuántica reemplaza RSA/ECC pero
mantiene AES.

### 6.3 · ¿Cómo implementa HTTPS el esquema híbrido RSA + AES?

Es **el mismo patrón que montamos en el ESP32**:

1. El servidor tiene un **certificado** con su clave pública RSA.
2. El cliente genera una clave de sesión simétrica aleatoria (equivalente a
   nuestra clave AES), la **cifra con la pública RSA del servidor** y se la envía.
3. El servidor la **descifra con su privada** → ambos comparten la misma clave
   simétrica sin que viajara en claro.
4. **Todo el tráfico** (páginas, datos) se cifra con esa clave simétrica (AES),
   que es rapidísima.

Es idéntico a nuestro flujo: **RSA protege la clave, AES protege los datos.**

> Matiz para nota máxima: el **TLS moderno (1.3)** prefiere **ECDHE** para
> intercambiar la clave (da *forward secrecy*) y usa RSA sobre todo para
> **autenticar** (firmar el certificado). Pero el principio híbrido —asimétrico
> para establecer la clave, simétrico para el grueso— es exactamente el mismo.

---

## 7. Preguntas de análisis — comunes a todos los grupos (4 a 7)

### 4 · ¿Cuántas claves distintas de 16 bytes existen? Compara con César (255)

16 bytes = 128 bits → `2¹²⁸ = 340.282.366.920.938.463.463.374.607.431.768.211.456`
≈ **3,4 × 10³⁸** claves.

César tiene 255 desplazamientos. La relación es
`3,4 × 10³⁸ / 255 ≈ 1,3 × 10³⁶`: la clave de 16 bytes tiene **más de un billón de
cuatrillones** de veces más combinaciones. (Aun así, tamaño ≠ seguridad si el
algoritmo es lineal — ver Eje C.)

### 5 · Demuestra que (M ⊕ K) ⊕ K = M. ¿Qué propiedad lo hace posible?

Usando las propiedades del XOR:

```
(M ⊕ K) ⊕ K = M ⊕ (K ⊕ K)     [asociatividad]
            = M ⊕ 0            [K ⊕ K = 0, cada valor es su propio inverso]
            = M                [X ⊕ 0 = X, elemento neutro]
```

La propiedad clave es que **XOR es involutivo (su propio inverso)**:
`a ⊕ a = 0`. Por eso cifrar y descifrar son **la misma operación** con la misma
clave. (Se apoya además en la asociatividad y en el neutro 0.)

Comprobación bit a bit (`⊕` de un bit consigo mismo):
`0⊕0=0`, `1⊕1=0` → siempre 0; y `x⊕0=x`. Aplicado bit a bit, se cumple para
bytes completos.

### 6 · Si capturas C1 = M1 ⊕ K y C2 = M2 ⊕ K, ¿qué da C1 ⊕ C2? ¿Qué revela?

```
C1 ⊕ C2 = (M1 ⊕ K) ⊕ (M2 ⊕ K) = M1 ⊕ M2     ← la clave se cancela
```

Da el **XOR de los dos textos planos** (la clave desaparece). Eso **revela la
relación entre ambos mensajes**: con texto probable / crib-dragging (palabras
esperadas como "Hola") se pueden **reconstruir los dos mensajes** sin conocer
nunca la clave. Es la razón por la que **reutilizar la clave es fatal** en XOR.

### 7 · ¿Qué debería cambiar en el XOR para ser tan seguro como AES? (aleatoriedad y longitud)

Dos caminos, según se lea la pregunta:

- **Camino teórico (One-Time Pad):** que la clave sea **verdaderamente aleatoria**,
  **del mismo largo que el mensaje** y usada **una sola vez** (nunca repetida).
  Eso da seguridad perfecta (Shannon), pero es **impráctico**: la clave es tan
  larga como todo lo que quieras enviar y hay que distribuirla en secreto.
- **Camino práctico (lo que hace AES):** como el OTP no escala, AES parte de una
  clave **corta** pero sustituye la **linealidad** del XOR por rondas de
  **sustitución no lineal (S-boxes) + difusión**, de modo que ya **no existe** la
  relación `M ⊕ C = K`. Además usa un **IV/nonce único por mensaje** (modos
  CTR/CBC) para que la misma clave no produzca nunca el mismo cifrado → resuelve
  el problema de reutilización sin alargar la clave.

**En una frase:** al XOR le falta **aleatoriedad efectiva y no repetición** — o
clave aleatoria tan larga como el mensaje (OTP), o una función no lineal fuerte
con IV único por mensaje (AES).

---

## 8. Extra — pregunta del Paso 1 del lab base

**¿Qué información sensible NO deberías enviar en texto plano por ESP-NOW?**
Contraseñas, tokens/credenciales, claves, datos personales o cualquier dato
privado: ESP-NOW no está cifrado por defecto y **cualquier ESP32 cercano en el
mismo canal puede capturar las tramas** (sniffing). Por eso el laboratorio añade
cifrado sobre el mensaje antes de enviarlo.
