/**
  ******************************************************************************
  * @file    main.c
  * @brief   02 章：LED 1 秒闪烁（寄存器版）
  * @note    实测定稿：PC13 = 绿灯（低电平亮，灌电流）
  *          不用任何外设库，直接读写寄存器——每个动作背后是什么，看注释
  ******************************************************************************
  */
#include "stm32f10x.h"

/* SysTick 毫秒延时（和标准库版同一原理——SysTick 是内核外设，谁都要自己配） */
void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* 1ms 计数次数 */
    SysTick->LOAD = tick - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
        }
    SysTick->CTRL = 0;
}

int main(void)
{
    SystemCoreClockUpdate();   /* 实测时钟：HSE 8M x PLL9 = 72MHz 写入 SystemCoreClock */

    /* ① 开 GPIOC 时钟：RCC->APB2ENR = APB2 外设时钟使能寄存器
       bit4 = IOPCEN（IO Port C Enable）。用 |= 只改这一位，不碰其他位。 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* ② PC13 配置为推挽输出 2MHz：
       CRH 管 PC8~15，每 4 位一格，格式 [CNF1 CNF0 MODE1 MODE0]。
       PC13 占 CRH[23:20]。推挽输出 2MHz = CNF=00(通用推挽) + MODE=10(2MHz)
       = 0b0010 = 0x2。先清掉这 4 位再填入 0x2，其他脚的配置保持不动。 */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;

    /* ③ 上电先灭：BSRR 写 1 把输出置位（高电平）。原子操作，读改写都不用。 */
    GPIOC->BSRR = GPIO_BSRR_BS13;

    while (1)
    {
        GPIOC->BRR  = GPIO_BRR_BR13;    /* BRR  写 1 = 输出复位(低电平) -> 亮 */
        delay_ms(1000);
        GPIOC->BSRR = GPIO_BSRR_BS13;   /* BSRR 写 1 = 输出置位(高电平) -> 灭 */
        delay_ms(1000);
    }
}
