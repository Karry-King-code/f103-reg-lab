/**
  ******************************************************************************
  * @file    main.c
  * @brief   04 章：串口收发（寄存器版）——亲手写 BRR/CR1，看每个位落位
  * @note    USART1 = PA9(TX)/PA10(RX)，115200-8-N-1。
  *          APB2=72MHz（HSE 8M×9，SystemInit 配好）：BRR=0x271。
  *          发送：等 TXE(bit7) 写 DR；接收：RXNE(bit5) 读 DR（读 DR 自动清 RXNE）。
  *          回显 + PC13 翻转。
  ******************************************************************************
  */
#include "stm32f10x.h"

volatile uint32_t g_rx_count = 0;            /* 已收字节数（调试器可读） */

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

/* 发送：先等 TXE（发送数据寄存器空，bit7），再写 DR（写 DR 自动清 TXE） */
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
    SystemCoreClockUpdate();                 /* 72MHz 实测值，delay_ms 用 */

    /* ① 时钟：APB2ENR 一条线开三个——IOPA(bit2)+IOPC(bit4)+USART1(bit14) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* ② 引脚：CRH 读-改-写（保留其他脚配置）
       PA9  = CRH[7:4]  = 0b1011（CNF=10 复用推挽 + MODE=11 50MHz）
       PA10 = CRH[11:8] = 0b0100（CNF=01 浮空输入 + MODE=00）
       合成值 = (0x4<<8) | (0xB<<4) = 0x4B0（⚠️写成 0x4B00 会把 0xB 错移到 PA10 上） */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;

    /* ③ PC13 = 推挽输出 2MHz：CRH[23:20] = 0b0010（02 章同款） */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;
    GPIOC->BSRR = GPIO_BSRR_BS13;            /* 初始灭：BSRR bit13 置位（输出高） */

    /* ④ 波特率：72e6/16/115200 = 39.0625 → 整数 39=0x27 写 [15:4]，小数 1 写 [3:0] */
    USART1->BRR = 0x271;

    /* ⑤ 使能分两步（实测教训：UE 和 TE/RE 同一次写，TE/RE 不生效！）
       ⑤-a UE(bit13) 总开关；⑤-b TE(bit3)+RE(bit2) 收发 */
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

    uart1_str("\r\n=== STM32F103 USART1 READY - REG (115200-8-N-1) ===\r\n");
    uart1_str("Type anything, I will echo it back. LED toggles per byte.\r\n");

    while (1)
    {
        if (USART1->SR & USART_SR_RXNE)      /* bit5：收到字节 */
        {
            uint8_t b = (uint8_t)USART1->DR; /* 读 DR 自动清 RXNE */
            uart1_send(b);                   /* 原样发回 */
            GPIOC->ODR ^= GPIO_ODR_ODR13;    /* 翻转绿灯（本工程无中断，读改写安全） */
            g_rx_count++;
        }
    }
}
