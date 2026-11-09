# 03. STM32CubeMX 및 MCU 주변장치 설정

> 근거: [.ioc](../p116.103RB_TM1637_BH1750_BMP280_SPI.ioc), [main.c](../Core/Src/main.c) (`MX_*_Init`, `SystemClock_Config`), [stm32f1xx_hal_msp.c](../Core/Src/stm32f1xx_hal_msp.c), HAL 소스(`Drivers/STM32F1xx_HAL_Driver`).
>  **RCC, GPIO, I2C1, SPI1, USART2**.

## 0. 초기화 순서 (`main()`)

```
HAL_Init()                  → Flash prefetch, NVIC 그룹, SysTick(1 ms), HAL_MspInit()
SystemClock_Config()        → HSI/2 ×16 PLL → SYSCLK 64 MHz
MX_GPIO_Init()              → GPIOA/GPIOB 클럭, CS/CLK/DIO 설정
MX_USART2_UART_Init()       → HAL_UART_Init → HAL_UART_MspInit (클럭 + PA2/PA3)
MX_I2C1_Init()              → HAL_I2C_Init  → HAL_I2C_MspInit  (클럭 + PB6/PB7)
MX_SPI1_Init()              → HAL_SPI_Init  → HAL_SPI_MspInit  (클럭 + PA5/PA6/PA7)
```

## 1. RCC / Clock

| 항목 | 값 |
|---|---|
| 클럭 소스 | HSI (내부 8 MHz) |
| PLL 입력 | HSI/2 = 4 MHz (`RCC_PLLSOURCE_HSI_DIV2`) |
| PLL 배수 | ×16 |
| **SYSCLK / HCLK** | **64 MHz** |
| APB1 (PCLK1) | 32 MHz (÷2) | 
| APB2 (PCLK2) | 64 MHz (÷1) |

**기술적 선택 이유 (추정 포함)**
- HSI 경로는 PLL 입력이 HSI/2 = 4 MHz로 고정되고 F103 HAL의 PLL 배수 상수는 최대 `RCC_PLL_MUL16`이므로, **HSE 없이 얻을 수 있는 최대 SYSCLK가 64 MHz**입니다 (72 MHz는 8 MHz HSE × 9 경로가 필요 — 일반 지식, 본 프로젝트는 HSE 미사용).
- F1은 APB1 최대 36 MHz 제한이 있어 ÷2 (32 MHz)가 필요합니다.

## 2. SysTick / NVIC

| 항목 | 값 |
|---|---|
| 틱 주기 | 1 ms (`HAL_TICK_FREQ_DEFAULT`) |
| SysTick 리로드 | `SystemCoreClock / 1000` = **64000** |
| 틱 우선순위 | 15 (최하위) |
| 우선순위 그룹 | `NVIC_PRIORITYGROUP_4` (4-bit preemption) |
| 핸들러 | `SysTick_Handler → HAL_IncTick()` |
`HAL_GetTick()`은 이 틱으로 증가하는 `uwTick`을 읽으며, 메인 루프 스케줄링(100 ms/500 ms)과 모든 HAL timeout의 기준입니다.

## 3. GPIO

| 핀 | CubeMX 설정 | 생성 코드 |
|---|---|---|
| PA4 `BMP280_CS` | Output, 초기 High, Speed High | `GPIO_MODE_OUTPUT_PP`, `NOPULL` |
| PB8 `TM1637_CLK` | Output, 초기 High, Speed High | `GPIO_MODE_OUTPUT_PP`, `NOPULL` |
| PB9 `TM1637_DIO` | Output **Open-Drain**, Pull-up, 초기 High | `GPIO_MODE_OUTPUT_OD`, `GPIO_PULLUP` |

```c
// main.c : MX_GPIO_Init (CubeMX 생성)
HAL_GPIO_WritePin(BMP280_CS_GPIO_Port, BMP280_CS_Pin, GPIO_PIN_SET);              // CS 비선택 상태 먼저 확보
HAL_GPIO_WritePin(GPIOB, TM1637_CLK_Pin|TM1637_DIO_Pin, GPIO_PIN_SET);
...
GPIO_InitStruct.Pin  = TM1637_DIO_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
GPIO_InitStruct.Pull = GPIO_PULLUP;
```

- **왜 DIO만 Open-Drain인가**: ACK 비트에서 TM1637이 DIO를 구동하므로 MCU는 라인을 놓아야 함 .
- 핀 속도 `GPIO_SPEED_FREQ_HIGH`는 F1에서 MODE=11(최대 50 MHz 슬루) 설정입니다.

## 4. I2C1 (BH1750)

| 항목 | 값 | 선택 이유 |
|---|---|---|
| 모드 | Standard, `ClockSpeed = 100000` | BH1750 가 지원하는 표준 속도 |
| Duty | `I2C_DUTYCYCLE_2` | Fast mode 전용 파라미터 (Standard에서는 CCR 계산에 영향 없음: `I2C_SPEED` 매크로) |
| 주소 모드 | 7-bit, OwnAddress 0 | 마스터 전용 사용 |
| Dual / General call | Disable | 단일 마스터 |
| 핀 | PB6(SCL), PB7(SDA) **AF Open-Drain**, High speed | `HAL_I2C_MspInit` |
| 클럭 | `__HAL_RCC_I2C1_CLK_ENABLE()` (APB1) | |

## 5. SPI1 (BMP280)

| 항목 | 값 | 선택 이유 |
|---|---|---|
| Mode | Master, Full-Duplex 2-line | BMP280 4-wire SPI |
| DataSize | 8-bit, MSB first | BMP280 레지스터 프레임 |
| **CPOL / CPHA** | **High / 2nd edge → SPI Mode 3** | BMP280은 Mode3(계속 측정 모드) |
| NSS | `SPI_NSS_SOFT` + GPIO `PA4` |  |
| Prescaler | **÷8** | 64 MHz(APB2)/8 = **8 MHz** (`.ioc`: 8.0 MBits/s) |
| CRC / TI mode | Disable | |
| 핀 | PA5, PA7 AF Push-Pull / PA6 Input | `HAL_SPI_MspInit` |
| 클럭 | `__HAL_RCC_SPI1_CLK_ENABLE()` (APB2) | |

## 6. USART2 (PC 로그)

| 항목 | 값 |
|---|---|
| Baud | 115200, 8N1, 흐름제어 없음, Oversampling ×16, Mode TX/RX |
| 핀 | PA2 AF-PP (TX), PA3 Input (RX) |
| 버스 | APB1 (32 MHz) — `UART_SetConfig`가 USART1만 PCLK2, 그 외는 PCLK1 사용 (`stm32f1xx_hal_uart.c:3735~3742`에서 확인) |

**보레이트 계산 (HAL 매크로 `stm32f1xx_hal_uart.h:866~874`, PCLK1 = 32 MHz 가정)**

```
DIV       = (32,000,000 × 25) / (4 × 115200) = 1736      (×100 스케일)
DIVMANT   = 1736 / 100 = 17
DIVFRAQ   = ((1736 − 1700) × 16 + 50) / 100 = 6
BRR       = (17 << 4) | 6 = 0x0116
실제 baud = 32 MHz / (16 × 17.375) ≈ 115,108 bps   (오차 ≈ −0.08 %)
```
