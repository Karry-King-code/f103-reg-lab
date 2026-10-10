#include "stm32f10x.h"
/* 紧急静音固件：上电第一时间把 PC15（蜂鸣器）拉低、PC14（继电器）拉高
   然后死循环，不做任何其他事，保证绝对安静 */
int main(void)
{
    SystemCoreClockUpdate();
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* PC13/PC14/PC15 推挽输出（CRH 三格 = 0x2222） */
    GPIOC->CRH = (GPIOC->CRH & 0x0000FFFFU) | 0x22220000U;

    /* 立刻静音：PC15=0(蜂鸣器不响) PC14=1(继电器释放) PC13=1(灯灭) */
    GPIOC->BRR  = (1u << 15);        /* PC15 输出低 = 静音 */
    GPIOC->BSRR = (1u << 14);        /* PC14 输出高 = 继电器释放 */
    GPIOC->BSRR = (1u << 13);        /* PC13 输出高 = 灯灭 */

    while(1){ }
}
