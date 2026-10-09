#include "stm32f10x.h"
/* ===== 任务八（寄存器版）：SPI W25Q 完整测试——擦除/写入/读回/断电不丢 =====
   SPI2：CS=PB12(手动) SCK=PB13 MISO=PB14 MOSI=PB15，CR1=0x35C(主机/8位/模式0/4.5MHz)
   流程：①读ID ②读地址0的5字节 → 等于 "STM32" 说明断电不丢；否则擦除→写入→读回
   结果写 RAM 0x20004000：[0]=0xAA(ID) [1]=0xBB(写成功) [2]=0xCC(持久)
   ⚠️ 写入测试必须拔掉光敏(PB12)和温湿度(PB14)的线 */
#define RES ((volatile uint8_t *)0x20004000)
#define ADDR 0x000000u

static void uart1_send(uint8_t b){ while((USART1->SR&USART_SR_TXE)==0){} USART1->DR=b; }
static void uart1_str(const char *s){ while(*s) uart1_send((uint8_t)*s++); }
static void uart1_hex(uint8_t v){ const char *h="0123456789ABCDEF"; uart1_send((uint8_t)h[v>>4]); uart1_send((uint8_t)h[v&15]); }

static void cs_low(void){ GPIOB->ODR &= ~(1u<<12); }
static void cs_high(void){ GPIOB->ODR |= (1u<<12); }

static uint8_t spi_xfer(uint8_t b)
{
    while ((SPI2->SR & SPI_SR_TXE) == 0) {}
    SPI2->DR = b;
    while ((SPI2->SR & SPI_SR_RXNE) == 0) {}
    return (uint8_t)SPI2->DR;
}

static void wait_ready(void)
{
    uint8_t sr;
    do { cs_low(); spi_xfer(0x05); sr = spi_xfer(0xFF); cs_high(); } while (sr & 0x01);
}

static void flash_read(uint32_t addr, uint8_t *buf, uint8_t n)
{
    uint8_t i;
    cs_low();
    spi_xfer(0x03);
    spi_xfer((uint8_t)(addr>>16)); spi_xfer((uint8_t)(addr>>8)); spi_xfer((uint8_t)addr);
    for(i=0;i<n;i++) buf[i] = spi_xfer(0xFF);
    cs_high();
}

int main(void)
{
    uint8_t id[3], buf[5], i, same;
    const char *magic = "STM32";

    SystemCoreClockUpdate();
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN|RCC_APB2ENR_IOPCEN|RCC_APB2ENR_USART1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /* PB12 输出(CS)；PB13/PB15 复用推挽50M；PB14 浮空输入 */
    GPIOB->CRH = (GPIOB->CRH & 0x0000FFFFU) | 0xB4B20000U;
    cs_high();

    /* SPI2：MSTR+SSM+SSI+SPE+BR=/8 */
    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | (3u<<3);
    SPI2->CR2 = 0;

    /* 串口 */
    GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
    USART1->BRR = 0x271;
    USART1->CR1 |= USART_CR1_UE;
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

    /* PC13 输出（完成后点亮） */
    GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;

    uart1_str("\r\n=== TASK8 REG: FLASH WRITE/PERSIST TEST ===\r\n");

    /* ① ID */
    cs_low(); spi_xfer(0x9F);
    id[0]=spi_xfer(0xFF); id[1]=spi_xfer(0xFF); id[2]=spi_xfer(0xFF);
    cs_high();
    uart1_str("JEDEC ID: "); uart1_hex(id[0]); uart1_send(' '); uart1_hex(id[1]); uart1_send(' '); uart1_hex(id[2]);
    uart1_str(" (W25Q 系列)\r\n");
    RES[0] = (id[0]==0xEF) ? 0xAA : 0x00;

    /* ② 先读 */
    flash_read(ADDR, buf, 5);
    uart1_str("READ before: ");
    for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_send(' '); }
    uart1_str("\r\n");

    same = 1;
    for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;

    if (same) {
        uart1_str("PERSIST OK: \"STM32\" survived reset! (flash keeps data)\r\n");
        RES[2] = 0xCC;
    } else {
        uart1_str("Erasing sector...\r\n");
        cs_low(); spi_xfer(0x06); cs_high();                 /* 写使能 */
        cs_low();
        spi_xfer(0x20);                                       /* 扇区擦除 4KB */
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        cs_high();
        wait_ready();
        uart1_str("Erase done\r\n");

        cs_low(); spi_xfer(0x06); cs_high();                 /* 每次写前都要写使能 */
        cs_low();
        spi_xfer(0x02);                                       /* 页编程 */
        spi_xfer((uint8_t)(ADDR>>16)); spi_xfer((uint8_t)(ADDR>>8)); spi_xfer((uint8_t)ADDR);
        for(i=0;i<5;i++) spi_xfer((uint8_t)magic[i]);
        cs_high();
        wait_ready();
        uart1_str("Write done\r\n");

        flash_read(ADDR, buf, 5);
        uart1_str("READ after : ");
        for(i=0;i<5;i++){ uart1_hex(buf[i]); uart1_send(' '); }
        same = 1;
        for(i=0;i<5;i++) if(buf[i] != (uint8_t)magic[i]) same = 0;
        uart1_str(same ? " -> VERIFY PASS\r\n" : " -> VERIFY FAIL\r\n");
        RES[1] = same ? 0xBB : 0x00;
        if (same) uart1_str("Now press RESET: data should stay\r\n");
    }

    GPIOC->BSRR = GPIO_BSRR_BR13;      /* PC13 亮 = 测试完成 */
    while(1){ }
}
