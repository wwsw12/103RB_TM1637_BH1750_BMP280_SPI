# 03. STM32CubeMX 및 MCU 주변장치 설정

> 근거: [.ioc](../p116.103RB_TM1637_BH1750_BMP280_SPI.ioc), [main.c](../Core/Src/main.c) (`MX_*_Init`, `SystemClock_Config`), [stm32f1xx_hal_msp.c](../Core/Src/stm32f1xx_hal_msp.c), HAL 소스(`Drivers/STM32F1xx_HAL_Driver`).
> 레지스터 설명은 **RM0008 (STM32F101xx/102xx/103xx 등 Reference Manual)** 의 해당 주변장치 장(RCC, GPIO, I2C, SPI, USART)을 기준으로 하되, 페이지 번호는 확인하지 않아 기재하지 않았습니다.
> 사용한 주변장치만 문서화합니다: **RCC, GPIO, I2C1, SPI1, USART2, SysTick/NVIC**. (ADC, Timer, DMA 미사용)

## 0. 초기화 순서 (`main()`)

```
HAL_Init()                  → Flash prefetch, NVIC 그룹, SysTick(1 ms), HAL_MspInit()
SystemClock_Config()        → HSI/2 ×16 PLL → SYSCLK 64 MHz
MX_GPIO_Init()              → GPIOA/GPIOB 클럭, CS/CLK/DIO 설정
MX_USART2_UART_Init()       → HAL_UART_Init → HAL_UART_MspInit (클럭 + PA2/PA3)
MX_I2C1_Init()              → HAL_I2C_Init  → HAL_I2C_MspInit  (클럭 + PB6/PB7)
MX_SPI1_Init()              → HAL_SPI_Init  → HAL_SPI_MspInit  (클럭 + PA5/PA6/PA7)
```

`HAL_xxx_Init()`이 내부에서 **`HAL_xxx_MspInit()`을 호출**하여 클럭과 핀을 설정하는 구조입니다 (`stm32f1xx_hal_i2c.c:463` 부근 `HAL_I2C_MspInit(hi2c)` 호출 확인).

## 1. RCC / Clock

| 항목 | 값 | 근거 |
|---|---|---|
| 클럭 소스 | HSI (내부 8 MHz) | `RCC_OSCILLATORTYPE_HSI`, HSE 미사용 |
| PLL 입력 | HSI/2 = 4 MHz (`RCC_PLLSOURCE_HSI_DIV2`) | F1 하드웨어 제약 |
| PLL 배수 | ×16 | `RCC_PLL_MUL16` |
| **SYSCLK / HCLK** | **64 MHz** | 4 MHz × 16 |
| APB1 (PCLK1) | 32 MHz (÷2) | `RCC_HCLK_DIV2` |
| APB2 (PCLK2) | 64 MHz (÷1) | `RCC_HCLK_DIV1` |
| Flash Latency | 2 WS | `FLASH_LATENCY_2` |

**기술적 선택 이유 (추정 포함)**
- HSI 경로는 PLL 입력이 HSI/2 = 4 MHz로 고정되고 F103 HAL의 PLL 배수 상수는 최대 `RCC_PLL_MUL16`이므로, **HSE 없이 얻을 수 있는 최대 SYSCLK가 64 MHz**입니다 (72 MHz는 8 MHz HSE × 9 경로가 필요 — 일반 지식, 본 프로젝트는 HSE 미사용). 이 선택이 의도된 것인지(보드에 크리스털 유무 등)는 소스에 기록되어 있지 않음.
- F1은 APB1 최대 36 MHz 제한이 있어 ÷2 (32 MHz)가 필요합니다 (RM0008 Clock 절 — 상세 수치는 해당 문서에서 확인 필요).
- Flash 대기 상태: 48 MHz 초과 ~72 MHz 이하는 2 WS가 필요한 것으로 알려져 있어 `FLASH_LATENCY_2` 사용 (RM0008/Datasheet 확인 필요).
- HSI 사용으로 외부 크리스털 정확도 이점은 없음 → UART 보레이트 오차가 크리스털 대비 클 수 있음 (실측 필요, 아래 UART 절).

관련 레지스터: `RCC_CR`(HSION/PLLON), `RCC_CFGR`(PLLSRC, PLLMUL, HPRE, PPRE1/2, SW), `FLASH_ACR`(LATENCY).

## 2. SysTick / NVIC

| 항목 | 값 | 근거 |
|---|---|---|
| 틱 주기 | 1 ms (`HAL_TICK_FREQ_DEFAULT`) | `stm32f1xx_hal.c:81` |
| SysTick 리로드 | `SystemCoreClock / 1000` = **64000** | `HAL_InitTick` (`stm32f1xx_hal.c:237`) |
| 틱 우선순위 | 15 (최하위) | `TICK_INT_PRIORITY 15U`, `.ioc NVIC.SysTick_IRQn` |
| 우선순위 그룹 | `NVIC_PRIORITYGROUP_4` (4-bit preemption) | `.ioc NVIC.PriorityGroup` |
| 핸들러 | `SysTick_Handler → HAL_IncTick()` | `stm32f1xx_it.c:183` |

`HAL_GetTick()`은 이 틱으로 증가하는 `uwTick`을 읽으며, 메인 루프 스케줄링(100 ms/500 ms)과 모든 HAL timeout의 기준입니다.
**주변장치 인터럽트(NVIC)는 활성화되지 않았습니다.**

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

- **왜 DIO만 Open-Drain인가**: ACK 비트에서 TM1637이 DIO를 구동하므로 MCU는 라인을 놓아야 함 (일반 원리). `.ioc`의 `PB9.GPIO_ModeDefaultOutputPP=GPIO_MODE_OUTPUT_OD`는 개발자가 CubeMX에서 모드를 변경했음을 보여줍니다.
- **F1 GPIO 특성**: 핀 모드는 `GPIOx_CRL`(핀 0~7: PA4) / `GPIOx_CRH`(핀 8~15: PB8, PB9)의 CNF/MODE 비트로 설정됩니다 (`HAL_GPIO_Init`, `stm32f1xx_hal_gpio.c:178`). 출력은 `GPIOx_BSRR`로 원자적 Set/Reset, 입력은 `GPIOx_IDR` 읽기입니다 (`HAL_GPIO_WritePin` :465, `HAL_GPIO_ReadPin` :431에서 확인).
- 핀 속도 `GPIO_SPEED_FREQ_HIGH`는 F1에서 MODE=11(최대 50 MHz 슬루) 설정입니다. 실제 TM1637 통신은 소프트웨어 지연으로 수십 kHz 이하이므로 속도 설정은 신호 품질(에지 속도)에만 영향.

## 4. I2C1 (BH1750)

| 항목 | 값 | 선택 이유 |
|---|---|---|
| 모드 | Standard, `ClockSpeed = 100000` | BH1750 가 지원하는 표준 속도 (센서 지원 속도는 데이터시트 확인 필요) |
| Duty | `I2C_DUTYCYCLE_2` | Fast mode 전용 파라미터 (Standard에서는 CCR 계산에 영향 없음: `I2C_SPEED` 매크로) |
| 주소 모드 | 7-bit, OwnAddress 0 | 마스터 전용 사용 |
| Dual / General call | Disable | 단일 마스터 |
| Clock stretching | Enable (`NOSTRETCH_DISABLE`) | 기본값 |
| 핀 | PB6(SCL), PB7(SDA) **AF Open-Drain**, High speed | `HAL_I2C_MspInit` |
| 클럭 | `__HAL_RCC_I2C1_CLK_ENABLE()` (APB1) | |

**HAL이 계산하는 레지스터 값 (PCLK1 = 32 MHz, 소스의 매크로로 계산)**

| 레지스터 | 계산식 (`stm32f1xx_hal_i2c.h:657~664`) | 값 |
|---|---|---|
| `I2C_CR2.FREQ` | `PCLK1 / 1 MHz` | **32** |
| `I2C_TRISE` | Standard: `FREQ + 1` | **33** |
| `I2C_CCR` | `((PCLK1−1)/(100000×2))+1` | **160** (Standard, FS=0) |
| SCL 주파수 | `PCLK1 / (2 × CCR)` = 32 MHz / 320 | **100 kHz** |

초기화 코드(`HAL_I2C_Init`, `stm32f1xx_hal_i2c.c:463~`): PE=0 → `SWRST` 토글 → `CR2/TRISE/CCR/CR1/OAR1/OAR2` 설정 → PE=1.

**영향**: SCL 100 kHz → 1바이트(8 bit + ACK = 9 clk) ≈ 90 µs. BH1750 한 번 읽기(주소 1B + 데이터 2B + Start/Stop)는 대략 0.3 ms 안팎 (이론값, 실측 없음).

## 5. SPI1 (BMP280)

| 항목 | 값 | 선택 이유 |
|---|---|---|
| Mode | Master, Full-Duplex 2-line | BMP280 4-wire SPI |
| DataSize | 8-bit, MSB first | BMP280 레지스터 프레임 |
| **CPOL / CPHA** | **High / 2nd edge → SPI Mode 3** | BMP280은 Mode 0/3 지원 (데이터시트 확인 필요) |
| NSS | `SPI_NSS_SOFT` + GPIO `PA4` | 읽기마다 CS를 직접 제어하여 멀티 바이트 트랜잭션 보장 |
| Prescaler | **÷8** | 64 MHz(APB2)/8 = **8 MHz** (`.ioc`: 8.0 MBits/s) |
| CRC / TI mode | Disable | |
| 핀 | PA5, PA7 AF Push-Pull / PA6 Input | `HAL_SPI_MspInit` |
| 클럭 | `__HAL_RCC_SPI1_CLK_ENABLE()` (APB2) | |

관련 레지스터: `SPI_CR1` (MSTR, BR[2:0]=010, CPOL, CPHA, SSM/SSI, SPE), `SPI_SR`(TXE/RXNE/BSY), `SPI_DR`.

**영향**: 8 MHz → 1바이트 = 1 µs. BMP280 `ReadData`(주소 1B + 데이터 6B) 의 SPI 전송 시간은 이론상 약 7 µs + HAL 오버헤드. 8 MHz가 BMP280 최대 SPI 클럭(데이터시트 기재값)을 넘지 않는지는 **데이터시트 확인 필요**.

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
> 위 값은 매크로를 직접 계산한 **이론값**이며 실측하지 않았습니다. HSI는 크리스털보다 부정확하므로 실제 오차는 HSI 편차를 더해 달라질 수 있습니다.

관련 레지스터: `USART_BRR`, `USART_CR1`(UE, TE, RE, M, PCE), `USART_SR`(TXE, TC), `USART_DR`.

## 7. 실제로 사용하지 않은 항목 (문서화 생략)

ADC, Timer(TIMx), DMA, EXTI, 주변장치 IRQ — `.ioc`에 설정 없음.

## 8. 빌드 설정 (프로젝트 파일 기준)

| 항목 | 값 | 근거 |
|---|---|---|
| 링커 스크립트 | `STM32F103RBTX_FLASH.ld` (Flash 128 KB @0x08000000, RAM 20 KB @0x20000000, Heap 0x200, Stack 0x400) | `.ld` |
| 최적화 | Debug 구성 `-O0 -g3` | `Debug/Core/Src/subdir.mk` |
| printf float | 활성 (`nanoprintffloat=true`) | `.cproject:26` |
| C 라이브러리 | newlib-nano (`--specs=nano.specs`), soft float | `subdir.mk` |
| HAL assert | 비활성 (`USE_FULL_ASSERT` 주석 처리) | `stm32f1xx_hal_conf.h:165` |
