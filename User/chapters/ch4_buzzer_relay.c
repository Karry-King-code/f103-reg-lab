/**
  ******************************************************************************
  * @file    main.c
  * @brief   05 章：按键控制蜂鸣器+继电器（寄存器版）
  * @note    KEY1=PB7 蜂鸣器(PC15 高响)；KEY2=PB6 继电器(PC14 低吸合实测)。
  *          串口命令 B/R/? 双通道验证；UART = 04 章同款(BRR=0x271, CR1分两写)。
  *          GPIOC CRH[31:16] = 0x2222（PC12~15 全推挽输出 2MHz）。
  *          GPIOB CRL：PB7/PB6 上拉输入两步（CNF=10 且 ODR=1）。
  ******************************************************************************
  */
#include "stm32f10x.h"

volatile uint32_t g_rx_count = 0;
static uint8_t buzz_state = 0, relay_state = 0;

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

static void print_state(void)
{
    uart1_str("[STATE] BUZZER=");
    uart1_str(buzz_state ? "ON" : "OFF");
    uart1_str("  RELAY=");
    uart1_str(relay_state ? "ON(closed)" : "OFF(open)");
    uart1_str("\r\n");
}
static void toggle_buzz(void)
{
    buzz_state ^= 1;
    GPIOC->BSRR = buzz_state ? GPIO_BSRR_BS15 : GPIO_BSRR_BR15;
    /* BSRR: 低 16 位=置位(set)，高 16 位=复位(reset)——都写 1 生效，互不干扰 */
    uart1_str("[KEY1/CMD B] ");
    print_state();
}
static void toggle_relay(void)
{
    relay_state ^= 1;
    GPIOC->BSRR = relay_state ? GPIO_BSRR_BR14 : GPIO_BSRR_BS14;
    /* 继电器低吸合：ON=Reset bit14(写 1<<30)，OFF=Set bit14(写 1<<14) */
    uart1_str("[KEY2/CMD R] ");
    print_state();
}

/* 按键：沿检测+双消抖+等释放（03 章同款，pin 指定 6/7） */
static uint8_t key_pressed(uint8_t pinno)      /* pinno: 6 or 7 */
{
    static uint8_t last7 = 1, last6 = 1;
    uint16_t mask = (uint16_t)(1u << pinno);
    uint8_t *last = (pinno == 7) ? &last7 : &last6;
    uint8_t now = (GPIOB->IDR & mask) ? 1 : 0;
    uint8_t evt = 0;

    if (*last == 1 && now == 0)
    {
        delay_ms(10);
        if ((GPIOB->IDR & mask) == 0)
        {
            evt = 1;
            while ((GPIOB->IDR & mask) == 0)
            {
            }
            delay_ms(10);
        }
    }
    *last = now;
    return evt;
}

int main(void)
{
    SystemCoreClockUpdate();

    /* ① 时钟：IOPA+IOPB+IOPC+USART1（一条线全开） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN |
                    RCC_APB2ENR_IOPCEN | RCC_APB2ENR_USART1EN;

    /* ② GPIOC：PC13/14/15 推挽输出 2MHz → CRH[31:16] = 0x2222（保留低半段） */
    GPIOC->CRH = (GPIOC->CRH & 0x0000FFFFU) | 0x22220000U;
    GPIOC->BSRR = GPIO_BSRR_BS13;            /* LED 灭 */
    GPIOC->BSRR = GPIO_BSRR_BS14;            /* 继电器 OFF（高=释放） */
    /* PC15 复位值已是 0（蜂鸣器 OFF），无需动作 */

    /* ③ 按键 PB7/PB6：上拉输入两步（03 章教训：光配 CNF 不够，ODR 必须置 1）
       PB7: CRL[31:28]=0b1000   PB6: CRL[27:24]=0b1000 → CRL[31:24] = 0x88 */
    GPIOB->CRL = (GPIOB->CRL & 0x00FFFFFFU) | 0x88000000U;
    GPIOB->ODR |= GPIO_ODR_ODR7 | GPIO_ODR_ODR6;

    /* ④ USART1：PA9/PA10（04 章同款：CRH 0x4B0、BRR=0x271、CR1 分两次写） */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
    USART1->BRR = 0x271;
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

    uart1_str("\r\n=== CH05 BUZZER+RELAY (REG) READY ===\r\n");
    print_state();

    while (1)
    {
        if (key_pressed(7))
            toggle_buzz();
        if (key_pressed(6))
            toggle_relay();

        if (USART1->SR & USART_SR_RXNE)
        {
            uint8_t b = (uint8_t)USART1->DR;
            g_rx_count++;
            if (b == 'B' || b == 'b')
                toggle_buzz();
            else if (b == 'R' || b == 'r')
                toggle_relay();
            else if (b == '?')
                print_state();
        }
    }
}
