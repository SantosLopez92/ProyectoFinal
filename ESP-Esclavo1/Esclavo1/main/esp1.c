#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "ssd1306.h"
#include "font8x8_basic.h"

// Configuracion WiFi
#define WIFI_SSID      "SANTOS"
#define WIFI_PASS      "123456789"
#define IP_CEREBRO     "192.168.137.64"

// Configuracion para ADC
#define ADC_CHANNEL    ADC1_CHANNEL_4
#define ADC_ATTEN      ADC_ATTEN_DB_11
#define ADC_WIDTH      ADC_WIDTH_BIT_12

// Configuracion para servo
#define SERVO_GPIO      GPIO_NUM_23
#define SERVO_FREQ_HZ   50
#define SERVO_TIMER     LEDC_TIMER_0
#define SERVO_MODE      LEDC_HIGH_SPEED_MODE
#define SERVO_CHANNEL   LEDC_CHANNEL_0
#define SERVO_RES       LEDC_TIMER_16_BIT

// Configuracion para OLED
#define CONFIG_SDA_GPIO     GPIO_NUM_21
#define CONFIG_SCL_GPIO     GPIO_NUM_22
#define CONFIG_RESET_GPIO   -1
#define CONFIG_SSD1306_128x64
#define tag "SSD1306"

// Configuracion para I2C Heartbeat
#define HB_SDA      GPIO_NUM_18
#define HB_SCL      GPIO_NUM_19
#define I2C_HB      I2C_NUM_1
#define ESP2_ADDR   0x08

// Variables OLED
int center = 1;
char line1[30];
char line2[30];
SSD1306_t dev;

// Variable intensidad
float intensidad = 0;

// Variables de control
bool activo = true;
volatile bool solicitar_medicion = false;

static const char *TAG = "ESP1";

// OLED

void OLED(void)
{
    ssd1306_clear_screen(&dev, false);

    ssd1306_display_text(&dev, 1, line1, strlen(line1), false);
    ssd1306_display_text(&dev, 3, line2, strlen(line2), false);
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

// Mover servo
void servo_move(int angle)
{
    ESP_LOGI(TAG, "Moviendo servo a %d grados", angle);

    ledc_set_duty(SERVO_MODE, SERVO_CHANNEL, angle_to_duty(angle));
    ledc_update_duty(SERVO_MODE, SERVO_CHANNEL);
}

// ADC

// Configurar adc
void in_adc(void)
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    ESP_LOGI(TAG, "ADC configurado");
}

// HTTP CLIENT

// Enviar datos JSON al ESP3
void enviar_json(const char *micro, const char *color, int adc, int servo, const char *estado)
{
    char json_payload[200];

    sprintf(json_payload,
            "{\"micro\":\"%s\", \"color\":\"%s\", \"adc\":%d, \"servo\":%d, \"estado\":\"%s\"}",
            micro, color, adc, servo, estado);

    char url_cerebro[100];

    sprintf(url_cerebro, "http://%s/datos", IP_CEREBRO);

    esp_http_client_config_t config = {
        .url = url_cerebro,
        .method = HTTP_METHOD_POST
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "POST OK");
    }
    else
    {
        ESP_LOGE(TAG, "POST FAIL");
    }

    esp_http_client_cleanup(client);
}

// Deteccion de colores
void leer_adc(void)
{
    int MAX = 1240;
    int raw;
    int mediciones = 0;

    servo_move(180);
    vTaskDelay(pdMS_TO_TICKS(500));

    for(int i = 0; i < 4; i++)
    {
        raw = adc1_get_raw(ADC_CHANNEL);
        mediciones += raw;

        ESP_LOGI(TAG, "Lectura %d = %d", i + 1, raw);

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    int promedio = mediciones / 4;

    intensidad = ((float)promedio / MAX) * 100;

    if(intensidad < 29)
    {
        sprintf(line1, "NEGRO");
        sprintf(line2, "Valor %.0f", intensidad);

        servo_move(150);

        enviar_json("ESP1", "NEGRO", promedio, 150, "OK");
    }
    else
    {
        sprintf(line1, "BLANCO");
        sprintf(line2, "Valor %.0f", intensidad);

        servo_move(180);

        enviar_json("ESP1", "BLANCO", promedio, 180, "OK");
    }

    OLED();

    vTaskDelay(pdMS_TO_TICKS(2000));

    sprintf(line1, "ESP1 ACTIVO");
    sprintf(line2, "ESPERANDO");

    OLED();
}

// Test ADC
bool test_adc(void)
{
    int adc_oscuro;
    int adc_iluminado;

    vTaskDelay(pdMS_TO_TICKS(300));
    adc_oscuro = adc1_get_raw(ADC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(300));
    adc_iluminado = adc1_get_raw(ADC_CHANNEL);

    int delta = adc_iluminado - adc_oscuro;

    ESP_LOGI(TAG, "Delta ADC = %d", delta);

    return (delta > 300);
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

// Enviar heartbeat
void send_heartbeat(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

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

// WIFI

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if(event_base == WIFI_EVENT &&
       event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }

    else if(event_base == WIFI_EVENT &&
            event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
}

// Inicializar WiFi
void wifi_init(void)
{
    nvs_flash_init();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// HTTP SERVER

esp_err_t leer_handler(httpd_req_t *req)
{
    if(activo)
    {
        solicitar_medicion = true;
    }

    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t test_handler(httpd_req_t *req)
{
    if(test_adc())
    {
        sprintf(line1, "ADC OK");

        enviar_json("ESP1", "TEST", 0, 0, "OK");
    }
    else
    {
        sprintf(line1, "FALLA ADC");

        enviar_json("ESP1", "TEST", 0, 0, "FAIL");
    }

    sprintf(line2, "");
    OLED();

    httpd_resp_send(req, "TEST", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t standby_handler(httpd_req_t *req)
{
    activo = false;

    sprintf(line1, "MODO ESPERA");
    sprintf(line2, "RESPALDO");

    OLED();

    httpd_resp_send(req, "STANDBY", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t activar_handler(httpd_req_t *req)
{
    activo = true;

    sprintf(line1, "ESP1 ACTIVO");
    sprintf(line2, "OPERANDO");

    OLED();

    httpd_resp_send(req, "ACTIVE", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_handle_t iniciar_servidor(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t leer_uri = {
            .uri = "/leer",
            .method = HTTP_GET,
            .handler = leer_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &leer_uri);

        httpd_uri_t test_uri = {
            .uri = "/test_adc",
            .method = HTTP_GET,
            .handler = test_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &test_uri);

        httpd_uri_t standby_uri = {
            .uri = "/standby",
            .method = HTTP_GET,
            .handler = standby_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &standby_uri);

        httpd_uri_t activar_uri = {
            .uri = "/activar",
            .method = HTTP_GET,
            .handler = activar_handler,
            .user_ctx = NULL
        };

        httpd_register_uri_handler(server, &activar_uri);
    }

    return server;
}

// TASK HEARTBEAT

void task_heartbeat(void *pv)
{
    while(1)
    {
        send_heartbeat();

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// TASK MEDICION

void task_medicion(void *pv)
{
    while(1)
    {
        if(solicitar_medicion)
        {
            solicitar_medicion = false;

            leer_adc();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    // Configuracion PWM
    pwm_timer_config();
    pwm_channel_config();

    // Posicion inicial del servo
    servo_move(180);

    // Configuracion ADC
    in_adc();

    // Configuracion OLED
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&dev, 128, 64);

    // Configuracion I2C heartbeat
    init_i2c_master();

    // Configuracion WiFi
    wifi_init();

    // Iniciar servidor HTTP
    iniciar_servidor();

    // Mensaje inicial
    sprintf(line1, "ESP1 ACTIVO");
    sprintf(line2, "ESPERANDO");

    OLED();

    // Task heartbeat
    xTaskCreate(task_heartbeat, "task_heartbeat", 4096, NULL, 1, NULL);

    // Task medicion
    xTaskCreate(task_medicion, "task_medicion", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "Sistema listo");
}
