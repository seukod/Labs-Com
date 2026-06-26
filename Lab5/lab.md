# Comunicación Local entre ESP32 utilizando ESP-NOW

```
Laboratorio de Comunicaciones
```
# 1. Objetivos

```
Al finalizar esta experiencia el estudiante será capaz de:
Configurar módulos ESP32 para comunicación inalámbrica.
Obtener y utilizar la dirección MAC de un dispositivo.
Implementar una comunicación punto a punto mediante ESP-NOW.
Transmitir estructuras de datos entre dos nodos.
Configurar un nodo como emisor y receptor simultáneamente.
Analizar el comportamiento de una red inalámbrica simple.
```
# 2. Marco Teórico

## 2.1. ESP-NOW

ESP-NOW es un protocolo de comunicación inalámbrica desarrollado por Espressif Systems
para sus microcontroladores ESP8266 y ESP32. Permite el intercambio de datos entre dispositivos
de forma directa, sin necesidad de una infraestructura Wi-Fi convencional como un router o punto
de acceso.

2.1.1. Arquitectura del protocolo
ESP-NOW opera en la capa de enlace de datos del modelo OSI, por debajo del stack TCP/IP.
Utiliza el estándar IEEE 802.11 en modo de acción de proveedor (vendor action frame), lo que
le permite enviar tramas cortas directamente entre dispositivos identificados por su dirección
MAC, con latencias típicas inferiores a 10 ms.
La arquitectura distingue dos roles principales:
Iniciador (Controller): dispositivo que envía datos. Debe registrar previamente la direc-
ción MAC del receptor como par (peer).
Receptor (Responder): dispositivo que recibe datos. Registra una función de callback
que se invoca cada vez que llega un paquete.
Un mismo dispositivo puede actuar simultáneamente como iniciador y receptor, habilitando
topologías bidireccionales sin modificaciones en el hardware.

2.1.2. Topologías soportadas
ESP-NOW soporta de forma nativa tres topologías principales:
Punto a punto (unicast): un dispositivo transmite a un único receptor registrado.
Uno a muchos (broadcast/multicast): un iniciador envía el mismo paquete a múltiples
pares, útil para difusión de estado o comandos globales.
Muchos a uno: múltiples nodos transmiten hacia un único concentrador (gateway), patrón
común en redes de sensores.


2.1.3. Características principales
Sin infraestructura: la comunicación es directa entre nodos, sin router ni AP.
Baja latencia: al operar fuera del stack TCP/IP, la sobrecarga es mínima.
Carga útil limitada: cada trama transporta hasta 250 bytes de datos de aplicación.
Seguridad opcional: soporta cifrado CCMP (AES-128) por par, con un máximo de 6
pares cifrados simultáneos en ESP32.
Coexistencia con Wi-Fi: el ESP32 puede mantener una conexión Wi-Fi activa mientras
usa ESP-NOW, siempre que ambos operen en el mismo canal.
Alcance: en condiciones de línea de vista, el alcance típico es de 200 a 480 m.

2.1.4. Flujo de operación
El ciclo de vida de una transmisión ESP-NOW sigue los pasos descritos a continuación:

1. Inicialización: se configura el modo Wi-Fi (WIFI_STA) y se llama a esp_now_init().
2. Registro de pares: el iniciador agrega la dirección MAC del receptor mediante esp_now_add_peer().
3. Registro de callbacks: se registran funciones de retrollamada para envío (esp_now_register_send_cb)
    y recepción (esp_now_register_recv_cb).
4. Transmisión: el iniciador invoca esp_now_send() con la MAC destino y el payload.
5. Confirmación: el callback de envío informa si la trama fue reconocida (ESP_NOW_SEND_SUCCESS)
    o falló (ESP_NOW_SEND_FAIL).
6. Recepción: en el receptor, el callback entrega los datos junto con metadatos como la MAC
    origen y la intensidad de señal (RSSI).

2.1.5. Comparación con otras alternativas
La Tabla 1 compara ESP-NOW con los protocolos inalámbricos más utilizados en sistemas
embebidos de bajo consumo.

```
Cuadro 1: Comparación de protocolos inalámbricos para sistemas embebidos
Protocolo Latencia Alcance Infraestructura Payload máx.
ESP-NOW<10 ms∼200 m No requerida 250 bytes
Wi-Fi 10–100 ms∼100 m Router/AP Sin límite práctico
Bluetooth 10–100 ms∼10 m No requerida 512 bytes
Zigbee∼15 ms∼100 m Coordinador 127 bytes
```
# 3. Descripción de la Actividad

Cada grupo trabajará con dos ESP32:
ESP32-A y ESP32-B, que alternarán los roles de transmisor y receptor.
La comunicación se realizará exclusivamente mediante ESP-NOW. No existe acceso a Internet
ni punto de acceso Wi-Fi disponible.
ESP32-A ---------> ESP32-B (actividades 1 a 6)
ESP32-A <--------> ESP32-B (actividad 7: bidireccional)


# 4. Materiales

```
Por grupo:
2 ESP32 DevKit V
2 cables USB
1 computador con Arduino IDE instalado
```
# 5. Actividad 1: Identificación de Dispositivos

Cargue el siguiente programa en cada uno de los ESP32 para obtener su dirección MAC.
1 #include <WiFi.h>
2
3 void setup()
4 {
5 Serial.begin (115200);
6 WiFi.mode(WIFI_STA);
7 delay (100);
8 Serial.print("MAC:␣");
9 Serial.println(WiFi.macAddress ());
10 }
11
12 void loop()
13 {
14 }
Abra el Monitor Serie (115200 baud) y registre la dirección MAC de cada dispositivo.

## Entregable

```
Dispositivo MAC Address
ESP32-A Transmisor: 8C:94:DF:4C:B6:3C
ESP32-B Receptor: 8C:94:DF:61:3A:A8
```
# 6. Actividad 2: Estructura de Datos

```
Se utilizará la siguiente estructura para transmitir información entre los nodos:
1 typedef struct
2 {
3 uint8_t grupo;
4 uint32_t secuencia;
5 } Paquete;
```
## Pregunta

```
¿Qué ventaja tiene transmitir una estructura en lugar de un mensaje de texto plano?
```
La ventaja es que funciona como un "bloque de lego" osea es plug and play para el receptor, como es una estructura, ya sabe como indexar sus datos.
# 7. Actividad 3: Programación del Receptor

```
Configure el ESP32-B como receptor. El programa debe:
```
1. Inicializar ESP-NOW en modo WIFI_STA.
2. Registrar el callback de recepción.
3. Mostrar por el Monitor Serie el número de grupo y el número de secuencia de cada paquete
    recibido.
Salida esperada en el Monitor Serie del receptor:
Grupo 4 Seq 1
Grupo 4 Seq 2
Grupo 4 Seq 3

## Entregable

```
Demostrar al profesor la recepción correcta de datos.
```
# 8. Actividad 4: Programación del Transmisor

Configure el ESP32-A como transmisor. El programa debe enviar cada 5 segundos una
estructura Paquete con:
El identificador del grupo.
Un número de secuencia que incremente con cada envío.
Salida esperada en el Monitor Serie del transmisor:
Enviando: Grupo 4 Seq 1
Enviando: Grupo 4 Seq 2
Enviando: Grupo 4 Seq 3

## Entregable

```
Demostración funcional simultánea:
El transmisor envía con secuencia creciente.
El receptor muestra los datos correctamente.
```
# 9. Actividad 5: Contador de Paquetes

Verifique que el número de secuencia incremente automáticamente en cada transmisión. Com-
plete la siguiente tabla registrando si cada paquete fue recibido:
Paquete enviado Recibido (S/N)
1
2
3
4
5


# 10. Actividad 6: Generación Automática de Tráfico

Modifique el transmisor para que los mensajes sean enviados en intervalos aleatorios utilizan-
do:
1 delay(random (1000, 5000));

```
El tiempo entre transmisiones variará entre 1 y 5 segundos.
```
## Entregable

```
Capture una ejecución mostrando al menos cuatro mensajes con intervalos variables, indi-
cando el tiempo transcurrido entre cada uno.
```
# 11. Actividad 7: Comunicación Bidireccional

```
En esta actividad ambos ESP32 actuarán al mismo tiempo como emisor y receptor.
Cada dispositivo debe:
```
1. Registrar la MAC del otro dispositivo como par.
2. Enviar periódicamente su propio Paquete (con su número de grupo).
3. Recibir y mostrar los paquetes del otro dispositivo.
    Para lograr esto, cada nodo debe registrar ambos callbacks (esp_now_register_send_cb
y esp_now_register_recv_cb) y agregar el peer antes de iniciar el loop de transmisión.
Salida esperada en el Monitor Serie del ESP32-A (grupo 1):
[TX] Enviando: Grupo 1 Seq 1
[RX] Recibido de Grupo 2 Seq 1
[TX] Enviando: Grupo 1 Seq 2
[RX] Recibido de Grupo 2 Seq 2
Salida esperada en el Monitor Serie del ESP32-B (grupo 2):
[RX] Recibido de Grupo 1 Seq 1
[TX] Enviando: Grupo 2 Seq 1
[RX] Recibido de Grupo 1 Seq 2
[TX] Enviando: Grupo 2 Seq 2

## Consideraciones de implementación

```
El callback de recepción se ejecuta en el contexto de la tarea Wi-Fi; no realice operaciones
bloqueantes dentro de él.
Utilice una variable de tipo volatile o una bandera si necesita comunicar datos entre el
callback y el loop().
Asegúrese de que ambos dispositivos estén programados antes de conectarlos al Monitor
Serie para observar la sincronía.
```

## Entregable

1. Demostrar ambos nodos transmitiendo y recibiendo simultáneamente.
2. Responda: ¿qué ocurre si el intervalo de transmisión de ambos nodos es idéntico? ¿Se
    producen colisiones? ¿Por qué?

# 12. Preguntas de Análisis

```
Responda las siguientes preguntas:
```
1. ¿Qué es ESP-NOW y en qué capa del modelo OSI opera?
2. ¿Qué función cumple la dirección MAC en ESP-NOW?
3. ¿Qué ocurre si se configura una MAC incorrecta en el peer?
4. ¿Cuál es la ventaja de utilizar estructuras en lugar de cadenas de texto?
5. ¿Qué diferencias existen entre una comunicación cableada y una inalámbrica en términos
    de fiabilidad y latencia?
6. ¿Por qué es importante llamar a WiFi.mode(WIFI_STA) antes de inicializar ESP-NOW?

# 13. Conclusiones

Describa brevemente los resultados obtenidos y las dificultades encontradas durante la prác-
tica.

