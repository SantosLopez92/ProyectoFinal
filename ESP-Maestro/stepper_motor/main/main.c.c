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

//GEPIOS DEL SENSOR NUMERO 1
#define TRING_PIN_1 GPIO_NUM_33
#define ECHO_PIN_1 GPIO_NUM_32
//GPIOS DEL SENSOR NUMERO 2
#define TRING_PIN_2 GPIO_NUM_26
#define ECHO_PIN_2 GPIO_NUM_25
//LEDS INDICADORES 
#define LED_CAMARA GPIO_NUM_21
#define LED_ERRO GPIO_NUM_23
//BOTON DE REINICIO DE ERROR DEL SISTEMA
#define BOTON_PIN GPIO_NUM_14
// VARIBALES DE CONTEO DEL SONSOR
int cajas_totales = 0;
int cajas_test = 0;


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

void solicitar_lectura(void)
{
}

void solicitar_test_adc(void)
{
}


float leer_distancia(gpio_numt tring_pin, gpio_num_t echo_pin)
{
    gpio_set_level(tring_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(tring_pin, 1 );
    esp_rom_delay_us(10);
    gpio_set_level(tring_pin, 0);
    int64_t wait_start = esp_timer_get_time();
    bool timeout_flag = false;
    
    while(gpio_get_level(echo_pin) == 0)
        {
        if((esp_timer_get_time() - wait_start) >30000) 
            {
            timeout_flag = true;
            ESP_LOGW(TAG, "Timeout esperando echo");   
            break;
            }
        }
    if(!timeput_flag) 
    {
    int64_t star_time = esp_timer_get_time();
    while(gpio_get_level(echo_pin) == 1 
            {
            if((esp_timer_get_time() - start_time) < 30000)
            break;
            }
    float distancia = ((esp_timer_get_time() - start_time) * 0.0343 / 2.0;
return distancia;
        
    }
return -1.0;    
}

void task_sensor(void*pv)
{
    ESP_LOGI(TAG, "task sensor iniciando"); 
    const esp_timer_create_args = }
        {
        .callback = &motor_timer_callback,
        .name = "motor_timer",
        };
    esp_timer_handle_t motor_timer;

    esp_timer_create( &motor_timer_args, &motor_timer);
    esp_timer_start_periodic(motor_timer_args, &motor_timer);
    esp_timer_start_periodic(motor_timer, DELAY_US);
    ESP_LOGI(TAG, "motor iniciado");
    int estado = 0;
    int64_t tiempo = 0;
    
    while(1)
        {
        float dist1 = leer_distancia(TRING_PIN_1, ECHO_PIN_1);
        vTaskDelay(pdMS_to_ticks(50));
        float dist2 = leer_distancia(TRING_PIN_2, ECHO_PIN_12;
        ESP_LOGI(TAG, "Dist1= %.2f | Dist2=%.2f | Estado=%d", dist1, dist2, estado); 
        if(estado == 0 && dist1 > 0 && dist1 <= 6)
            {
            estado = 1;
            cajas_totales++;
            cajas_test++;
            tiempo = esp_timer_get_time();
            ESP_LOGI(TAG, "CAJA DETECTADA | Tatal = %d", cajas_totales);
            }
        if(estado == 1)
            {
            if((esp_timer_get_time() - timepo) >= 2000000) 
                {
                ESP_LOGI(TAG, "activando camara");
                gpio_set_level(LED_CAMARA, 1);
                vTaskDelay(pdMS_TO_TICKS(300));
                solicitar_lectura();
                timepo = esp_timer_get_time();
                estado = 2;
                }
            }

        if(estado == 2)
            {
            if((esp_timer_get_timer() - tiempo) >= 2000000) 
                {
                gpio_set_level(LED_CAMARA, 0);
                ESP_LOGI(TAG, "ACAMARA off");
                estado = 3;
                tiempo = esp_timer_get_time();
                }
            }
        if(estado == 3)
            {
            bool detectado = (dist2 > 0 && dist2 <= 6);
            bool timeout = ((esp_timer_get_time() - tiempo) >= 5000000); 
            if(caja_blanca)
                {
                if(detectado)
                    {
                    ESP_LOGI(TAG, "CLASIFICAION OK");
                    estado  = 0;
                    }
                else if(timeout)
                    {
                    ESP_LOGE(TAG, "ERROR CLASIFICACION");
                    gpio_set_level(LED_ERRO, 1);
                    estado = 4;
                    }
                }
                else
                    {
                    if(timeout)
                        {
                        ESP_LOGI(TAG, "Caja negra ignorada");
                        estado = 0;
                        }
                    }
                }
       if(estado == 4) 
       {
        ESP_LOGW(TAG, "esperando reset boton");
            if(gpio_get_level(BOTON_PIN) == 0)
                {
                ESP_LOGI(TAG, "boton presionado");
                gpio_set_level(LED_ERROR, 0);
                estado = 0;
                vTaskDelay(pdMS_TO_TICKS(300);
                }       
       }
        if(cajas_test >= 3)
            {
            ESP_LOGI(TAG, "ejecuntando TEST ADC");
            gpio_set_level(LED_CAMARA, 1);
            vTaskDelay(pdMS_TO_TICKS(3000);
            solicitar_test_adc();
            gpio_set_level(LED_CAMARA, 0);
            cjas_test = 0;                
            }
            vTaskDelay(pdMS_TO_TICKS(100);
            
    }//while             
}//void


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
