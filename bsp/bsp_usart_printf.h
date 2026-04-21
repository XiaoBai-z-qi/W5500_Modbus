#ifndef __BSP_USART_PRINTF_H__
#define __BSP_USART_PRINTF_H__
#include "stm32f4xx.h"

#define USART_TX_BUF_SIZE 1024
#define USART_DMA_MAX_LEN 256


void BSP_Printf_Init(void);
int BSP_Printf(const char *fmt, ...);
uint8_t BSP_Printf_SendData(void);

#endif
