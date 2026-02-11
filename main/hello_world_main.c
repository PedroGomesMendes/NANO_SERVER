// #include "driver/spi_master.h"
// #include "driver/gpio.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "lwip/sockets.h"
// #include "lwip/inet.h"
// #include <string.h>
// #include <stdint.h>
// #include <esp_psram.h>
// #include "esp_task_wdt.h"
// #include <assert.h>

// #define PIN_NUM_MISO 12
// #define PIN_NUM_MOSI 17
// #define PIN_NUM_CLK  13
// #define PIN_NUM_CS   5
// #define GPIO_TOGGLE  38

// #define IMG_W 328
// #define IMG_H 320

// #define PIXEL_SIZE 2
// #define IMG_SIZE_BYTES (IMG_W * IMG_H * 12/8)

// #define DMA_CHUNK_SIZE (328*4*12/8) 
// static uint16_t *rx_psram_buffer = NULL;
// static uint8_t *packet = NULL;


// extern volatile bool buffer_ready;  // sinaliza que há dados prontos


// #define WIFI_SSID "NOS-3481"
// #define WIFI_PASS "WHPNSR9R"
// #define SERVER_IP "192.168.1.176"
// #define SERVER_PORT 5000

// static const char *TAG = "SPI_UDP";

// volatile uint16_t *rx_buffer = NULL;
// volatile bool buffer_ready = false;


// void pin_toggle() {
//     gpio_set_level(GPIO_TOGGLE, 1);
//     gpio_set_level(GPIO_TOGGLE, 0);
// }

// // Função SPI básica de envio
// esp_err_t spi_send_data(spi_device_handle_t handle, const uint8_t *data, size_t bits) {
//     spi_transaction_t t;
//     memset(&t, 0, sizeof(t));
//     t.length = bits;
//     t.tx_buffer = data;
//     t.flags = 0;


//     return spi_device_transmit(handle, &t);
// }

// // Função SPI básica de leitura DMA
// esp_err_t spi_read_dma_bits(spi_device_handle_t handle, void *dest_addr, size_t bits) {
//     if (!dest_addr || bits == 0) return ESP_ERR_INVALID_ARG;

//     spi_transaction_t t;
//     memset(&t, 0, sizeof(t));
//     t.length = bits;
//     t.rx_buffer = dest_addr;
//     t.tx_buffer = NULL;
//     t.flags = 0;

//     return spi_device_transmit(handle, &t);
// }

// // Inicialização Wi-Fi
// void wifi_init(void) {
//     ESP_ERROR_CHECK(nvs_flash_init());
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = WIFI_SSID,
//             .password = WIFI_PASS
//         }
//     };

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_ERROR_CHECK(esp_wifi_connect());
// }

// // Cria socket UDP
// int udp_create_socket(struct sockaddr_in *dest_addr) {
//     int sock;
//     dest_addr->sin_family = AF_INET;
//     dest_addr->sin_port = htons(SERVER_PORT);
//     dest_addr->sin_addr.s_addr = inet_addr(SERVER_IP);

//     sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//     if (sock < 0) {
//         ESP_LOGE(TAG, "Erro ao criar socket UDP");
//         return -1;
//     }

//     ESP_LOGI(TAG, "Socket UDP criado para %s:%d", SERVER_IP, SERVER_PORT);
//     return sock;
// }

// // Core 0: faz SPI e preenche buffer
// void core0_spi_loop(void *arg) {
    
//     uint16_t *frame_buffer = (uint16_t *)arg;  // recebe o ponteiro
//     uint16_t *temp         = (uint16_t *)arg;  // recebe o ponteiro


//     gpio_reset_pin(GPIO_TOGGLE);
//     gpio_set_direction(GPIO_TOGGLE, GPIO_MODE_OUTPUT);


//     spi_bus_config_t buscfg = {
//         .miso_io_num = PIN_NUM_MISO,
//         .mosi_io_num = PIN_NUM_MOSI,
//         .sclk_io_num = PIN_NUM_CLK,
//         .quadwp_io_num = -1,
//         .quadhd_io_num = -1,
//         .max_transfer_sz = 4096 // buffer completo
//     };
//     ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

//     spi_device_interface_config_t devcfg = {
//         .clock_speed_hz = 4 * 1000 * 1000,
//         .mode = 0,
//         .spics_io_num = PIN_NUM_CS,
//         .queue_size = 1
//     };

//     spi_device_handle_t handle;
//     ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &handle));
//     uint8_t tx_data[3];

//     // Sequência SPI inicial
   
//     tx_data[0] = 0x00;                                              
//     spi_send_data(handle, tx_data, 1);
//     // tx_data[0] = 0x91;  tx_data[1] = 0x41;  tx_data[2] = 0xE3;      
//     // spi_send_data(handle, tx_data, 24);
//     tx_data[0] = 0x92;  tx_data[1] = 0x00;  tx_data[2] = 0xCA;      
//     spi_send_data(handle, tx_data, 24);
//     tx_data[0] = 0x00;  tx_data[1] = 0x00;                          
//     spi_send_data(handle, tx_data, 10);

//     // Primeiras leituras para sincronização
//     spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 329);
//     spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 656);
//     spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 656);

//     int offset=0;

//     while (1) 
//     {
//         if (offset < IMG_H) {

//             // spi_read_dma_bits(handle,frame_buffer + (offset * 246),IMG_W * 12);
//             // offset += 1;

//             spi_read_dma_bits(handle, &frame_buffer[offset * 246 ], IMG_W * 12 * 8);
//             offset= offset + 8; 
            
//         } else {
//             pin_toggle();
//             buffer_ready = true;
//             spi_send_data(handle, (void *)rx_buffer, 12 * 328 );
//             spi_send_data(handle, (void *)rx_buffer, 12 * 8   );
//             spi_send_data(handle, (void *)rx_buffer, 12 * 648 );
//             spi_send_data(handle, (void *)rx_buffer, 12 * 656 );
//             spi_send_data(handle, (void *)rx_buffer, 12 * 328 );
//             offset = 0;
//             pin_toggle();
//         }
//     }
// }



// void core1_udp_loop(void *arg) {
//     uint8_t *frame_buffer = (uint8_t *)arg; // ponteiro RAW12 empacotado
//     packet = heap_caps_malloc(494, MALLOC_CAP_SPIRAM);

//     struct sockaddr_in dest_addr;
//     int sock = udp_create_socket(&dest_addr);
//     if (sock < 0) return;

//     int num_packets = 320;     
//     int raw_bytes_per_line = 492; // 328 pixels × 12 bits / 8
//     uint8_t packet[raw_bytes_per_line + 2]; // +2 bytes para índice da linha
//     int i = 0;

//     while (1) {
//         while(i < num_packets){
//             memcpy(packet, &frame_buffer[i * raw_bytes_per_line], raw_bytes_per_line);
//             packet[492]     = (uint8_t)(i & 0xFF);       // LSB
//             packet[493]     = (uint8_t)((i >> 8) & 0xFF); // MSB
//             sendto(sock, &packet, 494, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//             i++;
//         }
//         i =0;
//     }
// }


// void app_main(void) {

//     ESP_LOGI(TAG, "Main running");

//     rx_psram_buffer = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);
//     if (!rx_psram_buffer) {
//         ESP_LOGE("MAIN", "Falha ao alocar rx_psram_buffer (%d bytes)", IMG_SIZE_BYTES);
//         return;
//     }
//     esp_task_wdt_deinit();


//     wifi_init();
//     xTaskCreatePinnedToCore(core0_spi_loop, "core0_spi", 8192,(void *)rx_psram_buffer, 1, NULL, 0 );
//     xTaskCreatePinnedToCore(core1_udp_loop, "core1_udp", 8192,(void *)rx_psram_buffer, 1, NULL, 1 );
// }


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


#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK  13
#define PIN_NUM_CS   5
#define GPIO_TOGGLE  38

#define IMG_W 328
#define IMG_H 320

#define PIXEL_SIZE 2
#define IMG_SIZE_BYTES (IMG_W * IMG_H * 12/8)

static uint16_t *rx_psram_buffer = NULL;
static uint16_t *tx_psram_buffer = NULL;
static uint8_t *packet = NULL;


extern volatile bool buffer_ready;  // sinaliza que há dados prontos
extern volatile bool ready;  // sinaliza que há dados prontos


#define WIFI_SSID "NOS-3481"
#define WIFI_PASS "WHPNSR9R"
#define SERVER_IP "192.168.1.176"
#define SERVER_PORT 5000

static const char *TAG = "SPI_UDP";

volatile uint16_t *rx_buffer = NULL;
volatile bool buffer_ready = false;
volatile bool ready = false;


void pin_toggle() {
    gpio_set_level(GPIO_TOGGLE, 1);
    gpio_set_level(GPIO_TOGGLE, 0);
}

// Função SPI básica de envio
esp_err_t spi_send_data(spi_device_handle_t handle, const uint8_t *data, size_t bits) {
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = bits;
    t.tx_buffer = data;
    t.flags = 0;
    return spi_device_transmit(handle, &t);
}

// Função SPI básica de leitura DMA
esp_err_t spi_read_dma_bits(spi_device_handle_t handle, void *dest_addr, size_t bits) {
    if (!dest_addr || bits == 0) return ESP_ERR_INVALID_ARG;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = bits;
    t.rx_buffer = dest_addr;
    t.tx_buffer = NULL;
    t.flags = 0;

    return spi_device_transmit(handle, &t);
}

// Inicialização Wi-Fi
void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// Core 0: faz SPI e preenche buffer
void core0_spi_loop(void *arg) {
    
    uint16_t *frame_buffer = (uint16_t *)arg;  // recebe o ponteiro

    gpio_reset_pin(GPIO_TOGGLE);
    gpio_set_direction(GPIO_TOGGLE, GPIO_MODE_OUTPUT);


    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096 // buffer completo
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1
    };

    spi_device_handle_t handle;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &handle));
    uint8_t tx_data[3];

    while (!ready){    }


    int offset=0;
    tx_data[0] = 0x00;                                          spi_send_data(handle, tx_data, 1);
    tx_data[0] = 0x91;  tx_data[1] = 0x2D;  tx_data[2] = 0x3A;  spi_send_data(handle, tx_data, 24);
    tx_data[0] = 0x92;  tx_data[1] = 0x00;  tx_data[2] = 0xC8;  spi_send_data(handle, tx_data, 24);
    tx_data[0] = 0x00;  tx_data[1] = 0x00;  tx_data[2] = 0x00;  spi_send_data(handle, tx_data, 23);

    spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 328);
    spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 656);

    while (1) 
    {
        while (offset < IMG_H) 
        {
            spi_read_dma_bits(handle, &frame_buffer[offset * 246 ], IMG_W * 12 * 8);
            offset= offset + 8;
        } 
        spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 8);
        offset = 0;
        memcpy(tx_psram_buffer,frame_buffer,IMG_SIZE_BYTES);

        // spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 656);
        // spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 656);
        // spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 328);

        //ESP_LOGE("MAIN", "LINHA %d", offset);

        
        // tx_data[0] = 0x91;  tx_data[1] = 0x41;  tx_data[2] = 0x3A;  spi_send_data(handle, tx_data, 24);
        // tx_data[0] = 0x92;  tx_data[1] = 0x01;  tx_data[2] = 0x48;  spi_send_data(handle, tx_data, 24);
        // spi_read_dma_bits(handle, (void *)rx_buffer, 12 * 320    );
    }
}



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
    httpd_resp_send(req, (char *)rx_psram_buffer, IMG_SIZE_BYTES);
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
            int exposure = atoi(param);

            ESP_LOGI("WEB", "Nova exposição: %d", exposure);
        }
    }

    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// --- Configuração e arranque do WebServer ---
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "A iniciar webserver...");

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

        ESP_LOGI(TAG, "Webserver iniciado na porta 80");
        ready = true;

        return server;
    }

    ESP_LOGE(TAG, "Erro ao iniciar webserver");
    return NULL;
}

// ----------- TASK DO CORE 1 -----------
void core1_webserver(void *arg)
{
    ESP_LOGI(TAG, "Core 1: a iniciar servidor web...");
    start_webserver();
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void) {

    ESP_LOGI(TAG, "Main running");

    rx_psram_buffer = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);
    if (!rx_psram_buffer) {
        ESP_LOGE("MAIN", "Falha ao alocar rx_psram_buffer (%d bytes)", IMG_SIZE_BYTES);
        return;
    }
    tx_psram_buffer = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);
    if (!rx_psram_buffer) {
        ESP_LOGE("MAIN", "Falha ao alocar tx_psram_buffer (%d bytes)", IMG_SIZE_BYTES);
        return;
    }

    wifi_init();
    xTaskCreatePinnedToCore(core0_spi_loop,  "core0_spi", 8192,(void *)rx_psram_buffer, 1, NULL, 0 );
    xTaskCreatePinnedToCore(core1_webserver, "core1_webserver", 8192,(void *)tx_psram_buffer, 1, NULL, 1 );
}

