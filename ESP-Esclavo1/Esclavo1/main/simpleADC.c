#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define ADC_CHANNEL    ADC1_CHANNEL_4   // GPIO32
#define ADC_ATTEN      ADC_ATTEN_DB_11  // Permite lectura hasta ~3.3V
#define ADC_WIDTH      ADC_WIDTH_BIT_12 // Resolución de 12 bits

void app_main(void)
{
    // Configurar ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // Bucle de lectura
    while (1) {
        int raw = adc1_get_raw(ADC_CHANNEL);

        // Convertir a voltaje (aprox)
        float voltage = (raw / 4095.0) * 3.3;

        // Mostrar resultados
        printf("Lectura ADC: %d, Voltaje estimado: %.2f V\n", raw, voltage);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
