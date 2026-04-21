#include "task_printf.h"
#include "FreeRTOS.h"
#include "task.h"

extern uint32_t SystemCoreClock;

void Printf_Task(void *pvParameters)
{
    BSP_Printf_Init();
    BSP_Printf("System Clock is %d Hz\r\n", SystemCoreClock);
    while (1)
    {
        if( !BSP_Printf_SendData() )
            vTaskDelay(1);
    }
}
