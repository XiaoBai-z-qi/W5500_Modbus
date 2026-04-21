#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"

/* Private includes ----------------------------------------------------------*/
#include "task_printf.h"
#include "port_wizchip.h"
#include "task_modbus_tcp.h"
void default_Task(void *pvParameters);

void MX_FREERTOS_Init(void)
{
    /* Create the default task */
    xTaskCreate(Printf_Task, "Printf_Task", 256, NULL, 2, NULL);
    //xTaskCreate(default_Task, "default_Task", 512, NULL, 1, NULL);
	xTaskCreate(Modbus_TCP_Task, "Modbus", 512, NULL, 1, NULL);
	
}

void default_Task(void *pvParameters)
{
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    W5500_Init();
    while (1)
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        vTaskDelay(1000);
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        vTaskDelay(1000);
    }
}
