#include <RH_ASK.h>
#include <SPI.h>

RH_ASK rf_driver(2000, -1, 2);

#define ID_TX      0x01
#define ID_RX      0x06
#define BROADCAST  0x00
#define CABECERA   0xAA  

// Como enviamos de a 3 bytes, necesitamos 43 paquetes para los 128 bytes (128 / 3 = 42.6)
#define TOTAL_PAQUETES 43 
#define INYECTAR_ERROR false

bool enviar_como_broadcast = true;

// ESTRUCTURA EXACTA A LA IMAGEN DEL PDF (7 BYTES)
struct __attribute__((packed)) Paquete
{
    uint8_t cabecera; // 1 byte (0xAA)
    uint8_t id_tx;    // 1 byte
    uint8_t id_rx;    // 1 byte
    uint8_t datos[3]; // 3 bytes (B0, B1, B2)
    uint8_t checksum; // 1 byte (Suma mod 256)
};                    // Total: 7 bytes

const uint8_t imagen_binaria[32][32] PROGMEM =
{
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t imagen[128];

const char* nombreID(uint8_t id)
{
    switch(id)
    {
        case BROADCAST: return "BROADCAST (0x00)";
        case ID_TX:     return "TX_PRINCIPAL (0x01)";
        case ID_RX:     return "RX_SENSOR (0x06)";
        default:        return "DESCONOCIDO";
    }
}

void empaquetarImagen()
{
    int indice_byte = 0;
    for(int fila = 0; fila < 32; fila++)
    {
        for(int columna = 0; columna < 32; columna += 8)
        {
            uint8_t byte_actual = 0;
            for(int bit = 0; bit < 8; bit++)
            {
                byte_actual <<= 1;
                byte_actual |= pgm_read_byte(&(imagen_binaria[fila][columna + bit]));
            }
            imagen[indice_byte] = byte_actual;
            indice_byte++;
        }
    }
}

// Checksum ajustado a los 3 bytes
uint8_t calcularChecksum(Paquete p)
{
    uint8_t suma = 0;
    suma += p.cabecera; 
    suma += p.id_tx;
    suma += p.id_rx;
    suma += p.datos[0];
    suma += p.datos[1];
    suma += p.datos[2];
    return suma; 
}

void setup()
{
    Serial.begin(9600);
    empaquetarImagen();

    Serial.print(F("[STRUCT] sizeof(Paquete) = "));
    Serial.println(sizeof(Paquete)); // COMPROBACIÓN: Debe imprimir 7

    if (!rf_driver.init()) Serial.println(F("[ERROR] Fallo al iniciar transmisor"));
    else                   Serial.println(F("[OK] Transmisor listo"));
}

void loop()
{
    Serial.println();
    uint8_t destino = enviar_como_broadcast ? BROADCAST : ID_RX;

    Serial.print(F("=== INICIO TRANSMISIÓN (Paquetes de 7 Bytes) ==="));
    Serial.println();

    for(int paquete_actual = 0; paquete_actual < TOTAL_PAQUETES; paquete_actual++)
    {
        Paquete p;
        p.cabecera = CABECERA;
        p.id_tx    = ID_TX;
        p.id_rx    = destino;

        // Cargar los 3 bytes correspondientes
        for(int i = 0; i < 3; i++)
        {
            int indice_imagen = (paquete_actual * 3) + i;
            if(indice_imagen < 128) p.datos[i] = imagen[indice_imagen];
            else                    p.datos[i] = 0x00; // Relleno para el último paquete
        }

        p.checksum = calcularChecksum(p);

        if(INYECTAR_ERROR && paquete_actual == 15)
        {
            p.datos[0] ^= 0xFF;
            Serial.println(F("[TEST] Error inyectado"));
        }

        rf_driver.send((uint8_t *)&p, sizeof(Paquete));
        rf_driver.waitPacketSent();

        Serial.print(F("Enviando fragmento "));
        Serial.print(paquete_actual + 1);
        Serial.print(F("/"));
        Serial.println(TOTAL_PAQUETES);

        delay(100); // Delay reducido para no demorar tanto los 43 paquetes
    }

    Serial.println(F("Imagen completa enviada"));
    enviar_como_broadcast = !enviar_como_broadcast;
    delay(3000);
}