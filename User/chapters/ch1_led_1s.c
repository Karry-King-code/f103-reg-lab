/**
  ******************************************************************************
  * @file    main.c
  * @brief   02 章：LED 1 秒闪烁（寄存器版）
  * @note    实测定稿：PC13 = 绿灯（低电平亮，灌电流）
  *          不用任何外设库，直接读写寄存器——每个动作背后是什么，看注释
  *          页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码
  ******************************************************************************
  */
#include "stm32f10x.h"

/* SysTick 毫秒延时（和标准库版同一原理——SysTick 是 Cortex-M3 内核外设，
   不在 RM0008 里，见 PM0056《STM32F10x Cortex-M3 编程手册》第 4 章） */
void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* 1ms 计数次数（SystemCoreClockUpdate 实测：HSE 8M x PLL9 = 72MHz） */
    SysTick->LOAD = tick - 1;                     /* LOAD=重装值寄存器（内核）：数到 0 自动重装；写"次数-1"因为计数含 0 */
    SysTick->VAL  = 0;                            /* VAL=当前计数值：写任意值清零，同时清 COUNTFLAG */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;      /* CTRL bit0=1 启动计数（CLKSOURCE 默认 0=外部时钟 HCLK/8=9MHz） */
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {                                         /* CTRL bit16=COUNTFLAG：数到 0 置 1，读后自动清零=正好过了 1ms */
        }
    SysTick->CTRL = 0;                            /* 用完关掉 */
}

int main(void)
{
    SystemCoreClockUpdate();   /* 实测时钟：底层=读 RCC_CFGR 的 SW/HPRE 位算出 72MHz 写入全局变量 SystemCoreClock */

    /* ① 开 GPIOC 时钟：RCC->APB2ENR = APB2 外设时钟使能寄存器
       bit4 = IOPCEN（IO Port C Enable）。用 |= 只改这一位，不碰其他位。
       地址 = 基址 0x40021000 + 偏移 0x18 = 0x40021018。
       依据：RM0008 中文版 PDF 第 69 页（RCC 寄存器组偏移表）、第 77 页（APB2ENR 位定义表）。 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* ② PC13 配置为推挽输出 2MHz：
       CRH 管 PC8~15，每 4 位一格，格式 [CNF1 CNF0 MODE1 MODE0]。
       PC13 占 CRH[23:20]。推挽输出 2MHz = CNF=00(通用推挽) + MODE=10(2MHz)
       = 0b0010 = 0x2。先清掉这 4 位再填入 0x2，其他脚的配置保持不动。
       依据：RM0008 中文版 PDF 第 113 页（CNF/MODE 编码表）、第 114 页（GPIOx_CRH 位定义表）。 */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;

    /* ③ 上电先灭：BSRR 写 1 把输出置位（高电平）。原子操作，读改写都不用。
       依据：RM0008 中文版 PDF 第 115 页（GPIOx_BSRR / GPIOx_BRR 位定义表）。 */
    GPIOC->BSRR = GPIO_BSRR_BS13;

    while (1)
    {
        GPIOC->BRR  = GPIO_BRR_BR13;    /* BRR  写 1 = 输出复位(低电平) -> 亮（位表同 PDF 第115页） */
        delay_ms(1000);
        GPIOC->BSRR = GPIO_BSRR_BS13;   /* BSRR 写 1 = 输出置位(高电平) -> 灭 */
        delay_ms(1000);
    }
}
