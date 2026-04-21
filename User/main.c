#include "stm32f4xx.h"                  // Device header
#include "bsp_system.h"
#include "FreeRTOS.h"
#include "task.h"
extern void MX_FREERTOS_Init(void);
void NVIC_PriorityGroup_Init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
}

int main()
{
	BSP_SystemClockInit();
	BSP_SystickClockInit();
	
	NVIC_PriorityGroup_Init();
	MX_FREERTOS_Init();
    vTaskStartScheduler();
	while(1)
	{
		
	}
}

void led(void)
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}
