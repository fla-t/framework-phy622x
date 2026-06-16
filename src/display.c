#include "display.h"
#include <driver/spi/spi.h>
#include <driver/clock/clock.h> /* for WaitMs */


#define DISPLAY_SPI         SPI0
#define DISPLAY_SPI_TIMEOUT 1000 /* ms; upper bound for a blocking transfer */

/* Solid fills stream in horizontal strips of this many rows, so a full-screen
 * fill needs only NV3022B_WIDTH*DISPLAY_STRIP_ROWS px instead of a whole frame. */
#define DISPLAY_STRIP_ROWS  16
static uint16_t s_fill_strip[NV3022B_WIDTH * DISPLAY_STRIP_ROWS];

static uint8_t _width = 0;
static uint8_t _height = 0;
static uint8_t rotation = 0;


static inline void NV3022B_Command(uint8_t cmd) {
    hal_gpio_write(DC_PIN, 0);
    hal_spi_transmit(DISPLAY_SPI, &cmd, 1, DISPLAY_SPI_TIMEOUT);
}

static inline void NV3022B_Data(uint8_t *buff, uint8_t buff_size) {
    hal_gpio_write(DC_PIN, 1);
    hal_spi_transmit(DISPLAY_SPI, buff, buff_size, DISPLAY_SPI_TIMEOUT);
}

static inline void NV3022B_WriteCommand(uint8_t cmd) {
    NV3022B_Command(cmd);
}

static inline void NV3022B_WriteData(uint8_t data) {
    NV3022B_Data(&data, 1);
}

static inline void NV3022B_WriteDataMultiple(uint8_t *data, uint8_t size) {
    NV3022B_Data(data, size);
}

static void NV3022B_WriteCmd(uint8_t cmd, const uint8_t *data, uint8_t len) {
    NV3022B_WriteCommand(cmd);
    if (len > 0) NV3022B_WriteDataMultiple((uint8_t *)data, len);
}

static void NV3022B_Init(void) {
    // Sleep out, wait for display to wake
    NV3022B_WriteCommand(NV3022B_SLPOUT);
    WaitMs(120);

    // MADCTL: BGR color order
    NV3022B_WriteCommand(NV3022B_MADCTL);
    NV3022B_WriteData(NV3022B_MADCTL_BGR);

    // COLMOD: 16-bit RGB565
    NV3022B_WriteCommand(NV3022B_COLMOD);
    NV3022B_WriteData(0x55);

    // Display inversion on (IPS panel)
    NV3022B_WriteCommand(NV3022B_INVON);

    // Set full display address window
    // x: col_offset .. col_offset+79, y: row_offset .. row_offset+159
    NV3022B_WriteCmd(NV3022B_CASET, (uint8_t[]){0x00, NV3022B_COL_OFFSET, 0x00, NV3022B_COL_OFFSET + NV3022B_WIDTH - 1}, 4);
    NV3022B_WriteCmd(NV3022B_RASET, (uint8_t[]){0x00, NV3022B_ROW_OFFSET, 0x00, NV3022B_ROW_OFFSET + NV3022B_HEIGHT - 1}, 4);

    // Display on
    NV3022B_WriteCommand(NV3022B_DISPON);
    WaitMs(10);

    // Begin memory write
    NV3022B_WriteCommand(NV3022B_RAMWR);
}

void display_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    uint8_t data[4];

    NV3022B_WriteCommand(NV3022B_CASET);
    data[0] = 0x00;
    data[1] = x0 + NV3022B_COL_OFFSET;
    data[2] = 0x00;
    data[3] = x1 + NV3022B_COL_OFFSET;
    NV3022B_WriteDataMultiple(data, 4);

    NV3022B_WriteCommand(NV3022B_RASET);
    data[0] = 0x00;
    data[1] = y0 + NV3022B_ROW_OFFSET;
    data[2] = 0x00;
    data[3] = y1 + NV3022B_ROW_OFFSET;
    NV3022B_WriteDataMultiple(data, 4);

    NV3022B_WriteCommand(NV3022B_RAMWR);
}

void display_blit(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint16_t *buf, uint32_t num_pixels) {
    display_set_addr_window(x0, y0, x1, y1);
    hal_gpio_write(DC_PIN, 1);
    hal_spi_transmit(DISPLAY_SPI, (uint8_t *)buf, num_pixels * 2, DISPLAY_SPI_TIMEOUT);
}

void display_fill(uint16_t color) {
    uint16_t swapped = (color >> 8) | (color << 8);
    for (uint16_t i = 0; i < NV3022B_WIDTH * DISPLAY_STRIP_ROWS; i++)
        s_fill_strip[i] = swapped;

    for (uint8_t y = 0; y < NV3022B_HEIGHT; y += DISPLAY_STRIP_ROWS) {
        uint8_t rows = (NV3022B_HEIGHT - y < DISPLAY_STRIP_ROWS)
                     ? (NV3022B_HEIGHT - y) : DISPLAY_STRIP_ROWS;
        display_blit(0, y, NV3022B_WIDTH - 1, y + rows - 1,
                     s_fill_strip, (uint32_t)NV3022B_WIDTH * rows);
    }
}

/* 1px outline hugging all four logical edges. Handy as an alignment check:
 * if every side sits flush with the panel, the GRAM offsets are right. */
void display_draw_border(uint16_t color) {
    uint16_t swapped = (color >> 8) | (color << 8);
    /* fill enough of the scratch strip for the longest edge (the height) */
    for (uint16_t i = 0; i < NV3022B_HEIGHT; i++)
        s_fill_strip[i] = swapped;

    display_blit(0, 0, NV3022B_WIDTH - 1, 0, s_fill_strip, NV3022B_WIDTH);                                  /* top    */
    display_blit(0, NV3022B_HEIGHT - 1, NV3022B_WIDTH - 1, NV3022B_HEIGHT - 1, s_fill_strip, NV3022B_WIDTH); /* bottom */
    display_blit(0, 0, 0, NV3022B_HEIGHT - 1, s_fill_strip, NV3022B_HEIGHT);                                 /* left   */
    display_blit(NV3022B_WIDTH - 1, 0, NV3022B_WIDTH - 1, NV3022B_HEIGHT - 1, s_fill_strip, NV3022B_HEIGHT); /* right  */
}

void display_set_rotation(uint8_t m) {
    m %= 4;
    rotation = m;
    uint8_t madctl = 0;

    switch (m) {
        case 0:
            madctl = NV3022B_MADCTL_BGR;
            break;
        case 1:
            madctl = NV3022B_MADCTL_MV | NV3022B_MADCTL_BGR;
            break;
        case 2:
            madctl = NV3022B_MADCTL_MY | NV3022B_MADCTL_MX | NV3022B_MADCTL_BGR;
            break;
        case 3:
            madctl = NV3022B_MADCTL_MV | NV3022B_MADCTL_MY | NV3022B_MADCTL_BGR;
            break;
    }

    NV3022B_WriteCommand(NV3022B_MADCTL);
    NV3022B_WriteData(madctl);
}


uint16_t display_get_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((b & 0xF8) << 8)
         | ((r & 0xF8) << 3)
         | ((g & 0xFC) >> 2);
}

void display_draw_pixel(uint8_t x, uint8_t y, uint16_t color) {
    if (x >= _width || y >= _height)
        return;
    uint16_t s = (color >> 8) | (color << 8);
    display_blit(x, y, x, y, &s, 1);
}

void display_init(uint8_t width, uint8_t height, uint8_t _rotation) {
    _width = width;
    _height = height;
    rotation = _rotation;

    hal_gpio_pin_init(DC_PIN, GPIO_OUTPUT);
    hal_gpio_pin_init(RST_PIN, GPIO_OUTPUT);
    hal_gpio_pin_init(CS_PIN, GPIO_OUTPUT);

    // Hardware reset: low 10ms, high, wait 120ms
    hal_gpio_write(RST_PIN, 0);
    WaitMs(10);
    hal_gpio_write(RST_PIN, 1);
    WaitMs(120);

    spi_Cfg_t spi_cfg = {
        .sclk_pin    = SCLK_PIN,
        .ssn_pin     = CS_PIN,
        .MOSI        = MOSI_PIN,
        .MISO        = 0,
        .frequency   = 15000000,
        .spi_scmod   = SPI_MODE0,
        .spi_dfsmod  = SPI_8BIT,
        .force_cs    = SPI_FORCE_CS_DISABLED,
        .evt_handler = NULL,
    };

    hal_spi_bus_init(DISPLAY_SPI, spi_cfg);

    NV3022B_Init();

    display_set_rotation(rotation);

    hal_gpio_pin_init(BKL_PIN, GPIO_OUTPUT);
    backlight_turn_on();
}

void backlight_turn_off() {
    hal_gpio_write(BKL_PIN, 1);
}

void backlight_turn_on() {
    hal_gpio_write(BKL_PIN, 0);
}
