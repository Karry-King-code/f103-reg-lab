# f103-reg-lab —— STM32F103C8T6 精英板 · 寄存器版学习仓库

STM32F103C8T6 精英板（嘉立创 v1.3）外设驱动学习项目，**直接操作寄存器**版本——不用任何外设库函数，只靠 CMSIS 头文件里的寄存器地址定义，亲手读写每一个寄存器位。

> 本仓库是三条学习线之一：同一套外设功能（LED → 串口 → 按键 → 蜂鸣器/继电器 → 温湿度 → 烟雾 → OLED → W25Q Flash → ESP8266 连 OneNET）分别用 **[标准库] / [HAL 库] / [寄存器（本仓库）]** 三种方式各实现一遍。

## 教程目录

| 章节 | 内容 | 状态 |
|---|---|---|
| [01-环境搭建与脚手架](docs/01-环境搭建与脚手架.md) | 寄存器开发的工程形态、CMSIS 头文件、编译烧录 | ✅ 脚手架编译通过 |
| 02-GPIO输出-LED 1s闪烁 | 手写 RCC/GPIO 寄存器点灯（逐位讲解） | ✅ 上板验收 |
| 03-GPIO输入-按键控LED | CRL+ODR 两步上拉/IDR/ODR 异或翻转 | ✅ 上板验收 |
| 04-USART-串口收发 | 手算 BRR=0x271/CR1 分两步使能/SWD 读寄存器验证 | ✅ 上板验收 |
| 05-任务五 温湿度 DHT11 | 单总线时序/DWT 微秒延时 | ✅ 上板验收 |
| 06-任务六 光敏（烟雾替身） | 开漏必须上拉坑 | ✅ 上板验收 |
| 07-任务七 OLED | 软件 I2C 位带 | ✅ 上板验收 |
| 08-任务八 SPI Flash | W25Q16 实测+掉电不丢写测试 | ✅ 上板验收 |
| 09-任务九 WiFi 上云 | ESP8266 AT+手工 MQTT+OneNET | ✅ 上板验收 |

## 工程一览

- **主控**：STM32F103C8T6（Cortex-M3，72MHz，64KB Flash / 20KB RAM）
- **库**：无外设库。仅 ARM/ST 官方 **CMSIS** 头文件（`stm32f10x.h` 提供寄存器地址/位定义，非驱动代码）
- **IDE**：Keil MDK5 + AC5 + Keil::STM32F1xx_DFP 2.3.0
- **烧录**：ST-Link（SWD）+ STM32CubeProgrammer

```
f103-reg-lab/
├─ User/                          # main.c（寄存器代码主战场）+ 中断骨架
├─ Libraries/CMSIS/CM3/           # 内核层 + 设备层（stm32f10x.h / system / 启动文件）
├─ MDK-ARM/f103-reg-lab.uvprojx   # Keil 工程（无 SPL/HAL 任何 .c）
└─ docs/                          # 教程文档（原理图/手册不入库）
```

## 硬件资料说明

原理图与芯片手册因版权原因**保存在本地不入仓库**。引脚速查见 `docs/01-环境搭建与脚手架.md`。

## 每章独立代码（想跑哪章，就把对应文件内容复制进 main.c 编译烧录）

| 任务 | 内容 | 代码文件 | 状态 |
|---|---|---|---|
| 一 | 点亮 LED 灯 | `User/chapters/ch1_led_1s.c` | ✅ 烧录验证 |
| 二 | 串口收发 | `User/chapters/ch2_uart_echo.c` | ✅ 烧录验证 |
| 三 | 按键控制 LED | `User/chapters/ch3_key_led.c` | ✅ 烧录验证 |
| 四 | 蜂鸣器和继电器 | `User/chapters/ch4_buzzer_relay.c` | ✅ 烧录验证 |
| 五+六 | 温湿度 + 光敏打印 | `User/chapters/ch5_6_7_dht11_light_oled_reg.c` | ✅ 烧录验证 |
| 七 | OLED 显示 | 见上表文件（含 OLED 代码）| ✅ 烧录验证 |
| 八 | SPI Flash 读写+写入测试 | `User/chapters/ch8_spi_flash_reg.c` / `ch8_spi_flash_writetest_reg.c` | ✅ 烧录验证 |
| 九 | WiFi上云 ESP8266→OneNET | `User/chapters/ch9_wifi_onenet_reg.c` | ✅ 烧录验证 |

**接线定案**：DHT11=VCC/3V3 + DAT/B14 + GND｜光敏=VCC/3V3 + DO/B12 + GND｜OLED=VCC·GND·SCL/B8·SDA/B9
**三腿差异**：光敏上拉输入 = `GPIO_Mode_IPU`（标准库）/ `Pull=GPIO_PULLUP`（HAL）/ `CRH=0x8 且 ODR=1`（寄存器）
