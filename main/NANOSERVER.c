#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdint.h>
#include <esp_psram.h>
#include <assert.h>
#include "lwip/sockets.h"
#include "esp_task_wdt.h"
#include "lwip/inet.h"

#include "naneye.h"

static naneye_t naneye;
uint8_t *buffer1 = NULL;
uint8_t *buffer2 = NULL;
static uint8_t *write_buffer;
static uint8_t *read_buffer;

bool stop_stream       = false;
bool naneye_present    = true;
bool poncha040_present = false;

#define IMG_SIZE_BYTES NANEYE_IMG_W * NANEYE_IMG_H * NANEYE_PIXEL_BITS / 8

#define SERVER_IP  "192.168.4.2"
#define SEND_PORT 5001
#define RECV_PORT 5002


#define  WEBSERVER  1
#define  UPD_SENDER 0


static const char *TAG = "NANOSERVER";

#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK  13
#define PIN_NUM_CS   5
#define GPIO_TOGGLE  14
#define GPIO_ILLUM   10


// Function to turn on the LDO (Low Dropout Regulator)
void LDO_ON() 
{
    gpio_set_level(GPIO_TOGGLE, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(GPIO_TOGGLE, 1);
}


//Function to initialize the WiFi access point
void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "NANEYE_CAM",
            .ssid_len = strlen("NANEYE_CAM"),
            .password = "12345678",
            .channel = 6,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .beacon_interval = 100
        }
    };

    if (strlen((char *)ap_config.ap.password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(esp_wifi_set_protocol(
        WIFI_IF_AP,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N
    ));

    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT40));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    // Estas ficam depois do start
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(60));

    ESP_LOGI("WIFI", "AP iniciado. SSID: %s", ap_config.ap.ssid);
}

//Core 0 task for SPI communication loop to NANEYE camera
void core0_spi_loop(void *arg){
 

    while (stop_stream){}

select_sensor:

    if(naneye_present)
    {
        ESP_LOGI(TAG, "NANEYE PRESENT");

        naneye_config_t cfg = {
        .spi_host = SPI3_HOST,
        .pin_mosi = PIN_NUM_MISO ,
        .pin_miso = PIN_NUM_MOSI,
        .pin_sclk = PIN_NUM_CLK,
        .pin_cs   = PIN_NUM_CS,
        .spi_clock_hz = 31 * 1000 * 1000,
        .max_transfer_sz = 65536
        };

        naneye_init(&naneye,&cfg,write_buffer,read_buffer);
        naneye_start(&naneye);

        while (1) 
        {
            naneye_capture_frame(&naneye);
            if(stop_stream)
            {
                stop_stream = false;
                naneye_deinit(&naneye);
                goto select_sensor;
            }
        }
    }

    if(poncha040_present)
    {
        ESP_LOGI(TAG, "PONCHA040 PRESENT");
    }

}


#if WEBSERVER

    extern const uint8_t index_html_start[] asm("_binary_index_html_start");
    extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

    static esp_err_t index_handler(httpd_req_t *req)
    {
        size_t index_html_size = index_html_end - index_html_start;

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, (const char *)index_html_start, index_html_size);

        return ESP_OK;
    }

    static esp_err_t image_handler(httpd_req_t *req)
    {
        httpd_resp_set_type(req, "application/octet-stream");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_send(req, (char *)naneye.read_buffer, IMG_SIZE_BYTES);
        return ESP_OK;
    }

    static esp_err_t reset_handler(httpd_req_t *req)
    {
        ESP_LOGI(TAG, "RESET SENSOR");
        stop_stream = true;
        LDO_ON();
        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    static esp_err_t illumination_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;
        ESP_LOGI(TAG, "ILLUMINATION CONTROL");

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    static esp_err_t gain_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
                naneye_set_gain(&naneye, atoi(param));
                ESP_LOGE("MAIN", "GAIN");
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    static esp_err_t exposure_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
                naneye_set_exposure(&naneye, atoi(param));
                ESP_LOGE("MAIN", "EXPOSSURE");
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    static esp_err_t select_sensor_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) 
            {
                if(atoi(param) == 1){
                    naneye_present = true;
                    poncha040_present = false;
                }
                if(atoi(param) == 0){
                    naneye_present = false;
                    poncha040_present = true; 
                }
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    static httpd_handle_t start_webserver(void)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = 80;
        config.lru_purge_enable = true;

        ESP_LOGI(TAG, "Init webserver...");

        httpd_handle_t server = NULL;
        if (httpd_start(&server, &config) == ESP_OK)
        {
            httpd_uri_t index_page = {
                .uri = "/",
                .method = HTTP_GET,
                .handler = index_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &index_page);

            httpd_uri_t image_page = {
                .uri = "/image",
                .method = HTTP_GET,
                .handler = image_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &image_page);

            httpd_uri_t exposure_page = {
                .uri = "/set_exposure",
                .method = HTTP_GET,
                .handler = exposure_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &exposure_page);

            httpd_uri_t gain_page = {
                .uri = "/set_gain",
                .method = HTTP_GET,
                .handler = gain_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &gain_page);

            httpd_uri_t illum_page = {
                .uri = "/illum",
                .method = HTTP_GET,
                .handler = illumination_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &illum_page);

            httpd_uri_t reset_page = {
                .uri = "/reset",
                .method = HTTP_GET,
                .handler = reset_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &reset_page);

            httpd_uri_t select_sensor = {
                .uri = "/select_sensor",
                .method = HTTP_GET,
                .handler = select_sensor_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &select_sensor);

            ESP_LOGI(TAG, "Webserver iniciado na porta 80");
            return server;
        }

        ESP_LOGE(TAG, "Erro ao iniciar webserver");
        return NULL;
    }

    // Core 1 task for running the web server
    void core1_webserver(void *arg)
    {
        ESP_LOGI(TAG, "Core 1: INIT WEBSERVER...");
        start_webserver();
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

#endif

#if UPD_SENDER

int udp_create_tx_socket(struct sockaddr_in *dest_addr)
{
    int sock;

    dest_addr->sin_family = AF_INET;
    dest_addr->sin_port = htons(SEND_PORT);
    dest_addr->sin_addr.s_addr = inet_addr(SERVER_IP);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Erro ao criar socket TX");
        return -1;
    }

    ESP_LOGI(TAG, "Socket TX criado para %s:%d", SERVER_IP, SEND_PORT);
    return sock;
}

int udp_create_rx_socket()
{
    int sock;
    struct sockaddr_in rx_addr;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Erro ao criar socket RX");
        return -1;
    }

    rx_addr.sin_family = AF_INET;
    rx_addr.sin_port = htons(RECV_PORT);
    rx_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&rx_addr, sizeof(rx_addr)) < 0) {
        ESP_LOGE(TAG, "Erro no bind RX");
        close(sock);
        return -1;
    }
    ESP_LOGI(TAG, "Socket RX à escuta na porta %d", RECV_PORT);
    return sock;
}


void core1_udp_loop(void *arg) {

    struct sockaddr_in dest_addr;

    int rx_sock = udp_create_rx_socket();
    int tx_sock = udp_create_tx_socket(&dest_addr);

    if (rx_sock < 0  || tx_sock < 0) return;

    buffer_ready = true;

    uint8_t rx_buffer[16];

    int num_packets = 320;     
    int raw_bytes_per_line = 492; // 328 pixels × 12 bits / 8
    int i = 0;
    int len;
    while (1) {
        len = recvfrom(rx_sock,rx_buffer,sizeof(rx_buffer),MSG_DONTWAIT,NULL,NULL);

        if (len > 0)
        {
            exposure = rx_buffer[0];
            gain     = rx_buffer[2];
            ESP_LOGI("WIFI", "EXP: %d", rx_buffer[0]);
            ESP_LOGI("WIFI", "GAIN: %d", rx_buffer[2]);
            update_reg();
        }
        while(i < num_packets){
            memcpy(packet, &read_buffer[i * raw_bytes_per_line], raw_bytes_per_line);
            packet[492]     = (uint8_t)(i & 0xFF);         // LSB
            packet[493]     = (uint8_t)((i >> 8) & 0xFF);  // MSB
            sendto(tx_sock, packet, 494, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            i++;
        }
        i = 0;
    }
}

#endif


//Main application entry point
void app_main(void) {

    gpio_set_direction(GPIO_TOGGLE, GPIO_MODE_OUTPUT);
    LDO_ON();

    gpio_set_direction(GPIO_ILLUM, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_ILLUM, 0); // Turn on illumination

    buffer1 = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);
    buffer2 = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);

    if (!buffer1 || !buffer2) {
        ESP_LOGE("MAIN", "Failed to allocate buffers");
        return;
    }

    write_buffer  = buffer1;
    read_buffer   = buffer2;
    
    esp_task_wdt_deinit();
    wifi_init_ap();

    xTaskCreatePinnedToCore(core0_spi_loop,  "core0_spi", 8192,NULL, 1, NULL, 0 );
    
    #if WEBSERVER
        xTaskCreatePinnedToCore(core1_webserver, "core1_webserver", 8192,NULL, 1, NULL, 1 );
    #else
        packet = heap_caps_malloc(494, MALLOC_CAP_SPIRAM);
        xTaskCreatePinnedToCore(core1_udp_loop,  "core1_udp_loop" , 8192,NULL, 1, NULL, 1 );
    #endif
}

