#ifndef DISPLAY_H
#define DISPLAY_H

#include <log/log.h>
#include <driver/gpio/gpio.h>
#include "config.h"

#define BUFFER_SIZE (NV3022B_WIDTH * NV3022B_HEIGHT)

// NV3022B command definitions
#define NV3022B_SWRESET 0x01
#define NV3022B_SLPOUT  0x11
#define NV3022B_INVOFF  0x20
#define NV3022B_INVON   0x21
#define NV3022B_DISPON  0x29
#define NV3022B_CASET   0x2A
#define NV3022B_RASET   0x2B
#define NV3022B_RAMWR   0x2C
#define NV3022B_MADCTL  0x36
#define NV3022B_COLMOD  0x3A

// MADCTL bits
#define NV3022B_MADCTL_MX  0x40
#define NV3022B_MADCTL_MY  0x80
#define NV3022B_MADCTL_MV  0x20
#define NV3022B_MADCTL_BGR 0x08

// Display dimensions
#define NV3022B_WIDTH  80
#define NV3022B_HEIGHT 160

// Column/row offsets: the 80x160 glass sits on a sub-window of the controller's
// 132x162 GRAM. These position that window. The panel isn't centered in GRAM, it
// sits at the low-address corner, so these are < the centered (26,1) guess.
// If an edge isn't flush, nudge the matching offset by ±1 (see display.c notes).
#define NV3022B_COL_OFFSET 24
#define NV3022B_ROW_OFFSET 0

// Colors (RGB565, BGR-swapped for hardware)
#define NV3022B_BLACK  0x0000
#define NV3022B_WHITE  0xFFFF
#define NV3022B_RED    0x001F
#define NV3022B_GREEN  0x07E0
#define NV3022B_BLUE   0xF800

void display_init(uint8_t width, uint8_t height, uint8_t rotation);
void display_blit(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint16_t *buf, uint32_t num_pixels);
void display_fill(uint16_t color);
void display_draw_border(uint16_t color);
void display_set_rotation(uint8_t m);
uint16_t display_get_color(uint8_t r, uint8_t g, uint8_t b);
void display_draw_pixel(uint8_t x, uint8_t y, uint16_t color);
void backlight_turn_off(void);
void backlight_turn_on(void);

#endif // DISPLAY_H
