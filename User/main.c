/**
  ******************************************************************************
  * @file    main.c
  * @brief   07 ch: light sensor DO read + UART print (register version)
  * @note    PB13 -> CRH[(13-8)*4 = 23:20] = 0x4 (floating input). IDR bit13.
  *          UART same as ch04 (BRR=0x271, CR1 two-step). Heartbeat PC13.
  ******************************************************************************
  */
#include "stm32f10x.h"

static uint8_t last = 0xFF;
static uint16_t n = 0;

void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;
    SysTick->LOAD = tick - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    SysTick->CTRL = 0;
}
static void uart1_send(uint8_t b)
{
    while ((USART1->SR & USART_SR_TXE) == 0)
    {
    }
    USART1->DR = b;
}
static void uart1_str(const char *s)
{
    while (*s)
        uart1_send((uint8_t)*s++);
}

int main(void)
{
    SystemCoreClockUpdate();

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* PB13 floating input: CRH[23:20] = 0b0100, keep others */
    GPIOB->CRH = (GPIOB->CRH & 0xFF0FFFFFU) | 0x00400000U;
    /* PC13 output: CRH[23:20]... wait, PC13 is GPIOC. PC13 -> CRH[23:20]=0x2 */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;
    GPIOC->BSRR = GPIO_BSRR_BS13;

    /* PA9/PA10 */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
    USART1->BRR = 0x271;
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

    uart1_str("\r\n=== CH07 LIGHT SENSOR (REG) READY ===\r\n");
    uart1_str("Prints on level change. '?'=query. PB13=DO\r\n");

    while (1)
    {
        uint8_t now = (GPIOB->IDR & GPIO_IDR_IDR13) ? 1 : 0;

        if (now != last)
        {
            last = now;
            uart1_str("SMOKE(CH07 LIGHT) DO=");
            uart1_send((uint8_t)('0' + now));
            uart1_str(now ? "  (normal/bright)\r\n" : "  (ALARM/dark!)\r\n");
        }

        if (USART1->SR & USART_SR_RXNE)
        {
            uint8_t b = (uint8_t)USART1->DR;
            if (b == '?')
            {
                uart1_str("[QUERY] DO=");
                uart1_send((uint8_t)('0' + ((GPIOB->IDR & GPIO_IDR_IDR13) ? 1 : 0)));
                uart1_str("\r\n");
            }
        }

        delay_ms(10);
        if (++n >= 50)
        {
            n = 0;
            GPIOC->ODR ^= GPIO_ODR_ODR13;
        }
    }
}
