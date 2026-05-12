#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

// Configuracion para ADC
#define ADC_CHANNEL    ADC1_CHANNEL_4   // GPIO32
#define ADC_ATTEN      ADC_ATTEN_DB_11  // Permite lectura hasta ~3.3V
#define ADC_WIDTH      ADC_WIDTH_BIT_12 // Resolución de 12 bits

// Configuracion para servo
#define SERVO_GPIO      GPIO_NUM_23
#define SERVO_FREQ_HZ   50                  // 50 Hz para servos
#define SERVO_TIMER     LEDC_TIMER_0
#define SERVO_MODE      LEDC_HIGH_SPEED_MODE
#define SERVO_CHANNEL   LEDC_CHANNEL_0
#define SERVO_RES       LEDC_TIMER_16_BIT   // Resolución PWM

// Configuracion para OLED
#define CONFIG_SDA_GPIO		GPIO_NUM_21
#define CONFIG_SCL_GPIO		GPIO_NUM_22
#define CONFIG_RESET_GPIO	-1
#define CONFIG_SSD1306_128x64
#define tag "SSD1306"

// Configuracion para boton de prueba
#define BTN1 GPIO_NUM_12

// Configuracion para I2C Heartbeat
#define HB_SDA      GPIO_NUM_18
#define HB_SCL      GPIO_NUM_19
#define I2C_HB      I2C_NUM_1
#define ESP2_ADDR   0x08
#define RX_BUF     128

// Variables OLED
SSD1306_t dev;
char line1[30];
char line2[30];

// Variable para intensidad (deteccion de color)
float intensidad = 0;

// Variable heartbeat
int64_t last_heartbeat = 0;

// Variable respaldo
bool activo = false;

// Configurar botón 
void in_btn(void)
{
    gpio_reset_pin(BTN1);
    gpio_set_direction(BTN1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN1, GPIO_PULLUP_ONLY);
}

// SERVOMOTOR

// Convierte ángulo a duty cycle
uint32_t angle_to_duty(uint8_t angle)
{
    uint32_t min_duty = 1450;
    uint32_t max_duty = 7500;

    return min_duty + ((max_duty - min_duty) * angle) / 180;
}

// Configurar temporizador PWM
void pwm_timer_config(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_MODE,
        .timer_num        = SERVO_TIMER,
        .duty_resolution  = SERVO_RES,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };

    ledc_timer_config(&ledc_timer);
}

// Configurar canal PWM
void pwm_channel_config(void)
{
    ledc_channel_config_t ledc_channel = {
        .channel    = SERVO_CHANNEL,
        .duty       = 0,
        .gpio_num   = SERVO_GPIO,
        .speed_mode = SERVO_MODE,
        .hpoint     = 0,
        .timer_sel  = SERVO_TIMER
    };

    ledc_channel_config(&ledc_channel);
}

// Mover servo al angulo indicado
void servo_move(int angle)
{
    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(angle));
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);
}

// ADC

// Configurar adc
void in_adc(void)
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
}

// OLED
void OLED(void)
{
    ssd1306_clear_screen(&dev, false);
    ssd1306_display_text(&dev, 1, line1, strlen(line1), false);
    ssd1306_display_text(&dev, 3, line2, strlen(line2), false);
}

// Deteccion de colores
void leer_adc(void)
{
    int MAX = 1240; // Valor de prueba para máximo en blanco
    int raw;
    int mediciones;

    vTaskDelay(pdMS_TO_TICKS(300));
    // Captura de valores y promedio
    OLED();
    mediciones = 0;
	for(int i=0; i<4; i++){
		raw = adc1_get_raw(ADC_CHANNEL);
		mediciones += raw;
		float valor = ((float)raw/MAX)*100;
		
		sprintf(lineChar, "Valores: %.0f", valor);
		OLED();		
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	intensidad = ((float)(mediciones/4.0)/MAX)*100;
    
    // Color blanco o negro y movimiento correspondiente de servo
	if(intensidad < 29){
   		sprintf(lineChar, "Negro");
   		servo_move(0);
	}
	else if(intensidad > 30){
    	sprintf(lineChar, "Blanco");
    	servo_move(60);
	}
	OLED();
	
    vTaskDelay(pdMS_TO_TICKS(500));	
}

// I2C HEARTBEAT

// Configurar I2C slave
void init_i2c_slave(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = HB_SDA,
        .scl_io_num = HB_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave.slave_addr = ESP2_ADDR,
        .slave.addr_10bit_en = 0
    };

    i2c_param_config(I2C_HB, &conf);
    i2c_driver_install(I2C_HB, conf.mode, RX_BUF, 0, 0);
}

// Recibir heartbeat
void receive_heartbeat(void)
{
    uint8_t data[10];
    int len = i2c_slave_read_buffer(
        I2C_HB,
        data,
        sizeof(data),
        pdMS_TO_TICKS(50)
    );

    if(len > 0){
        last_heartbeat = esp_timer_get_time();
        sprintf(line1, "ESP1 OK");
        sprintf(line2, "HEARTBEAT");
        OLED_print();
    }
}

// Verificar heartbeat
void check_heartbeat(void)
{
    int64_t now = esp_timer_get_time();

    if((now - last_heartbeat) > 3000000){
        sprintf(line1, "ALERTA ESP1");
        sprintf(line2, "SIN HEARTBEAT");
        OLED_print();

        // Activar respaldo
        activo = true;
    }
    else{

        activo = false;
    }
}

void task_system(void *pv)
{
    int last_btn = 1;

    while(1){
        receive_heartbeat();
        check_heartbeat();

        // Boton de prueba
        int current_btn = gpio_get_level(BTN1);
        if(last_btn == 1 && current_btn == 0){
            leer_adc();
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        last_btn = current_btn;
        
        // Respaldo automatico
        if(activo){
            leer_adc();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
	// Configuracion PWM
    pwm_timer_config();
    pwm_channel_config();

    // Posicion inicial del servo
    servo_move(0);

    // Configuracion OLED
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);
    vTaskDelay(pdMS_TO_TICKS(300));
    
    // Configuracion I2C heartbeat
    init_i2c_slave();

    last_heartbeat = esp_timer_get_time();

    sprintf(line1, "ESP2 BACKUP");
    sprintf(line2, "ESPERANDO");

    OLED_print();

    xTaskCreate(task_system, "task_system", 4096, NULL, 1, NULL);
}
