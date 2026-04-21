#include "bsp_system.h"
#include "FreeRTOS.h"
extern uint32_t SystemCoreClock;

//#define SYSCLK_FREQ_25MHz_HSE  25000000
//#define SYSCLK_FREQ_50MHz_HSE  50000000
#define SYSCLK_FREQ_84MHz_HSE  84000000
//#define SYSCLK_FREQ_100MHz_HSE  100000000

#ifdef SYSCLK_FREQ_25MHz_HSE
    #define PLL_M   25
    #define PLL_N   200
    #define PLL_P   8
    #define PLL_Q   4
#elif  SYSCLK_FREQ_50MHz_HSE
    #define PLL_M   25
    #define PLL_N   200
    #define PLL_P   4
    #define PLL_Q   4
#elif  SYSCLK_FREQ_84MHz_HSE
    #define PLL_M   25
    #define PLL_N   168
    #define PLL_P   2
    #define PLL_Q   4
#elif  SYSCLK_FREQ_100MHz_HSE
    #define PLL_M   25
    #define PLL_N   200
    #define PLL_P   2
    #define PLL_Q   4
#else
    #error "SYSCLK_FREQ_xMHz_HSE not defined"
#endif

void BSP_SystemClockInit(void)
{
    /* open HSE */
    RCC->CR |= RCC_CR_HSEON;
    while(!(RCC->CR & RCC_CR_HSERDY));

    /* Flash Config */
#if SYSCLK_FREQ_25MHz_HSE
    FLASH->ACR = FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_1WS;
#elif SYSCLK_FREQ_50MHz_HSE
    FLASH->ACR = FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_1WS;
#elif SYSCLK_FREQ_84MHz_HSE
    FLASH->ACR = FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_2WS;
#elif SYSCLK_FREQ_100MHz_HSE
    FLASH->ACR = FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_3WS;
#endif

    /* 总线分频 */
    RCC->CFGR = 0;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
#if SYSCLK_FREQ_25MHz_HSE
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;
#elif SYSCLK_FREQ_50MHz_HSE
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;
#elif SYSCLK_FREQ_84MHz_HSE
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
#elif SYSCLK_FREQ_100MHz_HSE
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;
#endif
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

    /* 关闭PLL */
    RCC->CR &= ~(RCC_CR_PLLON);
    while(RCC->CR & RCC_CR_PLLRDY);

    /* 配置PLL */
    RCC->PLLCFGR = (PLL_M) |
    (PLL_N << 6) |
    (((PLL_P >> 1) - 1) << 16) |
    RCC_PLLCFGR_PLLSRC_HSE |
    (PLL_Q << 24);

    /* 开启PLL */
    RCC->CR |= RCC_CR_PLLON;
    while(!(RCC->CR & RCC_CR_PLLRDY));

    /* 选择PLL作为系统时钟源 */
    RCC->CFGR &= ~(RCC_CFGR_SW);
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);


#if SYSCLK_FREQ_25MHz_HSE
    SystemCoreClock = SYSCLK_FREQ_25MHz_HSE;
#elif SYSCLK_FREQ_50MHz_HSE
    SystemCoreClock = SYSCLK_FREQ_50MHz_HSE;
#elif SYSCLK_FREQ_84MHz_HSE
    SystemCoreClock = SYSCLK_FREQ_84MHz_HSE;
#elif SYSCLK_FREQ_100MHz_HSE
    SystemCoreClock = SYSCLK_FREQ_100MHz_HSE;
#endif
}

void BSP_SystickClockInit(void)
{
    SysTick_Config(SystemCoreClock / configTICK_RATE_HZ);
}
