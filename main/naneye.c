#include "naneye.h"

#include <string.h>

#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "NANEYE";

/* ============================================================
 * Register 0
 * ============================================================ */

uint16_t naneye_build_reg0(
    uint8_t rows_in_reset,
    uint8_t vrst_pix,
    uint8_t ramp_gain,
    uint8_t offset_ramp,
    uint8_t output_curr)
{
    uint16_t value = 0;

    value |= ((rows_in_reset & ROWS_IN_RESET_MASK)
              << ROWS_IN_RESET_POS);

    value |= ((vrst_pix & VRST_PIX_MASK)
              << VRST_PIX_POS);

    value |= ((ramp_gain & RAMP_GAIN_MASK)
              << RAMP_GAIN_POS);

    value |= ((offset_ramp & OFFSET_RAMP_MASK)
              << OFFSET_RAMP_POS);

    value |= ((output_curr & OUTPUT_CURR_MASK)
              << OUTPUT_CURR_POS);

    return value;
}


/* ============================================================
 * Register 1
 * ============================================================ */

uint16_t naneye_build_reg1(
    uint8_t rows_delay,
    uint8_t bias_curr_increase,
    uint8_t cds_gain,
    uint8_t output_mode,
    uint8_t mclk_mode,
    uint8_t vref,
    uint8_t cvc_curr,
    uint8_t idle_mode,
    uint8_t high_speed)
{
    uint16_t value = 0;

    value |= ((rows_delay & ROWS_DELAY_MASK)
              << ROWS_DELAY_POS);

    value |= ((bias_curr_increase & BIAS_CURR_MASK)
              << BIAS_CURR_POS);

    value |= ((cds_gain & CDS_GAIN_MASK)
              << CDS_GAIN_POS);

    value |= ((output_mode & OUTPUT_MODE_MASK)
              << OUTPUT_MODE_POS);

    value |= ((mclk_mode & MCLK_MODE_MASK)
              << MCLK_MODE_POS);

    value |= ((vref & VREF_MASK)
              << VREF_POS);

    value |= ((cvc_curr & CVC_CURR_MASK)
              << CVC_CURR_POS);

    value |= ((idle_mode & IDLE_MODE_MASK)
              << IDLE_MODE_POS);

    value |= ((high_speed & HIGH_SPEED_MASK)
              << HIGH_SPEED_POS);

    return value;
}


/* ============================================================
 * Build SPI frames
 * ============================================================ */

esp_err_t naneye_update_registers(naneye_t *dev)
{
    if (!dev)
        return ESP_ERR_INVALID_ARG;

    /*
     * Register 0
     */

    dev->reg0_val = naneye_build_reg0(
        dev->exposure & 0xFF,    // rows_in_reset
        0b10,                    // vrst_pix
        dev->gain & 0x03,        // ramp_gain
        0b11,                    // offset_ramp
        0b11                     // output_curr
    );


    /*
     * Register 1
     */

    dev->reg1_val = naneye_build_reg1(
        0b000000,                // rows_delay
        0,                       // bias_curr_increase
        0,                       // cds_gain
        0,                       // output_mode
        0b10,                    // mclk_mode
        0b10,                    // vref
        0b01,                    // cvc_curr
        0,                       // idle_mode
        1                        // high_speed
    );


    /*
     * Build temporary frames
     */

    uint32_t temp_frame0 =
        ((dev->update_code & 0xF) << 20) |
        ((0 & 0x7) << 17) |
        ((uint32_t)dev->reg0_val << 1);

    uint32_t temp_frame1 =
        ((dev->update_code & 0xF) << 20) |
        ((1 & 0x7) << 17) |
        ((uint32_t)dev->reg1_val << 1);


    /*
     * Keep original byte order
     */

    dev->frame0 = temp_frame0 & 0xFFFFFF;
    dev->frame1 = temp_frame1 & 0xFFFFFF;

    return ESP_OK;
}


/* ============================================================
 * SPI send
 * ============================================================ */

esp_err_t naneye_spi_send(
    naneye_t *dev,
    const uint8_t *data,
    size_t bits)
{
    if (!dev || !data)
        return ESP_ERR_INVALID_ARG;

    spi_transaction_t t;

    memset(&t, 0, sizeof(t));

    t.length = bits;
    t.tx_buffer = data;

    return spi_device_transmit(dev->spi, &t);
}


/* ============================================================
 * SPI read
 * ============================================================ */

esp_err_t naneye_spi_read(
    naneye_t *dev,
    void *dest_addr,
    size_t bits)
{
    if (!dev || !dest_addr)
        return ESP_ERR_INVALID_ARG;

    spi_transaction_t t;

    memset(&t, 0, sizeof(t));

    t.length = bits;
    t.rxlength = bits;
    t.rx_buffer = dest_addr;

    return spi_device_transmit(dev->spi, &t);
}


/* ============================================================
 * SPI queued DMA read
 * ============================================================ */

esp_err_t naneye_spi_read_queue(
    naneye_t *dev,
    spi_transaction_t *t,
    void *dest_addr,
    size_t bits)
{
    if (!dev || !t || !dest_addr)
        return ESP_ERR_INVALID_ARG;

    memset(t, 0, sizeof(*t));

    t->length   = bits;
    t->rxlength = bits;
    t->rx_buffer = dest_addr;

    return spi_device_queue_trans(
        dev->spi,
        t,
        portMAX_DELAY
    );
}


/* ============================================================
 * Set exposure
 * ============================================================ */

esp_err_t naneye_set_exposure(
    naneye_t *dev,
    uint8_t exposure)
{
    if (!dev)
        return ESP_ERR_INVALID_ARG;

    dev->exposure = exposure;

    return naneye_update_registers(dev);
}


/* ============================================================
 * Set gain
 * ============================================================ */

esp_err_t naneye_set_gain(
    naneye_t *dev,
    uint8_t gain)
{
    if (!dev)
        return ESP_ERR_INVALID_ARG;

    dev->gain = gain & 0x03;

    return naneye_update_registers(dev);
}


/* ============================================================
 * NANEYE initialization
 * ============================================================ */

esp_err_t naneye_init(
    naneye_t *dev,
    const naneye_config_t *config,
    uint8_t *buffer0,
    uint8_t *buffer1)
{
    if (!dev || !config)
        return ESP_ERR_INVALID_ARG;

    memset(dev, 0, sizeof(*dev));
    memset(rx_data, 0, sizeof(rx_data));

    dev->write_buffer = buffer0;
    dev->read_buffer  = buffer1;

    dev->exposure = 1;
    dev->gain = 2;

    dev->update_code = 0x9;

    spi_bus_config_t buscfg = {
        .miso_io_num = config->pin_miso,
        .mosi_io_num = config->pin_mosi,
        .sclk_io_num = config->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->max_transfer_sz
    };


    esp_err_t ret = spi_bus_initialize(
        config->spi_host,
        &buscfg,
        SPI_DMA_CH_AUTO
    );

    esp_rom_delay_us(5000);

    if (ret != ESP_OK)
        return ret;


    /*
     * SPI device
     */

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->spi_clock_hz,
        .mode = 0,
        .spics_io_num = config->pin_cs,
        .queue_size = 8
    };


    ret = spi_bus_add_device(
        config->spi_host,
        &devcfg,
        &dev->spi
    );

    if (ret != ESP_OK)
    {
        spi_bus_free(config->spi_host);
        return ret;
    }



    gpio_set_drive_capability(
        config->pin_mosi,
        GPIO_DRIVE_CAP_0
    );



    //ret = naneye_update_registers(dev);


    dev->frame0 = 0x3A2D91;                 //0x912D3A; 
    dev->frame1 = 0xCA0092;                // 0x920148; 
    dev->initialized = true;


    return ESP_OK;
}



esp_err_t naneye_start(naneye_t *dev)
{
    if (!dev || !dev->initialized)
    return ESP_ERR_INVALID_STATE;

    uint8_t tx_data[3];

    tx_data[0] = 0x00;

    naneye_spi_send(dev, tx_data, 1);
    naneye_spi_send(dev, (uint8_t *)&dev->frame0, 24);
    naneye_spi_send(dev, (uint8_t *)&dev->frame1, 24);

    esp_rom_delay_us(10);

    tx_data[0] = 0x00;
    tx_data[1] = 0x00;
    tx_data[2] = 0x00;

    naneye_spi_send(dev, rx_data, (4 * 12) - 2);
    naneye_spi_send(dev, rx_data, 12 * (1312 + 329));

    for (int offset = 0; offset < NANEYE_IMG_H; offset += 16) {
        naneye_spi_read(dev, dev->write_buffer + (offset * 492), NANEYE_IMG_W * 12 * 16);
    }

    naneye_spi_read(dev, rx_data, 12 * 8);

    return ESP_OK;
}


esp_err_t naneye_capture_frame(naneye_t *dev)
{
    int offset = 0;

    naneye_spi_send(dev, (uint8_t *)&dev->frame0, 24);
    naneye_spi_send(dev, (uint8_t *)&dev->frame1, 24);

    naneye_spi_send(dev, rx_data, 12 * 644);
    naneye_spi_send(dev, rx_data, 12 * 2 * 328);
    naneye_spi_send(dev, rx_data, 12 * 2 * 328);

    while (offset < NANEYE_IMG_H) {
        naneye_spi_read(dev, dev->write_buffer + (offset * 492), NANEYE_IMG_W * 12 * 16);
        offset += 16;
    }

    uint8_t *tmp = dev->write_buffer;
    dev->write_buffer = dev->read_buffer;
    dev->read_buffer = tmp;

    naneye_spi_send(dev, rx_data, 12 * 8);

    return ESP_OK;
}

esp_err_t naneye_deinit(naneye_t *dev)
{
    if (!dev)
        return ESP_ERR_INVALID_ARG;

    if (!dev->initialized)
        return ESP_OK;

    if (dev->spi) {
        esp_err_t ret = spi_bus_remove_device(dev->spi);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_remove_device failed: %s",
                     esp_err_to_name(ret));
            return ret;
        }

        dev->spi = NULL;
    }

    esp_err_t ret = spi_bus_free(SPI3_HOST);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_free failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    dev->initialized = false;

    return ESP_OK;
}

