#include <RH_ASK.h>
#include <SPI.h>

RH_ASK rf_driver(2000, 2, -1);

#define MI_ID     0x06
#define ID_TX     0x01
#define BROADCAST 0x00
#define CABECERA  0xAA 
#define TOTAL_PAQUETES 43

#define LED_RX 13

// ESTRUCTURA EXACTA A LA IMAGEN DEL PDF (7 BYTES)
struct __attribute__((packed)) Paquete
{
    uint8_t cabecera;
    uint8_t id_tx;
    uint8_t id_rx;
    uint8_t datos[3];
    uint8_t checksum;
};

uint8_t imagen[128];
int bytes_recibidos = 0; // Lleva la cuenta secuencial de lo que llega
int paquetes_recibidos = 0;

const char* nombreID(uint8_t id)
{
    switch(id)
    {
        case BROADCAST: return "BROADCAST (0x00)";
        case ID_TX:     return "TX_PRINCIPAL (0x01)";
        case MI_ID:     return "RX_SENSOR (0x04)";
        default:        return "DESCONOCIDO";
    }
}

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

void imprimirImagen()
{
    Serial.println();
    Serial.println(F("[IMG] Imagen reconstruida (128 bytes):"));
    Serial.println();

    for(int y = 0; y < 32; y++)
    {
        for(int x = 0; x < 32; x++)
        {
            int indice_bit = y * 32 + x;
            int byte_index = indice_bit / 8;
            int bit_index  = 7 - (indice_bit % 8);
            bool pixel = (imagen[byte_index] >> bit_index) & 0x01;
            if(pixel) Serial.print(F("░"));
            else      Serial.print(F("▓"));
        }
        Serial.println();
    }
}

void setup()
{
    Serial.begin(9600);
    pinMode(LED_RX, OUTPUT);

    Serial.print(F("[STRUCT] sizeof(Paquete) = "));
    Serial.println(sizeof(Paquete)); // COMPROBACIÓN: Debe imprimir 7

    if (!rf_driver.init()) Serial.println(F("[ERROR] Fallo al iniciar receptor"));
    else                   Serial.println(F("[OK] Receptor listo"));
}

void loop()
{
    uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
    uint8_t buflen = sizeof(buf);

    if (rf_driver.recv(buf, &buflen))
    {
        // Limpiamos la matriz si estamos recibiendo el primer paquete de una nueva imagen
        if(bytes_recibidos == 0) {
           Serial.println(F("\n--- INICIANDO RECEPCIÓN DE NUEVA IMAGEN ---"));
           paquetes_recibidos = 0;
        }

        Paquete p;
        memcpy(&p, buf, sizeof(Paquete));

        // 1. Validar Cabecera
        if (p.cabecera != CABECERA) return; 

        // 2. Validar Destinatario
        if(p.id_rx != MI_ID && p.id_rx != BROADCAST) return;

        // 3. Validar Checksum
        uint8_t checksum_calculado = calcularChecksum(p);
        if(checksum_calculado != p.checksum)
        {
            Serial.println(F("[ALERTA] Checksum inválido. Paquete descartado."));
            return;
        }

        // Feedback visual
        digitalWrite(LED_RX, HIGH);
        delay(10);
        digitalWrite(LED_RX, LOW);

        // Guardar los 3 bytes en orden secuencial
        for(int i = 0; i < 3; i++)
        {
            if(bytes_recibidos < 128) 
            {
                imagen[bytes_recibidos] = p.datos[i];
                bytes_recibidos++;
            }
        }
        paquetes_recibidos++;

        Serial.print(F("Recibiendo fragmento "));
        Serial.print(paquetes_recibidos);
        Serial.print(F("/"));
        Serial.println(TOTAL_PAQUETES);

        // Si ya completamos los 128 bytes, imprimimos la imagen
        if(bytes_recibidos >= 128)
        {
            imprimirImagen();
            
            // Calculamos el PDR basándonos en si llegaron todos los fragmentos
            float pdr = ((float)paquetes_recibidos / TOTAL_PAQUETES) * 100.0;
            Serial.print(F("PDR de esta imagen: "));
            Serial.print(pdr);
            Serial.println(F("%"));

            // Reiniciamos el contador para la próxima imagen
            bytes_recibidos = 0; 
        }
    }
}