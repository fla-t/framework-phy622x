#include "types.h" /* for UNUSED */
#include <driver/pwm/pwm.h>

#include <log/log.h>
#include "FreeRTOS.h"
#include "task.h"
#include "display/display.h"

#include "osal_nuker.h"

// LED
#define GPIO_LED GPIO_P00

void genericTask(void *argument)
{
    UNUSED(argument);
    LOG("Hi from genericTask");

    display_init(NV3022B_WIDTH, NV3022B_HEIGHT, 0);

    /* Calibration: 1px red border on black, hugging all four logical edges.
     * If every side sits flush with the panel, the GRAM offsets are right. */
    display_fill(NV3022B_BLACK);
    display_draw_border(NV3022B_RED);

    for (;;)
        vTaskDelay(pdMS_TO_TICKS(1000));
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
