/**
  ******************************************************************************
  * @file    main.c
  * @brief   03 章：按键控制 LED（寄存器版）——按一下开关闪烁
  * @note    KEY1=PB7（上拉输入，按下=0）；绿灯=PC13（低电平亮）
  *          标准库/HAL 替你做的两件事，这里亲手做：
  *          ① 上拉输入需要 ODR 位=1（光配 CNF 不够！）
  *          ② LED 翻转用 ODR 异或
  *          页码注释约定：RM0008 = 《STM32F103xx参考手册(中文参考)》，页码为 PDF 阅读器页码
  ******************************************************************************
  */
#include "stm32f10x.h"

void delay_ms(uint32_t ms)
{
    uint32_t tick = SystemCoreClock / 8 / 1000;   /* SysTick 内核外设：PM0056 第4章（RM0008 无） */
    SysTick->LOAD = tick - 1;                     /* 重装值（含 0 所以 -1） */
    SysTick->VAL  = 0;                            /* 清计数与 COUNTFLAG */
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;      /* bit0=1 启动，时钟源 HCLK/8 */
    while (ms--)
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {                                         /* bit16 COUNTFLAG 数到 0 置位，读即清 */
        }
    SysTick->CTRL = 0;
}

/* 查一次按键：捕捉"松开->按下"的沿（消抖+等释放）。返回 1 = 完整按压一次 */
uint8_t key_pressed(void)
{
    static uint8_t last = 1;                     /* 上次状态（static 跨调用记忆） */
    uint8_t now = (GPIOB->IDR & GPIO_IDR_IDR7) ? 1 : 0;   /* 读 IDR bit7：0=按下 1=松开（IDR 偏移 0x08，RM0008 PDF 第105页 GPIO 寄存器组表） */
    uint8_t evt  = 0;

    if (last == 1 && now == 0)                   /* 1 -> 0 = 按下沿 */
    {
        delay_ms(10);                            /* 按下消抖 */
        if ((GPIOB->IDR & GPIO_IDR_IDR7) == 0)    /* 抖完仍是 0 = 真按下 */
        {
            evt = 1;
            while ((GPIOB->IDR & GPIO_IDR_IDR7) == 0)
            {
            }                                    /* 等松手 */
            delay_ms(10);                        /* 释放消抖 */
        }
    }
    last = now;
    return evt;
}

int main(void)
{
    uint8_t  running = 0;                        /* 闪烁开关 */
    uint16_t slice   = 0;                        /* 10ms 片计数 */

    SystemCoreClockUpdate();                     /* 实测 72MHz（底层=读 RCC_CFGR 算） */

    /* ① 开两个时钟：GPIOB(bit3) + GPIOC(bit4)，一条语句全开（|= 两次也行）
       APB2ENR 地址=0x40021000+0x18；位表 RM0008 中文版 PDF 第77页 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    /* ② PB7 = 上拉输入，两步缺一不可：
       步骤 a：CRL[31:28] = 0b1000（PB7 是第 7 格 -> bit31:28；CNF=10 上/下拉输入 + MODE=00 输入）
       步骤 b：ODR bit7 = 1 -> 选"上拉"（=0 就成下拉，按键失效！标准库/HAL 都替你做了这步）
       依据：RM0008 中文版 PDF 第113页(CNF/MODE 编码表)/第114页(CRL 位表) */
    GPIOB->CRL = (GPIOB->CRL & 0x0FFFFFFFU) | 0x80000000U;
    GPIOB->ODR |= GPIO_ODR_ODR7;

    /* ③ PC13 = 推挽输出 2MHz：CRH[23:20] = 0b0010（02 章同款；PDF 第113/114页） */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;

    /* ④ 初始灭：BSRR bit13 置位（输出高）；位表 RM0008 PDF 第115页 */
    GPIOC->BSRR = GPIO_BSRR_BS13;

    while (1)
    {
        if (key_pressed())                       /* 每片开头照看按键 */
        {
            running = !running;                  /* 开 <-> 停 */
            if (running == 0)
                GPIOC->BSRR = GPIO_BSRR_BS13;    /* 停止：灯灭（BSRR 置位，原子） */
        }
        if (running)
        {
            if (++slice >= 50)                   /* 50 片 x 10ms = 500ms */
            {
                slice = 0;
                GPIOC->ODR ^= GPIO_ODR_ODR13;     /* 翻转：ODR 异或 bit13（本工程无中断，读改写安全；有中断并发时应改 BSRR） */
            }
        }
        delay_ms(10);                            /* 节拍器 */
    }
}
