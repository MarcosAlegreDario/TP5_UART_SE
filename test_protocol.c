// Hola
#include <stdio.h>
#include <string.h>
#include <stdint.h>
// Para traer los valores de las constantes para las tramas incluimos el archivo de la configuración con sus constantes
#include "firmware/config/app_config.h"
#include "firmware/config/FreeRTOSConfig.h"

#define PROTOCOL_MAX_BODY_SIZE 52   //Solo para este testeo


// Prototipos de funciones



static int hex_char_to_nibble(char c);
uint8_t protocol_checksum(const char *input, size_t len);
int protocol_encode(const char *type, const char *payload, char *buf, size_t buf_size);
int protocol_validate(const char *frame, size_t frame_len);

// Programa principal de prueba

int main()
{
    char buf[64]; // Como tiene 64 elementos su sizeof da 64

    printf("\n--- TEST ETAPA 1: ENCODE & CHECKSUM ---\n\n");

    protocol_encode("CMD", "ping", buf, sizeof(buf));
    printf("Esperado: @08:CMD:ping:52\n");
    printf("Obtenido: %s\n", buf);

    protocol_encode("CMD", "led=on", buf, sizeof(buf));
    printf("Esperado: @0A:CMD:led=on:6A\n");
    printf("Obtenido: %s\n", buf);

    protocol_encode("CMD", "led=toggle", buf, sizeof(buf));
    printf("Esperado: @0E:CMD:led=toggle:7D\n");
    printf("Obtenido: %s\n", buf);

    protocol_encode("ERR", "code=unknown_cmd", buf, sizeof(buf));
    printf("Esperado: @14:ERR:code=unknown_cmd:2D\n");
    printf("Obtenido: %s\n", buf);

    printf("Primera parte del testeo cumplida, pasamos a la siguiente\n");
    printf("Implementamos el protocol_validate()\n");

    printf("Le pasamos 08:CMD:ping:52 y vemos si devuelve 1 o 0\n");
    int validacion = protocol_validate("08:CMD:ping:52", 14);
    printf("Resultado: %d\n", validacion);

    printf("Le pasamos 08:CMD:ping:53 y vemos si devuelve 1 o 0\n");
    validacion = protocol_validate("08:CMD:ping:53", 16);
    printf("Resultado: %d\n", validacion);

    printf("Le pasamos 0A:ACK:cmd=ok:6B y vemos si devuelve 1 o 0\n");
    validacion = protocol_validate("0A:ACK:cmd=ok:6B", 16);
    printf("Resultado: %d\n", validacion);

    
    printf("Le pasamos 14:ERR:code=unknown_cmd:2D y vemos si devuelve 1 o 0\n");
    validacion = protocol_validate("14:ERR:code=unknown_cmd:2D", 26);
    printf("Resultado: %d\n", validacion);
    

               return 0;
}

// Conversión Hexadecimal
static int hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Checksum XOR byte a byte

uint8_t protocol_checksum(const char *input, size_t len)
{
    uint8_t cs = 0U;
    for (size_t i = 0U; i < len; i++)
    {
        cs ^= (uint8_t)input[i];
    }
    return cs;
}

// Armado de la Trama
int protocol_encode(const char *type, const char *payload, char *buf, size_t buf_size)
{
    size_t type_len = strlen(type);
    size_t payload_len = strlen(payload);

    // Validaciones básicas
    if (type_len != 3)
        return -1;
    if (payload_len > PROTOCOL_MAX_PAYLOAD_LENGTH)
        return -1;

    for (size_t i = 0; i < payload_len; i++)
    {
        if (payload[i] < 0x20 || payload[i] > 0x7E || payload[i] == '@')
            return -1;
    }

    // Cuerpo: TTT:PAYLOAD
    char body[PROTOCOL_MAX_BODY_SIZE + 1];
    snprintf(body, sizeof(body), "%s:%s", type, payload);
    size_t body_len = strlen(body);

    // LL:TTT:PAYLOAD (Input para el checksum)
    char cs_input[PROTOCOL_MAX_FRAME_LENGTH + 1];
    int cs_bytes = snprintf(cs_input, sizeof(cs_input), "%02X:%s", (unsigned int)body_len, body);
    if (cs_bytes < 0 || (size_t)cs_bytes >= sizeof(cs_input))
        return -1;

    // Calcular el Checksum
    uint8_t cs = protocol_checksum(cs_input, (size_t)cs_bytes);

    // Trama completa
    int written = snprintf(buf, buf_size, "@%s:%02X\n", cs_input, cs);
    if (written < 0 || (size_t)written >= buf_size)
        return -1;

    return written;
}

// Validación de trama recibida
static int protocol_validate(const char *frame, size_t frame_len)
{
    const char *last_colon = NULL;
    for (size_t i = 0; i < frame_len; i++)
    {
        if (frame[i] == ':')
            last_colon = &frame[i];
    }

    if (last_colon == NULL)
        return -1;
    if (last_colon + 3 != frame + frame_len)
        return -1;

    const char *cs_received_str = last_colon + 1;

    int hi = hex_char_to_nibble(cs_received_str[0]);
    int lo = hex_char_to_nibble(cs_received_str[1]);
    if (hi < 0 || lo < 0)
        return -1;

    uint8_t cs_received = (uint8_t)((hi << 4) | lo);

    size_t cs_input_len = (size_t)(last_colon - frame);
    // Usamos la función de checksum que declaramos en protocol.h
    uint8_t cs_calculated = protocol_compute_checksum(frame, cs_input_len);
    return (cs_calculated == cs_received) ? 0 : 1;
}