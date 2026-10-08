/**
  ******************************************************************************
  * @file    main.c
  * @brief   03 章：按键控制 LED（寄存器版）——按一下开关闪烁
  * @note    KEY1=PB7（上拉输入，按下=0）；绿灯=PC13（低电平亮）
  *          标准库/HAL 替你做的两件事，这里亲手做：
  *          ① 上拉输入需要 ODR 位=1（光配 CNF 不够！）
  *          ② LED 翻转用 ODR 异或
  ******************************************************************************
  */
#include "stm32f10x.h"

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

/* 查一次按键：捕捉"松开->按下"的沿（消抖+等释放）。返回 1 = 完整按压一次 */
uint8_t key_pressed(void)
{
    static uint8_t last = 1;                     /* 上次状态（static 跨调用记忆） */
    uint8_t now = (GPIOB->IDR & GPIO_IDR_IDR7) ? 1 : 0;   /* 读 IDR bit7：0=按下 1=松开 */
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

    SystemCoreClockUpdate();                     /* 实测 72MHz */

    /* ① 开两个时钟：GPIOB(bit3) + GPIOC(bit4)，一条语句全开（|= 两次也行） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    /* ② PB7 = 上拉输入，两步缺一不可：
       步骤 a：CRL[31:28] = 0b1000（PB7 是第 7 格 -> bit31:28；CNF=10 上/下拉输入 + MODE=00 输入）
       步骤 b：ODR bit7 = 1 -> 选"上拉"（=0 就成下拉，按键失效！标准库/HAL 都替你做了这步） */
    GPIOB->CRL = (GPIOB->CRL & 0x0FFFFFFFU) | 0x80000000U;
    GPIOB->ODR |= GPIO_ODR_ODR7;

    /* ③ PC13 = 推挽输出 2MHz：CRH[23:20] = 0b0010（02 章同款） */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;

    /* ④ 初始灭：BSRR bit13 置位（输出高） */
    GPIOC->BSRR = GPIO_BSRR_BS13;

    while (1)
    {
        if (key_pressed())                       /* 每片开头照看按键 */
        {
            running = !running;                  /* 开 <-> 停 */
            if (running == 0)
                GPIOC->BSRR = GPIO_BSRR_BS13;    /* 停止：灯灭 */
        }
        if (running)
        {
            if (++slice >= 50)                   /* 50 片 x 10ms = 500ms */
            {
                slice = 0;
                GPIOC->ODR ^= GPIO_ODR_ODR13;     /* 翻转：ODR 异或 bit13（本工程无中断，读改写安全） */
            }
        }
        delay_ms(10);                            /* 节拍器 */
    }
}
