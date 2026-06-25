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

#define ROWS_IN_RESET_POS 8
#define VRST_PIX_POS 6
#define RAMP_GAIN_POS 4
#define OFFSET_RAMP_POS 2
#define OUTPUT_CURR_POS 0
// Bitpositionen_Reg1
#define ROWS_DELAY_POS 11
#define BIAS_CURR_POS 10
#define CDS_GAIN_POS 9
#define OUTPUT_MODE_POS 8
#define MCLK_MODE_POS 6
#define VREF_POS 4
#define CVC_CURR_POS 2
#define IDLE_MODE_POS 1
#define HIGH_SPEED_POS 0

// Bitmasken_Reg0
#define ROWS_IN_RESET_MASK 0xFF // 8 Bit
#define VRST_PIX_MASK 0x03		// 2 Bit
#define RAMP_GAIN_MASK 0x03		// 2 Bit
#define OFFSET_RAMP_MASK 0x03	// 2 Bit
#define OUTPUT_CURR_MASK 0x03	// 2 Bit
// Bitmasken_Reg1
#define ROWS_DELAY_MASK 0x1F  // 5 Bit
#define BIAS_CURR_MASK 0x01	  // 1 Bit
#define CDS_GAIN_MASK 0x01	  // 1 Bit
#define OUTPUT_MODE_MASK 0x01 // 1 Bit
#define MCLK_MODE_MASK 0x03	  // 2 Bit
#define VREF_MASK 0x03		  // 2 Bit
#define CVC_CURR_MASK 0x03	  // 2 Bit
#define IDLE_MODE_MASK 0x01	  // 1 Bit
#define HIGH_SPEED_MASK 0x01  // 1 Bit



#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK  13
#define PIN_NUM_CS   5
#define GPIO_TOGGLE  14

#define IMG_W 328
#define IMG_H 320

#define PIXEL_SIZE 2
#define IMG_SIZE_BYTES (IMG_W * IMG_H * 12/8)

volatile bool buffer_ready = false;

uint8_t *write_buffer = NULL;
uint8_t *read_buffer  = NULL;
uint8_t *buffer_a = NULL;
uint8_t *buffer_b = NULL;
uint8_t *packet = NULL;

#define BITS_PER_PIXEL 12
#define WORD_BITS      16

#define WORDS_PER_LINE_BITS (IMG_W * BITS_PER_PIXEL)
#define WORDS_PER_LINE ((WORDS_PER_LINE_BITS + WORD_BITS - 1) / WORD_BITS) // 246

#define SERVER_IP  "192.168.4.2"
#define SERVER_PORT 5001

#define  WEBSERVER  1
#define  UPD_SENDER 0
#define  NUM_TRANS 20
#define  LINES_PER_TRANS 16


static const char *TAG = "SPI_UDP";

volatile uint16_t *rx_buffer = NULL;
uint8_t rx_data[492*5] = {0};

uint16_t reg0_val;
uint16_t reg1_val; 
const uint32_t update_code = 0x9;

uint32_t frame0 = 0x3A2D91;                 //0x912D3A; 
uint32_t frame1 = 0xCA0092;                // 0x920148; 
uint32_t temp_frame0 = 0x3A2D91;           //0x912D3A; 
uint32_t temp_frame1 = 0xCA0092;           // 0x920148; 

spi_transaction_t t;
spi_transaction_t trans[NUM_TRANS];
spi_transaction_t *ret_trans;

int exposure = 150; // 
int gain = 1;       // 

// Function to build the value for register 0 based on the provided parameters
uint16_t build_reg0_value(uint8_t rows_in_reset,uint8_t vrst_pix,uint8_t ramp_gain,uint8_t offset_ramp,uint8_t output_curr)
{
	uint16_t value = 0;
	value |= ((rows_in_reset & ROWS_IN_RESET_MASK) << ROWS_IN_RESET_POS);
	value |= ((vrst_pix & VRST_PIX_MASK) << VRST_PIX_POS);
	value |= ((ramp_gain & RAMP_GAIN_MASK) << RAMP_GAIN_POS);
	value |= ((offset_ramp & OFFSET_RAMP_MASK) << OFFSET_RAMP_POS);
	value |= ((output_curr & OUTPUT_CURR_MASK) << OUTPUT_CURR_POS);
	return value;
}
// Function to build the value for register 1 based on the provided parameters
uint16_t build_reg1_value(uint8_t rows_delay,uint8_t bias_curr_increase,uint8_t cds_gain,uint8_t output_mode,uint8_t mclk_mode,uint8_t vref,uint8_t cvc_curr,uint8_t idle_mode,uint8_t high_speed)
{
	uint16_t value = 0;
	value |= ((rows_delay & ROWS_DELAY_MASK) << ROWS_DELAY_POS);
	value |= ((bias_curr_increase & BIAS_CURR_MASK) << BIAS_CURR_POS);
	value |= ((cds_gain & CDS_GAIN_MASK) << CDS_GAIN_POS);
	value |= ((output_mode & OUTPUT_MODE_MASK) << OUTPUT_MODE_POS);
	value |= ((mclk_mode & MCLK_MODE_MASK) << MCLK_MODE_POS);
	value |= ((vref & VREF_MASK) << VREF_POS);
	value |= ((cvc_curr & CVC_CURR_MASK) << CVC_CURR_POS);
	value |= ((idle_mode & IDLE_MODE_MASK) << IDLE_MODE_POS);
	value |= ((high_speed & HIGH_SPEED_MASK) << HIGH_SPEED_POS);
	return value;
}

// Function to turn on the LDO (Low Dropout Regulator)
void LDO_ON() 
{
    gpio_set_level(GPIO_TOGGLE, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(GPIO_TOGGLE, 1);
}

// Function to send data over SPI
esp_err_t spi_send_data(spi_device_handle_t handle, const uint8_t *data, size_t bits) {
    memset(&t, 0, sizeof(t));
    t.length = bits;
    t.tx_buffer = data;
    t.flags = 0;
    return spi_device_transmit(handle, &t);
}

// Function to read data over SPI using DMA
esp_err_t spi_read_dma_bits(spi_device_handle_t handle, void *dest_addr, size_t bits) {
    memset(&t, 0, sizeof(t));
    t.length = bits;
    t.rx_buffer = dest_addr;
    t.tx_buffer = NULL;
    t.flags = 0;
    return spi_device_transmit(handle, &t);
}

// Function to queue a SPI read transaction using DMA
esp_err_t spi_read_dma_bits_queue(spi_device_handle_t handle,spi_transaction_t *t,void *dest_addr,size_t bits)
{
    memset(t, 0, sizeof(*t));

    t->length    = bits;
    t->rxlength  = bits;
    t->rx_buffer = dest_addr;

    return spi_device_queue_trans(handle, t, portMAX_DELAY);
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
void core0_spi_loop(void *arg) {
    
    while(!buffer_ready);

    ESP_LOGI(TAG, "A init NANEYE...");

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MOSI ,
        .mosi_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 65536  // buffer completo
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 31*1000*1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 8
        };

    spi_device_handle_t handle;


    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &handle));
    uint8_t tx_data[3];

    esp_rom_delay_us(5000);

    tx_data[0] = 0x00;                                          
    spi_send_data(handle, tx_data, 1);
    spi_send_data(handle, (uint8_t*) &frame0, 24);
    spi_send_data(handle, (uint8_t*) &frame1, 24);

    esp_rom_delay_us(10);
    tx_data[0] = 0x00;  
    tx_data[1] = 0x00;  
    tx_data[2] = 0x00;  
    spi_send_data(handle, tx_data, (4*12)-2);  //READ IGNORE

    spi_send_data(handle, rx_data, 12 * (1312+329));
    int offset = 0;

    for (offset = 0; offset < IMG_H; offset += 16) { spi_read_dma_bits(handle,write_buffer + (offset * 492),IMG_W * 12 * 16);} //READ 1st image and IGNORE

    spi_read_dma_bits(handle, rx_data, 12 * 8);

    while (1) 
    {

        offset = 0;
        spi_send_data(handle, (uint8_t*) &frame0, 24);
        spi_send_data(handle, (uint8_t*) &frame1, 24);
        spi_send_data(handle, rx_data, 12 * 644);
        spi_send_data(handle, rx_data, 12 * 2 * 328);
        spi_send_data(handle, rx_data, 12 * 2 * 328);
        while (offset < IMG_H) 
        {
            spi_read_dma_bits(handle, write_buffer + (offset * 492), (IMG_W * 12 * 16));
            offset +=16;
        }
        uint8_t *tmp = write_buffer;
        write_buffer = read_buffer;
        read_buffer = tmp;
        spi_send_data(handle, rx_data, 12 * 8);
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

    //Function to handle HTTP requests for serving the image data
    static esp_err_t image_handler(httpd_req_t *req)
    {
        httpd_resp_set_type(req, "application/octet-stream");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");

        httpd_resp_send(req, (char *)read_buffer, IMG_SIZE_BYTES);

        return ESP_OK;
    }

    //Function to update the camera registers based on the current exposure and gain values
    int update_reg() {
        reg0_val = build_reg0_value(
        exposure &0xFF  ,// rows_in_reset
        0b10            ,// vrst_pix
        gain&0x03       ,// ramp_gain
        0b11,		        // offset_ramp
        0b11		        // output_curr
        );
                
        reg1_val = build_reg1_value(
        0b000000, // rows_delay, 00001
        0,		  // bias_curr_increase
        0,		  // cds_gain
        0,		  // output_mode
        0b10,	  // mclk_mode
        0b10,	  // vref
        0b01,	  // cvc_curr
        0,		  // idle_mode
        1		  // high_speed
        );

        temp_frame0 = ((update_code & 0xF) << 20) | ((0 & 0x7) << 17) | ((uint32_t)reg0_val << 1);
        temp_frame1 = ((update_code & 0xF) << 20) | ((1 & 0x7) << 17) | ((uint32_t)reg1_val << 1); 

        frame0 = ((temp_frame0 & 0x0000FF) << 16) |
        ( temp_frame0 & 0x00FF00) |
        ((temp_frame0 & 0xFF0000) >> 16);             

        frame1 = ((temp_frame1 & 0x0000FF) << 16) |
        ( temp_frame1 & 0x00FF00) |
        ((temp_frame1 & 0xFF0000) >> 16); 
        return 0;
    }

    //Gain handler function to handle HTTP requests for setting the gain value
    static esp_err_t gain_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
             gain = atoi(param);
                update_reg();
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    //Exposure handler function to handle HTTP requests for setting the exposure value
    static esp_err_t exposure_handler(httpd_req_t *req)
    {
        char buf[32];
        int buf_len = httpd_req_get_url_query_len(req) + 1;

        if (buf_len > sizeof(buf)) return ESP_FAIL;

        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[8];
            if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
                exposure = atoi(param);
                update_reg();
            }
        }

        httpd_resp_send(req, "OK", 2);
        return ESP_OK;
    }

    // Function to start the web server and register URI handlers
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

            ESP_LOGI(TAG, "Webserver iniciado na porta 80");
            buffer_ready = true;
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

    int udp_create_socket(struct sockaddr_in *dest_addr) {
        int sock;
        dest_addr->sin_family = AF_INET;
        dest_addr->sin_port = htons(SERVER_PORT);
        dest_addr->sin_addr.s_addr = inet_addr(SERVER_IP);

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Erro ao criar socket UDP");
            return -1;
        }

        ESP_LOGI(TAG, "Socket UDP criado para %s:%d", SERVER_IP, SERVER_PORT);
        return sock;
    }

    void core1_udp_loop(void *arg) {

        struct sockaddr_in dest_addr;
        int sock = udp_create_socket(&dest_addr);
        if (sock < 0) return;

        buffer_ready = true;


        int num_packets = 320;     
        int raw_bytes_per_line = 492; // 328 pixels × 12 bits / 8
        int i = 0;

        while (1) {
            while(i < num_packets){
                memcpy(packet, &read_buffer[i * raw_bytes_per_line], raw_bytes_per_line);
                packet[492]     = (uint8_t)(i & 0xFF);         // LSB
                packet[493]     = (uint8_t)((i >> 8) & 0xFF);  // MSB
                sendto(sock, packet, 494, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                i++;
            }
            i =0;
        }
    }

#endif


//Main application entry point
void app_main(void) {

    update_reg();
    gpio_set_direction(GPIO_TOGGLE, GPIO_MODE_OUTPUT);
    LDO_ON();

    buffer_a = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);
    buffer_b = heap_caps_malloc(IMG_SIZE_BYTES, MALLOC_CAP_SPIRAM);

    if (!buffer_a || !buffer_b) {
        ESP_LOGE("MAIN", "Failed to allocate buffers");
        return;
    }

    write_buffer  = buffer_a;
    read_buffer   = buffer_b;
    
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

