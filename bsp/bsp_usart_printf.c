#include "bsp_usart_printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    uint8_t  buf[USART_TX_BUF_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint8_t  busy;
}CircleBuff_t;

static uint16_t dma_len = 0;
static CircleBuff_t xCircleBuff;
/* Private includes ----------------------------------------------------------*/
static void bsp_usart1_init(void);
static void bsp_usart1_init_dma(void);
static uint8_t bsp_usart1_transmit_dma(uint8_t *data, uint16_t len);
static uint16_t buf_used(void);
static uint16_t buf_free(void);
static void buf_write(const uint8_t *data, uint16_t len);


/* ======================================== USART Port ======================================== */
static void bsp_usart1_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9,  GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

static void bsp_usart1_init_dma(void)
{
    bsp_usart1_init();
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
    DMA_DeInit(DMA2_Stream7);
    while(DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);

    DMA_InitStructure.DMA_Channel = DMA_Channel_4;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStructure.DMA_Memory0BaseAddr = 0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream7, &DMA_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Stream7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DMA_ITConfig(DMA2_Stream7, DMA_IT_TC, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA2_Stream7, DISABLE);
}

static uint8_t bsp_usart1_transmit_dma(uint8_t *data, uint16_t len)
{
    DMA_Cmd(DMA2_Stream7, DISABLE);
    while (DMA_GetCmdStatus(DMA2_Stream7) != DISABLE);
    DMA_ClearFlag(DMA2_Stream7, DMA_FLAG_TCIF7);
    DMA2_Stream7->M0AR = (uint32_t)data;
    DMA2_Stream7->NDTR = len;
    DMA_Cmd(DMA2_Stream7, ENABLE);
    return 1;
}

/* ======================================== 环形缓冲区 ======================================== */
static uint16_t buf_used(void)
{
    return (xCircleBuff.head - xCircleBuff.tail) & (USART_TX_BUF_SIZE - 1);
}

static uint16_t buf_free(void)
{
    return USART_TX_BUF_SIZE - 1 - buf_used();
}

/* 写入环形缓冲区 */
static void buf_write(const uint8_t *data, uint16_t len)
{
    uint16_t free_space = buf_free();

    if (len > free_space) {
        return;
    }

    for (uint16_t i = 0; i < len; i++) {
        xCircleBuff.buf[(xCircleBuff.head + i) & (USART_TX_BUF_SIZE - 1)] = data[i];
    }

    xCircleBuff.head = (xCircleBuff.head + len) & (USART_TX_BUF_SIZE - 1);
}

/* ======================================== 内部函数 ======================================== */
void BSP_Printf_Init(void)
{
    bsp_usart1_init_dma();
    xCircleBuff.head = 0;
    xCircleBuff.tail = 0;
    xCircleBuff.busy = 0;
}

uint8_t BSP_Printf_SendData(void)
{
    if (xCircleBuff.busy || buf_used() == 0)
        return 0;

    uint16_t tail = xCircleBuff.tail;
    uint16_t len;

    if (xCircleBuff.head >= tail) {
        len = xCircleBuff.head - tail;
    } else {
        len = USART_TX_BUF_SIZE - tail;
    }

    if (len > USART_DMA_MAX_LEN) {
        len = USART_DMA_MAX_LEN;
    }

    xCircleBuff.busy = 1;
    dma_len = len;

    bsp_usart1_transmit_dma(&xCircleBuff.buf[tail], len);
    return 1;
}

int BSP_Printf(const char *fmt, ...)
{
    char tmp[128];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    if (n <= 0) return 0;
    if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;

    taskENTER_CRITICAL();
    buf_write((uint8_t *)tmp, n);
    taskEXIT_CRITICAL();

    return n;
}

void bsp_usart1_IRQHandler(void)
{
    if(DMA_GetITStatus(DMA2_Stream7, DMA_IT_TCIF7) != RESET)
    {
        DMA_ClearITPendingBit(DMA2_Stream7, DMA_IT_TCIF7);
        xCircleBuff.tail = (xCircleBuff.tail + dma_len) & (USART_TX_BUF_SIZE - 1);
        xCircleBuff.busy = 0;
    }
}
