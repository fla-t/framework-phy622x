#include "types.h"
#include <driver/pwm/pwm.h>
#include <driver/spi/spi.h>
#include <driver/gpio/gpio.h>
#include <driver/clock/clock.h>
#include <log/log.h>

#include "FreeRTOS.h"
#include "task.h"
#include "display.h"
#include "config.h"

#include "osal_nuker.h"

#define DISPLAY_SPI SPI0
#define DISPLAY_SPI_TIMEOUT 1000

// LED
#define GPIO_LED GPIO_P00

static void port_spi_tx(uint8_t *buffer, uint8_t buffer_size)
{
	hal_spi_transmit(DISPLAY_SPI, buffer, buffer_size, DISPLAY_SPI_TIMEOUT);
}

static void port_set_cd(bool state)
{
	hal_gpio_write(DC_PIN, state);
}

void display_backlight_on(void)
{
	hal_gpio_write(BKL_PIN, 0);
}
void display_backlight_off(void)
{
	hal_gpio_write(BKL_PIN, 1);
}

void display_port_init(void)
{
	// GPIO directions
	hal_gpio_pin_init(DC_PIN, GPIO_OUTPUT);
	hal_gpio_pin_init(RST_PIN, GPIO_OUTPUT);
	hal_gpio_pin_init(CS_PIN, GPIO_OUTPUT);
	hal_gpio_pin_init(BKL_PIN, GPIO_OUTPUT);

	// Hardware reset: low 10ms, release, settle 120ms
	hal_gpio_write(RST_PIN, 0);
	WaitMs(10);
	hal_gpio_write(RST_PIN, 1);
	WaitMs(120);

	// SPI bus: 15 MHz, mode 0, 8-bit, peripheral-driven chip select
	spi_Cfg_t spi_cfg = {
		.sclk_pin = SCLK_PIN,
		.ssn_pin = CS_PIN,
		.MOSI = MOSI_PIN,
		.MISO = 0,
		.frequency = 15000000,
		.spi_scmod = SPI_MODE0,
		.spi_dfsmod = SPI_8BIT,
		.force_cs = SPI_FORCE_CS_DISABLED,
		.evt_handler = NULL,
	};
	hal_spi_bus_init(DISPLAY_SPI, spi_cfg);

	// Hand the driver its comms, then let it run the controller init
	display_set_spi_tx(port_spi_tx);
	display_set_set_cd(port_set_cd);
	display_init();

	display_backlight_on();
}

void genericTask(void *argument)
{
	UNUSED(argument);
	LOG("Hi from genericTask");

	display_port_init();

	/* Calibration: 1px red border on black, hugging all four logical edges.
     * If every side sits flush with the panel, the GRAM offsets are right.
     * display_fill/draw_border aren't implemented in the driver yet, so build
     * both out of display_draw_pixel (the one drawing primitive that is). */

	/* clear to black */
	for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++)
		for (uint8_t x = 0; x < DISPLAY_WIDTH; x++)
			display_draw_pixel(x, y, NV3022B_BLACK);

	/* 1px red border */
	for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
		display_draw_pixel(x, 0, NV3022B_RED);
		display_draw_pixel(x, DISPLAY_HEIGHT - 1, NV3022B_RED);
	}
	for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
		display_draw_pixel(0, y, NV3022B_RED);
		display_draw_pixel(DISPLAY_WIDTH - 1, y, NV3022B_RED);
	}

	/* Static image: it lives in the panel's own GRAM, so once it's drawn this
     * task has nothing left to do. Delete it to hand its stack back to the heap
     * (a task must never just return — that would trap). */
	vTaskDelete(NULL);
}

int main(void)
{
	/* init stuff as if OSAL was in charge */
	osal_nuker_init(SYS_CLK_DLL_96M, CLK_32K_RCOSC);

	LOG("g_hclk %d", g_hclk);

	xTaskCreate(genericTask, "genericTask", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

	LOG("starting FreeRTOS scheduler");

	vTaskStartScheduler();

	return 0;
}
