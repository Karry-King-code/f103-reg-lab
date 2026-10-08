#include "stm32f10x.h"
/* ===== 任务八（寄存器版）：SPI W25Q 读 JEDEC ID + 读数据 =====
   板载 Flash 挂 SPI2（APB1 36MHz）：CS=PB12(手动) SCK=PB13 MISO=PB14 MOSI=PB15
   SPI2->CR1 = MSTR|SPE|SSM|SSI|BR=011(/8=4.5MHz)，模式0（CPOL=0 CPHA=0）
   读 ID 0x9F：厂商+类型+容量（实测本板 EF 40 15 = W25Q16 2MB）
   结果写 RAM 0x20004000：[0]=0xAA [1..3]=ID [4..7]=地址0起4字节
   ⚠️ 烧录前拔掉光敏(DO线)和温湿度(DAT线)——与 Flash 共用 PB12/PB14 */
#define RES ((volatile uint8_t *)0x20004000)

static void uart1_send(uint8_t b){ while((USART1->SR&USART_SR_TXE)==0){} USART1->DR=b; }
static void uart1_str(const char *s){ while(*s) uart1_send((uint8_t)*s++); }
static void uart1_hex(uint8_t v){ const char *h="0123456789ABCDEF"; uart1_send((uint8_t)h[v>>4]); uart1_send((uint8_t)h[v&15]); }

/* CS：PB12 手动控制 */
static void cs_low(void){ GPIOB->ODR &= ~(1u<<12); }
static void cs_high(void){ GPIOB->ODR |= (1u<<12); }

/* SPI2 收发一个字节：等 TXE 写 DR -> 等 RXNE 读 DR */
static uint8_t spi_xfer(uint8_t b)
{
    while ((SPI2->SR & SPI_SR_TXE) == 0) {}
    SPI2->DR = b;
    while ((SPI2->SR & SPI_SR_RXNE) == 0) {}
    return (uint8_t)SPI2->DR;
}

int main(void)
{
    uint8_t id[3], data0[4], sr, i;

    SystemCoreClockUpdate();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN|RCC_APB2ENR_IOPCEN|RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /* PB12 推挽输出(CS) [19:16]=0x2；PB13/PB15 复用推挽50M [23:20]=[31:28]=0xB；PB14 浮空输入 [27:24]=0x4 */
    GPIOB->CRH = (GPIOB->CRH & 0x0000FFFFU) | 0xB4B20000U;
    cs_high();

    /* SPI2 配置：MSTR + SSM + SSI + BR=/8 + SPE；CPOL/CPHA=0 模式0 */
    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | (3u<<3);
    SPI2->CR2 = 0;

    /* 串口 */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
    USART1->BRR = 0x271;
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

    uart1_str("\r\n=== TASK8 REG: W25Q SPI READ ID ===\r\n");

    /* ① 状态寄存器 0x05 */
    cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high();
    uart1_str("STATUS=0x"); uart1_hex(sr); uart1_str(sr==0 ? " (idle)\r\n" : " (BUSY!)\r\n");

    /* ② JEDEC ID 0x9F */
    cs_low();
    spi_xfer(0x9F);
    id[0]=spi_xfer(0xFF); id[1]=spi_xfer(0xFF); id[2]=spi_xfer(0xFF);
    cs_high();
    uart1_str("JEDEC ID: "); uart1_hex(id[0]); uart1_send(' '); uart1_hex(id[1]); uart1_send(' '); uart1_hex(id[2]);
    if(id[0]==0xEF && id[1]==0x40) uart1_str("  -> Winbond OK\r\n");
    else uart1_str("  -> 读取失败\r\n");

    /* ③ 读地址0起4字节（0x03 + 24位地址） */
    cs_low();
    spi_xfer(0x03); spi_xfer(0x00); spi_xfer(0x00); spi_xfer(0x00);
    for(i=0;i<4;i++) data0[i] = spi_xfer(0xFF);
    cs_high();
    uart1_str("DATA[0..3]: ");
    for(i=0;i<4;i++){ uart1_hex(data0[i]); uart1_send(' '); }
    uart1_str("\r\n");

    RES[0] = (id[0]==0xEF) ? 0xAA : 0x00;
    RES[1]=id[0]; RES[2]=id[1]; RES[3]=id[2];
    RES[4]=data0[0]; RES[5]=data0[1]; RES[6]=data0[2]; RES[7]=data0[3];

    /* PC13 亮起表示完成 */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;
    GPIOC->BSRR = GPIO_BSRR_BR13;

    while(1){ }
}
