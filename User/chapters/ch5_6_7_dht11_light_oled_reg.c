#include "stm32f10x.h"
/* ===== 任务五+六（寄存器版）：OLED显示 + 光敏(PB12) + DHT11温湿度(PB14) =====
   屏幕四行：F103 SMART ENV / LIGHT:OK|DARK / HUMI:xx.x% / TEMP:xx.xC
   串口同步打印；结果写 RAM 0x20004000（[0]光敏 [1]err [2]湿度 [3]温度 [4]成功数）
   ★光敏 DO 是开漏输出：PB12 必须配【上拉输入】= CRH=0x8 且 ODR12=1
   ★DHT 数据线 PB14（B15 孔磨损弃用）；读取失败卡死时断电重启 VCC */
#define RES ((volatile uint8_t *)0x20004000)

/* ---- DWT 微秒延时 ---- */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFC)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static void dwt_init(void){ DEMCR |= (1u<<24); DWT_CYCCNT=0; DWT_CTRL |= 1u; }
static void delay_us(uint32_t us){ uint32_t s=DWT_CYCCNT; while((DWT_CYCCNT-s) < us*72){} }
static void delay_ms(uint32_t ms){ while(ms--) delay_us(1000); }

/* ---- 串口 ---- */
static void uart1_send(uint8_t b){ while((USART1->SR&USART_SR_TXE)==0){} USART1->DR=b; }
static void uart1_str(const char *s){ while(*s) uart1_send((uint8_t)*s++); }
static void uart1_dec2(uint8_t v){ uart1_send((uint8_t)('0'+v/10%10)); uart1_send((uint8_t)('0'+v%10)); }

/* ---- 软件 I2C（OLED）---- */
static void dly(void){ volatile uint32_t n=800; while(n--){} }
#define SCL_BIT  (1u<<8)
#define SDA_BIT  (1u<<9)
static void cfg_scl_out(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFFF0U)|0x00000002U; GPIOB->ODR|=SCL_BIT; }
static void cfg_sda_in(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000080U; GPIOB->ODR|=SDA_BIT; }
static void cfg_sda_lo(void){ GPIOB->CRH=(GPIOB->CRH&0xFFFFFF0FU)|0x00000020U; GPIOB->ODR&=~SDA_BIT; }
static void scl_hi(void){ GPIOB->ODR |= SCL_BIT; }
static void scl_lo(void){ GPIOB->ODR &= ~SCL_BIT; }
static uint8_t sda_rd(void){ return (uint8_t)((GPIOB->IDR & SDA_BIT)?1:0); }
static uint8_t wr(uint8_t b){ uint8_t i,ack;
  for(i=0;i<8;i++){ if(b&0x80) cfg_sda_in(); else cfg_sda_lo(); b<<=1; dly(); scl_hi(); dly(); scl_lo(); }
  cfg_sda_in(); dly(); scl_hi(); dly(); ack=sda_rd(); scl_lo(); return ack; }
static void start(void){ cfg_sda_in(); dly(); scl_hi(); dly(); cfg_sda_lo(); dly(); scl_lo(); dly(); }
static void stop(void){ cfg_sda_lo(); dly(); scl_hi(); dly(); cfg_sda_in(); dly(); }

static const uint8_t F57[] = {
0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x5F,0x00,0x00, 0x00,0x07,0x00,0x07,0x00, 0x14,0x7F,0x14,0x7F,0x14,
0x24,0x2A,0x7F,0x2A,0x12, 0x23,0x13,0x08,0x64,0x62, 0x36,0x49,0x55,0x22,0x50, 0x00,0x05,0x03,0x00,0x00,
0x00,0x1C,0x22,0x41,0x00, 0x00,0x41,0x22,0x1C,0x00, 0x14,0x08,0x3E,0x08,0x14, 0x08,0x08,0x3E,0x08,0x08,
0x00,0x50,0x30,0x00,0x00, 0x08,0x08,0x08,0x08,0x08, 0x00,0x60,0x60,0x00,0x00, 0x20,0x10,0x08,0x04,0x02,
0x3E,0x51,0x49,0x45,0x3E, 0x00,0x42,0x7F,0x40,0x00, 0x42,0x61,0x51,0x49,0x46, 0x21,0x41,0x45,0x4B,0x31,
0x18,0x14,0x12,0x7F,0x10, 0x27,0x45,0x45,0x45,0x39, 0x3C,0x4A,0x49,0x49,0x30, 0x01,0x71,0x09,0x05,0x03,
0x36,0x49,0x49,0x49,0x36, 0x06,0x49,0x49,0x29,0x1E, 0x00,0x36,0x36,0x00,0x00, 0x00,0x56,0x36,0x00,0x00,
0x08,0x14,0x22,0x41,0x00, 0x14,0x14,0x14,0x14,0x14, 0x00,0x41,0x22,0x14,0x08, 0x02,0x01,0x51,0x09,0x06,
0x32,0x49,0x79,0x41,0x3E, 0x7E,0x11,0x11,0x11,0x7E, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01, 0x3E,0x41,0x49,0x49,0x7A,
0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41,
0x7F,0x40,0x40,0x40,0x40, 0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46, 0x46,0x49,0x49,0x49,0x31,
0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F,
0x63,0x14,0x08,0x14,0x63, 0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43 };
static void o_cmd(uint8_t c){ start(); wr(0x78); wr(0x00); wr(c); stop(); }
static void o_data(uint8_t d){ start(); wr(0x78); wr(0x40); wr(d); stop(); }
static void o_pos(uint8_t pg, uint8_t col){ o_cmd(0xB0|pg); o_cmd(0x00|(col&0x0F)); o_cmd(0x10|(col>>4)); }
static void o_print(uint8_t pg, uint8_t col, const char *s){
  o_pos(pg,col);
  while(*s){ uint8_t c=(uint8_t)*s++; uint8_t i;
    if(c<0x20||c>0x5F) c=' ';
    for(i=0;i<5;i++) o_data(F57[(c-0x20)*5+i]);
    o_data(0x00); }
}
static void o_clear(uint8_t pg){ uint8_t i; o_pos(pg,0); for(i=0;i<128;i++) o_data(0x00); }
static void o_init(void){
  uint8_t i; for(i=0;i<100;i++) dly();
  start(); wr(0x78); wr(0x00);
  wr(0xAE); wr(0xD5); wr(0x80); wr(0xA8); wr(0x3F); wr(0xD3); wr(0x00);
  wr(0x40); wr(0x8D); wr(0x14); wr(0x20); wr(0x02); wr(0xA1); wr(0xC8);
  wr(0xDA); wr(0x12); wr(0x81); wr(0xCF); wr(0xD9); wr(0xF1); wr(0xDB);
  wr(0x40); wr(0xA4); wr(0xA6); wr(0xAF);
  stop();
}

/* ---- DHT11（PB14）---- */
static void dht_out_low(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x02000000U; GPIOB->ODR&=~(1u<<14); }
static void dht_rel(void){ GPIOB->CRH=(GPIOB->CRH&0xF0FFFFFFU)|0x08000000U; GPIOB->ODR|=(1u<<14); }
#define DHT_READ() ((GPIOB->IDR>>14)&1u)
static uint8_t wait_line(uint8_t lv){
  uint32_t t0 = DWT_CYCCNT;
  while (DHT_READ() != lv)
    if ((DWT_CYCCNT - t0) > 5000u*72) return 1;
  return 0;
}
/* 0=成功 1=超时 2=校验错 */
static uint8_t dht11_read(uint8_t d[5]){
  uint8_t i, j;
  for(i=0;i<5;i++) d[i]=0;
  dht_out_low(); delay_ms(20); dht_rel();
  if(wait_line(0)) { RES[8]=0x21; return 1; }
  if(wait_line(1)) { RES[8]=0x22; return 1; }
  if(wait_line(0)) { RES[8]=0x23; return 1; }
  for(i=0;i<5;i++){
    for(j=0;j<8;j++){
      if(wait_line(1)) { RES[8]=0x24; return 1; }
      delay_us(40);
      d[i] = (uint8_t)(d[i]<<1);
      if(DHT_READ()){ d[i] |= 1; if(wait_line(0)) { RES[8]=0x25; return 1; } }
    }
  }
  if((uint8_t)(d[0]+d[1]+d[2]+d[3]) != d[4]) { RES[8]=2; return 2; }
  RES[8]=4;
  return 0;
}

int main(void){
  uint8_t d[5], err, disp=0xFF, humi_i=0xFF, temp_i=0xFF, ok_cnt=0;
  char buf[8];
  SystemCoreClockUpdate();
  dwt_init();
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN|RCC_APB2ENR_IOPBEN|RCC_APB2ENR_IOPCEN|RCC_APB2ENR_USART1EN;
  GPIOA->CRH = (GPIOA->CRH & 0xFFFF00FFU) | 0x000004B0U;
  USART1->BRR = 0x271;
  USART1->CR1 |= USART_CR1_UE;
  USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;

  /* PB12 光敏 DO = 上拉输入（开漏输出必须上拉！CRH=0x8 + ODR12=1） */
  GPIOB->CRH = (GPIOB->CRH & 0xFF00FF0FU) | 0x40080000U;
  GPIOB->ODR |= (1u<<12);
  /* PB14 DHT = 浮空输入（读取时函数内部切换） */
  /* PB8 推挽输出 */
  GPIOB->CRH = (GPIOB->CRH & 0xFFFFFFF0U) | 0x00000002U;
  cfg_scl_out();
  cfg_sda_in();
  /* PC13 心跳 */
  GPIOC->CRH = (GPIOC->CRH & 0xFF0FFFFFU) | 0x00200000U;
  GPIOC->ODR |= (1u<<13);

  delay_ms(100);
  o_init();
  { uint8_t p; for(p=0;p<8;p++) o_clear(p); }
  o_print(0, 0, "F103 SMART ENV");
  o_print(2, 0, "LIGHT:");
  o_print(4, 0, "HUMI:");
  o_print(6, 0, "TEMP:");
  uart1_str("\r\n=== TASK5+6 REG: OLED + DHT11(PB14) + LIGHT(PB12) ===\r\n");

  { uint8_t lastv = 0xFF;
    while(1){
      uint8_t now = (uint8_t)((GPIOB->IDR>>12)&1);
      if(now != lastv){
        lastv = now;
        o_print(2, 48, now ? "OK  " : "DARK");
      }
      err = dht11_read(d);
      RES[0]=now; RES[1]=err; RES[2]=d[0]; RES[3]=d[2]; RES[4]=ok_cnt;
      uart1_str("LIGHT="); uart1_send((uint8_t)('0'+now));
      if(err==0){
        uart1_str("  HUMI="); uart1_dec2(d[0]); uart1_str("."); uart1_send((uint8_t)('0'+d[1])); uart1_str("%");
        uart1_str("  TEMP="); uart1_dec2(d[2]); uart1_str("."); uart1_send((uint8_t)('0'+d[3])); uart1_str("C\r\n");
        ok_cnt++;
        if(d[0] != humi_i){ humi_i = d[0];
          o_print(4, 40, "      ");
          buf[0]=(char)('0'+d[0]/100); buf[1]=(char)('0'+d[0]/10%10); buf[2]=(char)('0'+d[0]%10); buf[3]='.'; buf[4]=(char)('0'+d[1]); buf[5]='%'; buf[6]=0;
          o_print(4, 40, buf); }
        if(d[2] != temp_i){ temp_i = d[2];
          o_print(6, 40, "      ");
          buf[0]=(char)('0'+d[2]/100); buf[1]=(char)('0'+d[2]/10%10); buf[2]=(char)('0'+d[2]%10); buf[3]='.'; buf[4]=(char)('0'+d[3]); buf[5]='C'; buf[6]=0;
          o_print(6, 40, buf); }
      } else {
        uart1_str(err==1 ? "  DHT11 TIMEOUT\r\n" : "  DHT11 CHECKSUM ERR\r\n");
        o_print(4, 40, " -- ");
        o_print(6, 40, " -- ");
        humi_i=0xFF; temp_i=0xFF;
      }
      delay_ms(1500);
      GPIOC->ODR ^= GPIO_ODR_ODR13;
    }
  }
}
