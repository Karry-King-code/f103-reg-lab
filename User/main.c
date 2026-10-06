#include "stm32f10x.h"
/* OLED 双方向 I2C 扫描（纯寄存器临时固件，ch08 诊断用）
   PB8=CRH[3:0] PB9=CRH[7:4]；输出推挽2M=0x2；输入=0x8(CNF=10,高/低由ODR定) */
static void dly(void){ volatile uint32_t n=3000; while(n--){} }
static uint8_t g_sw = 0;   /* 0=SCL=PB8/SDA=PB9；1=对调 */
static void uart1_send(uint8_t b){ while((USART1->SR&USART_SR_TXE)==0){} USART1->DR=b; }
static void uart1_str(const char*s){ while(*s) uart1_send((uint8_t)*s++); }
#define SCL_BIT  (g_sw ? (1u<<9) : (1u<<8))
#define SDA_BIT  (g_sw ? (1u<<8) : (1u<<9))
static void cfg_scl_out(void){
  if(!g_sw){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFFF0U)|0x00000002U; }
  else     { GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000020U; }
  GPIOB->ODR|=SCL_BIT;
}
static void cfg_sda_out_lo(void){
  if(!g_sw){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000020U; }
  else     { GPIOB->CRH=(GPIOB->CRH&0xFFFFFFF0U)|0x00000002U; }
  GPIOB->ODR&=~SDA_BIT;
}
static void cfg_sda_in(void){
  if(!g_sw){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000080U; }
  else     { GPIOB->CRH=(GPIOB->CRH&0xFFFFFFF0U)|0x00000080U; }
  GPIOB->ODR|=SDA_BIT;
}
static uint8_t sda_rd(void){ return (uint8_t)((GPIOB->IDR & SDA_BIT)?1:0); }
static void scl_hi(void){ GPIOB->ODR|=SCL_BIT; }
static void scl_lo(void){ GPIOB->ODR&=~SCL_BIT; }
static uint8_t wr(uint8_t b){ uint8_t i,ack;
  for(i=0;i<8;i++){ if(b&0x80) cfg_sda_in(); else cfg_sda_out_lo(); b<<=1; dly(); scl_hi(); dly(); scl_lo(); }
  cfg_sda_in(); dly(); scl_hi(); dly(); ack=sda_rd(); scl_lo(); return ack; }
static void start(void){ cfg_sda_in(); dly(); scl_hi(); dly(); cfg_sda_out_lo(); dly(); scl_lo(); dly(); }
static void stop(void){ cfg_sda_out_lo(); dly(); scl_hi(); dly(); cfg_sda_in(); dly(); }
static uint8_t scan(uint8_t*buf){ uint8_t a,n=0;
  for(a=3;a<0x78;a++){ start(); if(wr((uint8_t)(a<<1))==0&&n<8) buf[n++]=a; stop(); } return n; }
void delay_ms(uint32_t ms){ uint32_t t=SystemCoreClock/8/1000; SysTick->LOAD=t-1; SysTick->VAL=0;
  SysTick->CTRL=SysTick_CTRL_ENABLE_Msk; while(ms--) while((SysTick->CTRL&SysTick_CTRL_COUNTFLAG_Msk)==0){} SysTick->CTRL=0; }
int main(void){
  uint8_t f[8],n,i;
  SystemCoreClockUpdate();
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN|RCC_APB2ENR_IOPCEN|RCC_APB2ENR_USART1EN;
  GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
  USART1->BRR = 0x271; USART1->CR1 |= USART_CR1_UE; USART1->CR1 |= USART_CR1_TE|USART_CR1_RE;
  GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;
  GPIOB->CRH = (GPIOB->CRH & 0xFFFFFF00U) | 0x00000088U;
  GPIOB->ODR &= ~((1u<<8)|(1u<<9));
  delay_ms(2);
  uart1_str("IPD8="); uart1_send((uint8_t)('0'+((GPIOB->IDR>>8)&1)));
  uart1_str(" IPD9="); uart1_send((uint8_t)('0'+((GPIOB->IDR>>9)&1)));
  GPIOB->ODR |= (1u<<8)|(1u<<9);
  delay_ms(2);
  uart1_str(" IPU8="); uart1_send((uint8_t)('0'+((GPIOB->IDR>>8)&1)));
  uart1_str(" IPU9="); uart1_send((uint8_t)('0'+((GPIOB->IDR>>9)&1)));
  uart1_str("\r\n");
  g_sw=0; cfg_scl_out(); cfg_sda_in();
  n=scan(f);
  if(n==0) uart1_str("ScanA(SCL=PB8): NONE\r\n");
  else { uart1_str("ScanA:"); for(i=0;i<n;i++){ uart1_str(" 0x"); uart1_send((uint8_t)"0123456789ABCDEF"[f[i]>>4]); uart1_send((uint8_t)"0123456789ABCDEF"[f[i]&15]); } uart1_str("\r\n"); }
  g_sw=1; cfg_scl_out(); cfg_sda_in();
  n=scan(f);
  if(n==0) uart1_str("ScanB(SCL=PB9): NONE\r\n");
  else { uart1_str("ScanB:"); for(i=0;i<n;i++){ uart1_str(" 0x"); uart1_send((uint8_t)"0123456789ABCDEF"[f[i]>>4]); uart1_send((uint8_t)"0123456789ABCDEF"[f[i]&15]); } uart1_str("\r\n"); }
  g_sw=0; cfg_scl_out(); cfg_sda_in();
  uart1_str("OLED force-on via scan-code...\r\n");
  start(); wr(0x78); wr(0x00);
  wr(0xAE); wr(0xD5); wr(0x80); wr(0xA8); wr(0x3F); wr(0xD3); wr(0x00);
  wr(0x40); wr(0x8D); wr(0x14); wr(0x20); wr(0x02); wr(0xA1); wr(0xC8);
  wr(0xDA); wr(0x12); wr(0x81); wr(0xCF); wr(0xD9); wr(0xF1); wr(0xDB);
  wr(0x40); wr(0xA4); wr(0xA6); wr(0xAF); wr(0xA5);
  stop();
  uart1_str("commands sent (0xA5=all pixels ON)\r\n");
  g_sw=0;
  while(1){ delay_ms(500); GPIOC->ODR ^= GPIO_ODR_ODR13; }
}
