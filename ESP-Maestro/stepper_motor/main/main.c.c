#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define WIFI_SSID "SANTOS"
#define WIFI_PASS "123456789"

#define IP_ESP1 "192.168.137.12"
#define IP_ESP2 "192.168.137.226"

#define PIN_IN1 GPIO_NUM_19
#define PIN_IN2 GPIO_NUM_18
#define PIN_IN3 GPIO_NUM_5
#define PIN_IN4 GPIO_NUM_17

#define DELAY_US 2500

#define TRIG_PIN_1 GPIO_NUM_33
#define ECHO_PIN_1 GPIO_NUM_32

#define TRIG_PIN_2 GPIO_NUM_26
#define ECHO_PIN_2 GPIO_NUM_25

#define LED_CAMARA GPIO_NUM_21
#define LED_ERROR  GPIO_NUM_23

#define BOTON_PIN  GPIO_NUM_14

static const char *TAG = "CEREBRO";

volatile int direccion = -1;
volatile int paso_secuencia = 0;

const uint8_t secuencia[4] = 
{
    0x09,
    0x0C,
    0x06,
    0x03
};

int cajas_totales = 0;
int cajas_test = 0;

volatile int estado_maquina = 0; 

bool caja_blanca = false;
bool adc_fail = false;
bool backup_activado = false;

char esp_activo_ip[20] = IP_ESP1;

char ultimo_json_esclavo[256] =
"{\"micro\":\"NINGUNO\", \"color\":\"N/A\", \"adc\":0, \"servo\":0, \"estado\":\"ESPERA\"}";

void IRAM_ATTR actualizar_bobinas(uint8_t paso) 
{

    gpio_set_level(PIN_IN1, (paso >> 0) & 0x01);
    gpio_set_level(PIN_IN2, (paso >> 1) & 0x01);
    gpio_set_level(PIN_IN3, (paso >> 2) & 0x01);
    gpio_set_level(PIN_IN4, (paso >> 3) & 0x01);
}

void IRAM_ATTR motor_timer_callback(void* arg) {

    actualizar_bobinas(secuencia[paso_secuencia]);

    paso_secuencia += direccion;

    if(paso_secuencia > 3) paso_secuencia = 0;
    if(paso_secuencia < 0) paso_secuencia = 3;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) 
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Conectando WiFi...");
        esp_wifi_connect();
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi desconectado");
        esp_wifi_connect();
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "CEREBRO CONECTADO - IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void) {
    ESP_LOGI(TAG, "Inicializando WiFi");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

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

void enviar_comando(char *url) {
    ESP_LOGI(TAG, "Enviando comando HTTP: %s", url);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if(err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP OK | Status=%d", esp_http_client_get_status_code(client));
    }
    else {
        ESP_LOGE(TAG, "HTTP FAIL: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void solicitar_lectura(void) {
    char url[100];
    sprintf(url, "http://%s/leer", esp_activo_ip);
    ESP_LOGI(TAG, "Solicitando lectura ADC");
    enviar_comando(url);
}

void solicitar_test_adc(void) {
    char url[100];
    sprintf(url, "http://%s/test_adc", esp_activo_ip);
    ESP_LOGI(TAG, "Solicitando TEST ADC");
    enviar_comando(url);
}

// ---------------- FAILOVER ----------------
void activar_backup(void) {
    ESP_LOGW(TAG, "ALERTA: ACTIVANDO FAILOVER ESP2");
    char url_activar[100];
    char url_standby[100];

    sprintf(url_activar, "http://%s/activar", IP_ESP2);
    sprintf(url_standby, "http://%s/standby", IP_ESP1);

    enviar_comando(url_activar);
    enviar_comando(url_standby);

    strcpy(esp_activo_ip, IP_ESP2);
    ESP_LOGI(TAG, "ESP activo cambiado a ESP2");
}


void restaurar_esp1(void) 
{
    ESP_LOGW(TAG, "ESP1 RECUPERADO - DEVOLVIENDO CONTROL");
    char url_activar[100];
    char url_standby[100];

    sprintf(url_activar, "http://%s/activar", IP_ESP1);
    sprintf(url_standby, "http://%s/standby", IP_ESP2);

    enviar_comando(url_activar);
    enviar_comando(url_standby);

    strcpy(esp_activo_ip, IP_ESP1);
    ESP_LOGI(TAG, "ESP ACTIVO CAMBIADO A ESP1");
}

esp_err_t recibir_datos_handler(httpd_req_t *req) {
    char buf[256];
    int recibidos = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if(recibidos > 0) {
        buf[recibidos] = '\0';
        strcpy(ultimo_json_esclavo, buf);
        ESP_LOGI(TAG, "JSON RECIBIDO: %s", ultimo_json_esclavo);

        if(strstr(buf, "\"color\":\"BLANCO\"")) {
            caja_blanca = true;
            ESP_LOGI(TAG, "Caja BLANCA");
        }
        else if(strstr(buf, "\"color\":\"NEGRO\"")) {
            caja_blanca = false;
            ESP_LOGI(TAG, "Caja NEGRA");
        }

        if(strstr(buf, "\"estado\":\"FAIL\"")) {
            adc_fail = true;
            ESP_LOGE(TAG, "FALLA DETECTADA");

            if(!backup_activado) {
                activar_backup();
                backup_activado = true;
            }
        }
        else if(strstr(buf, "\"estado\":\"RECOVER\"")) {
            ESP_LOGW(TAG, "ESP1 RECUPERADO");
            restaurar_esp1();
            backup_activado = false;
            adc_fail = false;
        }
    }
    
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t api_estado_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "HTTP GET /api/estado");
    char respuesta_html[500];

    sprintf(
        respuesta_html,
        "{\"cajas_totales\":%d, \"alarma_banda\":%d, \"estado_maquina\":%d, \"esclavo_actual\":%s}",
        cajas_totales,
        gpio_get_level(LED_ERROR), 
        estado_maquina, 
        ultimo_json_esclavo
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, respuesta_html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_handle_t iniciar_servidor(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Servidor HTTP iniciado");

        httpd_uri_t ruta_datos = {
            .uri = "/datos",
            .method = HTTP_POST,
            .handler = recibir_datos_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &ruta_datos);

        httpd_uri_t ruta_api = {
            .uri = "/api/estado",
            .method = HTTP_GET,
            .handler = api_estado_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &ruta_api);
    }
    return server;
}

float leer_distancia(gpio_num_t trig_pin, gpio_num_t echo_pin) {
    gpio_set_level(trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(trig_pin, 0);

    int64_t wait_start = esp_timer_get_time();
    bool timeout_flag = false;

    while(gpio_get_level(echo_pin) == 0) {
        if((esp_timer_get_time() - wait_start) > 30000) {
            timeout_flag = true;
            ESP_LOGW(TAG, "Timeout esperando echo");
            break;
        }
    }

    if(!timeout_flag) {
        int64_t start_time = esp_timer_get_time();
        while(gpio_get_level(echo_pin) == 1) {
            if((esp_timer_get_time() - start_time) > 30000) break;
        }
        float distancia = ((esp_timer_get_time() - start_time) * 0.0343) / 2.0;
        return distancia;
    }
    return -1.0;
}

void task_sensor(void *pv) {
    ESP_LOGI(TAG, "Task sensor iniciada");

    const esp_timer_create_args_t motor_timer_args = {
        .callback = &motor_timer_callback,
        .name = "motor_timer",
    };

    esp_timer_handle_t motor_timer;
    esp_timer_create(&motor_timer_args, &motor_timer);
    esp_timer_start_periodic(motor_timer, DELAY_US);
    ESP_LOGI(TAG, "Motor iniciado");

    estado_maquina = 0; // Se usa la variable global
    int64_t tiempo = 0;

    while(1) {
        float dist1 = leer_distancia(TRIG_PIN_1, ECHO_PIN_1);
        vTaskDelay(pdMS_TO_TICKS(50));
        float dist2 = leer_distancia(TRIG_PIN_2, ECHO_PIN_2);

        ESP_LOGI(TAG, "Dist1=%.2f | Dist2=%.2f | Estado=%d", dist1, dist2, estado_maquina);

        if(estado_maquina == 0 && dist1 > 0 && dist1 <= 6) {
            estado_maquina = 1;
            cajas_totales++;
            cajas_test++;
            tiempo = esp_timer_get_time();
            ESP_LOGI(TAG, "CAJA DETECTADA | Total=%d", cajas_totales);
        }

        if(estado_maquina == 1) {
            if((esp_timer_get_time() - tiempo) >= 3500000) {
                ESP_LOGI(TAG, "Activando camara");
                gpio_set_level(LED_CAMARA, 1);
                vTaskDelay(pdMS_TO_TICKS(300));
                
                solicitar_lectura();
                
                tiempo = esp_timer_get_time();
                estado_maquina = 2;
            }
        }

        if(estado_maquina == 2) {
            if((esp_timer_get_time() - tiempo) >= 3000000) {
                gpio_set_level(LED_CAMARA, 0);
                ESP_LOGI(TAG, "Camara OFF");
                estado_maquina = 3;
                tiempo = esp_timer_get_time();
            }
        }

        if(estado_maquina == 3) {
            bool detectado = (dist2 > 0 && dist2 <= 6);
            bool timeout = ((esp_timer_get_time() - tiempo) >= 20000000);

            if(caja_blanca) {
                if(detectado) {
                    ESP_LOGI(TAG, "CLASIFICACION OK");
                    estado_maquina = 0;
                }
                else if(timeout) {
                    ESP_LOGE(TAG, "ERROR CLASIFICACION");
                    gpio_set_level(LED_ERROR, 1);
                    estado_maquina = 4;
                }
            }
            else {
                if(timeout) {
                    ESP_LOGI(TAG, "Caja negra ignorada");
                    estado_maquina = 0;
                }
            }
        }

        if(estado_maquina == 4) {
            ESP_LOGW(TAG, "Esperando reset boton");
            if(gpio_get_level(BOTON_PIN) == 0) {
                ESP_LOGI(TAG, "Boton presionado");
                gpio_set_level(LED_ERROR, 0);
                estado_maquina = 0;
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        }

        if(cajas_test >= 20) {
            ESP_LOGI(TAG, "Ejecutando TEST ADC");
            gpio_set_level(LED_CAMARA, 1);
            vTaskDelay(pdMS_TO_TICKS(300));
            solicitar_test_adc();
            gpio_set_level(LED_CAMARA, 0);
            solicitar_test_adc();
            cajas_test = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "===== INICIANDO CEREBRO =====");
    wifi_init();
    iniciar_servidor();

    gpio_config_t motor_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<PIN_IN1) | (1ULL<<PIN_IN2) | (1ULL<<PIN_IN3) | (1ULL<<PIN_IN4)
    };
    gpio_config(&motor_conf);

    ESP_LOGI(TAG, "Motor configurado");

    gpio_set_direction(TRIG_PIN_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_PIN_1, GPIO_MODE_INPUT);
    gpio_set_direction(TRIG_PIN_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_PIN_2, GPIO_MODE_INPUT);
    gpio_set_direction(LED_CAMARA, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_CAMARA, 0);
    gpio_set_direction(LED_ERROR, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_ERROR, 0);
    gpio_set_direction(BOTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTON_PIN, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "GPIOs configurados");

    xTaskCreatePinnedToCore(task_sensor, "task_sensor", 4096, NULL, 1, NULL, 1);

    ESP_LOGI(TAG, "Sistema listo");
}
