//Librerias necesarias durante todo el proyecto
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "string.h"
#include "freertos/freeRTOS.h"
#include "freetos/task.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "esp_wife.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"

// Pines
#define PIN_IN1 GPIO_NUM_19
#define PIN_IN2 GPIO_NUM_18
#define PIN_IN3 GPIO_NUM_5
#define PIN_IN4 GPIO_NUM_17
// Velocidad
#define DELAY_PASOS 1850
// Dirección inversa
int direccion = -1;
// Paso actual
int paso_secuencia = 0;

// Secuencia del motor
const uint8_t secuencia[4] = 
{
    0x09,
    0x0C,
    0x06,
    0x03
};

// Actualiza bobinas
void IRAM_ATTR  actualizar_bobinas(uint8_t paso)
{
    gpio_set_level(PIN_IN1, (paso >> 0) & 0x01);
    gpio_set_level(PIN_IN2, (paso >> 1) & 0x01);
    gpio_set_level(PIN_IN3, (paso >> 2) & 0x01);
    gpio_set_level(PIN_IN4, (paso >> 3) & 0x01);
}

void IRAM_ATTR motor_timer_callback(void *arg)
{
actulizar_bobinas(secuencia[paso_secuencia]);
paso_secuencial += direccion;
    if(paso_secuencia > 3)
    {
    paso_secuencia = 0;
    }
    if(paso_secuencia < 0 )
    {
    paso_secuencia = 3;
    }
}


void app_main(void)
{
    // Configuración de salidas
    gpio_config_t motor_conf = 
    {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << PIN_IN1) |
            (1ULL << PIN_IN2) |
            (1ULL << PIN_IN3) |
            (1ULL << PIN_IN4)
    };

    gpio_config(&motor_conf);


    }
}
