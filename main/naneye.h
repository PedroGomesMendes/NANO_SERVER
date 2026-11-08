#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

/* ============================================================
 * NANEYE configuration
 * ============================================================ */

#define NANEYE_IMG_W       328
#define NANEYE_IMG_H       320

#define NANEYE_PIXEL_BITS  12


/* Register 0 */

#define ROWS_IN_RESET_MASK     0xFF
#define ROWS_IN_RESET_POS      8

#define VRST_PIX_MASK          0x03
#define VRST_PIX_POS           6

#define RAMP_GAIN_MASK         0x03
#define RAMP_GAIN_POS          4

#define OFFSET_RAMP_MASK       0x03
#define OFFSET_RAMP_POS        2

#define OUTPUT_CURR_MASK       0x03
#define OUTPUT_CURR_POS        0


/* Register 1 */

#define ROWS_DELAY_MASK        0x1F
#define ROWS_DELAY_POS         11

#define BIAS_CURR_MASK         0x01
#define BIAS_CURR_POS          10

#define CDS_GAIN_MASK          0x01
#define CDS_GAIN_POS           9

#define OUTPUT_MODE_MASK       0x01
#define OUTPUT_MODE_POS        8

#define MCLK_MODE_MASK         0x03
#define MCLK_MODE_POS          6

#define VREF_MASK              0x03
#define VREF_POS               4

#define CVC_CURR_MASK          0x03
#define CVC_CURR_POS           2

#define IDLE_MODE_MASK         0x01
#define IDLE_MODE_POS          1

#define HIGH_SPEED_MASK        0x01
#define HIGH_SPEED_POS         0

static uint8_t rx_data[492 * 5];

/* ============================================================
 * NANEYE configuration structure
 * ============================================================ */

typedef struct
{
    spi_host_device_t spi_host;

    gpio_num_t pin_mosi;
    gpio_num_t pin_miso;
    gpio_num_t pin_sclk;
    gpio_num_t pin_cs;

    int spi_clock_hz;

    size_t max_transfer_sz;

} naneye_config_t;

/* ============================================================
 * NANEYE handle
 * ============================================================ */

typedef struct
{
    spi_device_handle_t spi;

    uint32_t frame0;
    uint32_t frame1;
    int exposure;
    int gain;
    uint16_t reg0_val;
    uint16_t reg1_val;

    uint8_t *write_buffer;
    uint8_t *read_buffer;

    uint8_t update_code;

    bool initialized;
    bool sensor_reset_flag;

} naneye_t;

/* ============================================================
 * API
 * ============================================================ */

esp_err_t naneye_init(
    naneye_t *dev,
    const naneye_config_t *config,
    uint8_t *buffer0,
    uint8_t *buffer1);

esp_err_t naneye_deinit(naneye_t *dev);

esp_err_t naneye_start(naneye_t *dev);

esp_err_t naneye_update_registers(naneye_t *dev);

esp_err_t naneye_set_exposure(
    naneye_t *dev,
    uint8_t exposure
);

esp_err_t naneye_set_gain(
    naneye_t *dev,
    uint8_t gain
);

esp_err_t naneye_capture_frame(
    naneye_t *dev
);

/* ============================================================
 * Low level SPI
 * ============================================================ */

esp_err_t naneye_spi_send(
    naneye_t *dev,
    const uint8_t *data,
    size_t bits
);

esp_err_t naneye_spi_read(
    naneye_t *dev,
    void *dest_addr,
    size_t bits
);

esp_err_t naneye_spi_read_queue(
    naneye_t *dev,
    spi_transaction_t *t,
    void *dest_addr,
    size_t bits
);

/* ============================================================
 * Register builders
 * ============================================================ */

uint16_t naneye_build_reg0(
    uint8_t rows_in_reset,
    uint8_t vrst_pix,
    uint8_t ramp_gain,
    uint8_t offset_ramp,
    uint8_t output_curr
);

uint16_t naneye_build_reg1(
    uint8_t rows_delay,
    uint8_t bias_curr_increase,
    uint8_t cds_gain,
    uint8_t output_mode,
    uint8_t mclk_mode,
    uint8_t vref,
    uint8_t cvc_curr,
    uint8_t idle_mode,
    uint8_t high_speed
);