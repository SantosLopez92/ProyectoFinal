#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver_ledc.h"
#include "driver/gpio.h"

/* // Configuracion para ADC
#define ADC_CHANNEL    ADC1_CHANNEL_4   // GPIO32
#define ADC_ATTEN      ADC_ATTEN_DB_11  // Permite lectura hasta ~3.3V
#define ADC_WIDTH      ADC_WIDTH_BIT_12 // Resolución de 12 bits
*/

// Configuracion para servo
#define SERVO_GPIO      GPIO_NUM_23
#define SERVO_FREQ_HZ   50                  // 50 Hz para servos
#define SERVO_TIMER     LEDC_TIMER_0
#define SERVO_MODE      LEDC_HIGH_SPEED_MODE
#define SERVO_CHANNEL   LEDC_CHANNEL_0
#define SERVO_RES       LEDC_TIMER_16_BIT   // Resolución PWM

// Configuracion para boton de prueba
#define BTN1 GPIO_NUM_12

// Convierte ángulo a duty cycle
uint32_t angle_to_duty(uint8_t angle)
{
    uint32_t min_duty = 1450;
    uint32_t max_duty = 7500;

    return min_duty + ((max_duty - min_duty) * angle) / 180;
}

void app_main(void)
{
    /*// Configurar ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    */

    // Configurar botón
    gpio_reset_pin(BTN1);
    gpio_set_direction(BTN1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN1, GPIO_PULLUP_ONLY);

    // Configurar temporizador PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .timer_num        = SERVO_TIMER,
        .duty_resolution  = SERVO_RES,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    ledc_timer_config(&ledc_timer);

    // Configurar canal PWM
    ledc_channel_config_t ledc_channel = {
        .channel    = SERVO_CHANNEL,
        .duty       = 0,
        .gpio_num   = SERVO_GPIO,
        .speed_mode = SERVO_MODE,
        .hpoint     = 0,
        .timer_sel  = SERVO_TIMER
    };

    ledc_channel_config(&ledc_channel);

    int angle = 0;
    int last_state = 1;

    // Posición inicial
    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(angle));
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);
    
    // Bucle de lectura
    while (1) {
        int current_state = gpio_get_level(BTN1);

        // Detectar flanco de bajada (botón presionado)
        if (last_state == 1 && current_state == 0)
        {
            // Cambiar entre 0° y 60°
            if (angle == 0)
                angle = 60;
            else
                angle = 0;+

            ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(angle));
            ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);

            // Pequeño debounce
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        last_state = current_state;

        vTaskDelay(pdMS_TO_TICKS(10));
        
    }
}
