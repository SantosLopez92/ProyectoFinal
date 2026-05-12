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

// Variables OLED
int center=3;
char lineChar[20];
SSD1306_t dev;
char lastLine[20] = "";

// Variable para intensidad (deteccion de color)
float intensidad = 0;

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
	if(strcmp(lineChar, lastLine) != 0){
   		ssd1306_clear_screen(&dev, false);
    	ssd1306_display_text(&dev, center, lineChar, strlen(lineChar), false);
    	strcpy(lastLine, lineChar);
	}
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

// Configurar I2C maestro
void init_i2c_master(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HB_SDA,
        .scl_io_num = HB_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };

    i2c_param_config(I2C_HB, &conf);

    i2c_driver_install(
        I2C_HB,
        conf.mode,
        0,
        0,
        0
    );
}

// Enviar heartbeat al ESP2
void send_heartbeat(void)
{
    i2c_cmd_handle_t cmd =
        i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(
        cmd,
        (ESP2_ADDR << 1) | I2C_MASTER_WRITE,
        true
    );

    char msg[] = "HB";

    i2c_master_write(cmd, (uint8_t*)msg, strlen(msg), true);

    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_HB, cmd, pdMS_TO_TICKS(100));

    i2c_cmd_link_delete(cmd);
}

// Enviar heartbeat cada segundo
void task_heartbeat(void *pv)
{
    while(1)
    {
        send_heartbeat();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
	// Configuracion PWM
    pwm_timer_config();
    pwm_channel_config();

    // Variable para flanco de bajada BTN1
    int last_state_adc = 1;

    // Posicion inicial del servo
    servo_move(0);

    // Configuracion OLED
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);
    vTaskDelay(pdMS_TO_TICKS(300));

    // Inicializacion de ADC y botón
    in_adc();
    in_btn();
    
    // Configuracion I2C heartbeat
    init_i2c_master();

    // Mensaje inicial
    sprintf(lineChar, "ESP1 ACTIVO");

    OLED();

    // Task heartbeat
    xTaskCreate(task_heartbeat,"task_heartbeat",4096,NULL,1,NULL);

    // Bucle de lectura
    while (1) {
        // Boton prueba adc
        int current_state_adc = gpio_get_level(BTN1);

        // Detectar flanco de bajada
        if (last_state_adc == 1 && current_state_adc == 0)
        {
            leer_adc();

            // Debounce
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        last_state_adc = current_state_adc;
        vTaskDelay(pdMS_TO_TICKS(10));        
    }
}
