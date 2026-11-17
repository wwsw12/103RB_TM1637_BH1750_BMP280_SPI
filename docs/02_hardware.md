# 02. 하드웨어 회로 구성 및 인터페이스 분석

> 이 문서는 **회로도([hardware_schematic.png](images/hardware_schematic.png))**, **소스/`.ioc`**, **실제 사용 모듈 정보**를 근거로 작성했습니다.
> 근거 수준은 다음과 같이 구분합니다.
> - **확인** — 회로도·코드·실물 사진·개발자 확인으로 확정된 사항
> - **데이터시트** — 부품 데이터시트에서 쪽수까지 확인한 사항 (쪽수는 PDF 뷰어 기준)
> - **레퍼런스 매뉴얼** — STM32F10x 레퍼런스 매뉴얼(RM0008)에서 쪽수까지 확인한 사항
> - **일반 사양** — 해당 모듈/IC의 일반적인 구성 (데이터시트 미보유 또는 미확인)
> - **확인 필요** — 측정 또는 실물 확인이 필요한 사항
>
> 참고 문서: STM32F103x8/xB 데이터시트(DS5319), STM32F10x 레퍼런스 매뉴얼(RM0008), BMP280 데이터시트(Bosch), BH1750FVI 데이터시트(ROHM), TM1637 데이터시트 V2.1(Titan Micro)

![STM32F103RB 하드웨어 연결 회로도](images/hardware_schematic.png)

### 실제 배선 및 모듈 사진

![전체 배선 — NUCLEO-F103RB, 브레드보드, 센서 모듈, TM1637 표시기(기압 1018 hPa 표시 중)](images/wiring_overview.jpg)

| TM1637 4자리 모듈 | GY-302 (BH1750) | GY-BMP280 |
|---|---|---|
| ![TM1637 모듈](images/module_tm1637.jpg) | ![GY-302 모듈](images/module_gy302_bh1750.jpg) | ![GY-BMP280 모듈](images/module_gy_bmp280.jpg) |
| 핀: CLK / DIO / GND / 5V | 핀: VCC / GND / SCL / SDA / ADDR (5핀 모두 결선) | 핀: VCC / GND / SCL / SDA / CSB / SDO |

## 1. 구성 요소와 역할

| 장치 | 사용 제품 | 전원 (회로도) | 역할 |
|---|---|---|---|
| STM32F103RBT6 | NUCLEO-F103RB 보드 | USB (ST-LINK) | 마스터 MCU |
| BH1750 | GY-302 모듈 | +3V3 | 조도 센서, I2C 7-bit 주소 0x23 (ADDR → GND) |
| BMP280 | GY-BMP280 6핀 모듈 (VCC/GND/SCL/SDA/CSB/SDO) | +3V3 | 기압/온도 센서, SPI 접속 |
| TM1637 | 4자리 7-segment 모듈 (DIY MORE "4-Digit LED Display") | **+5V** (NUCLEO 보드 5V 핀) | 7-segment LED 드라이버 |
| PC 로그 | NUCLEO 내장 ST-LINK 가상 COM 포트 | — | USART2 출력 (Tera Term) |

## 2. MCU 핀 할당표 (`.ioc` + `main.h` + `stm32f1xx_hal_msp.c` + 회로도)

| 핀 | 기능 | GPIO 모드 (생성 코드) | 초기 레벨 | 연결 대상 (회로도) |
|---|---|---|---|---|
| PA2 | USART2_TX | AF Push-Pull | — | ST-LINK VCP RX |
| PA3 | USART2_RX | Input (No pull) | — | ST-LINK VCP TX (코드상 수신 미사용) |
| PA4 | `BMP280_CS` | Output PP, High speed | High | J2-5 CSB |
| PA5 | SPI1_SCK | AF Push-Pull | — | J2-3 SCL(SCK) |
| PA6 | SPI1_MISO | Input (No pull) | — | J2-6 SDO |
| PA7 | SPI1_MOSI | AF Push-Pull | — | J2-4 SDA(SDI) |
| PB6 | I2C1_SCL | AF Open-Drain | — | J1-3 SCL |
| PB7 | I2C1_SDA | AF Open-Drain | — | J1-4 SDA |
| PB8 | `TM1637_CLK` | Output Open-Drain, High speed | High | J3-3 CLK |
| PB9 | `TM1637_DIO` | Output Open-Drain, High speed (`GPIO_PULLUP` 지정했지만 F1 출력 모드에서는 무시됨 — 4-1절) | High | J3-4 DIO |
| PA13 / PA14 | SWDIO / SWCLK | Serial Wire | — | 온보드 ST-LINK |
| BOOT0 | — | — | — | 10 kΩ 풀다운 (Flash 부팅) |
| NRST | — | — | — | 10 kΩ 풀업 + 리셋 버튼(GND) |

> BMP280 모듈의 핀 이름은 I2C 기준(SCL/SDA)으로 인쇄되어 있지만, SPI 모드에서는 **SCL=SCK, SDA=SDI(MOSI), SDO=MISO, CSB=CS**로 사용합니다.

## 3. 인터페이스별 신호선과 데이터 방향

### 3-1. I2C1 — BH1750 (GY-302)

| 신호 | 방향 | 비고 |
|---|---|---|
| SCL (PB6) | MCU → 센서 (마스터 클럭) | 100 kHz (Standard mode) |
| SDA (PB7) | 양방향 | Open-Drain |
| ADDR | — | **GND 연결 (확인)** → 7-bit 주소 0x23 |

- **데이터시트** (BH1750FVI p.7): ADDR = 'L'일 때 슬레이브 주소는 `0100011`(= 0x23)입니다.
- 주소 바이트: 7-bit `0x23` → HAL 규약상 `0x23 << 1 = 0x46` 전달 (HAL이 R/W 비트를 OR).
- 통신: `Power On(0x01)` → `Continuous H-Res Mode(0x10)` 전송 후, 매 읽기마다 2바이트 수신.
- **데이터시트** (BH1750FVI p.2): I2C SCL 최대 400 kHz → 100 kHz 설정은 범위 안. H-Resolution 측정 시간은 typ 120 ms / max 180 ms → 조도는 max보다 긴 **200 ms 주기**로 읽습니다 (`main.c` `BH1750_READ_PERIOD_MS`, [05_troubleshooting.md](05_troubleshooting.md) 4장 5번).
- ADDR 연결은 회로도의 J1(4핀)에는 그려져 있지 않습니다. 회로도 갱신 시 ADDR → GND를 추가하는 것을 권장합니다.

### 3-2. SPI1 — BMP280 (GY-BMP280)

| 신호 | 방향 | 비고 |
|---|---|---|
| SCK (PA5) | MCU → 센서 | 64 MHz / 8 = 8 MHz, **CPOL=1, CPHA=1 (Mode 3)** |
| MOSI (PA7) | MCU → 센서 | 4-wire(Full duplex) 구성 |
| MISO (PA6) | 센서 → MCU | MISO를 별도 입력으로 사용 (3-wire 아님) |
| CS (PA4) | MCU → 센서 | **소프트웨어 CS**, Low = 선택 (`SPI_NSS_SOFT`) |

- **데이터시트** (BMP280 p.2, 5.3절 p.30): SPI는 최대 10 MHz, Mode '00'과 '11'을 지원합니다. 두 모드는 CSB 하강 직후의 SCK 레벨로 자동 구분됩니다. 따라서 Mode 3 / 8 MHz 설정은 사양 범위 안입니다.
- **데이터시트** (BMP280 5.1절 p.28): CSB가 한 번이라도 Low가 되면 SPI가 선택되고, 다음 전원 리셋까지 I2C는 꺼집니다. 그래서 부팅 때 CS를 High로 두었다가 첫 트랜잭션에서 Low로 내리는 현재 구조로 SPI 모드에 진입합니다.
- **데이터시트** (BMP280 p.31): CSB는 Active Low이고 칩 내부에 풀업이 있습니다. SDI는 SCK 상승 에지에서 읽히고 SDO는 하강 에지에서 바뀝니다.

### 3-3. TM1637 — GPIO 2선

| 신호 | 방향 | 비고 |
|---|---|---|
| CLK (PB8) | MCU → TM1637 | Open-Drain (High는 모듈 풀업) |
| DIO (PB9) | 양방향 | Open-Drain 출력. High는 모듈의 외부 풀업이 만듦. ACK 구간에 High 해제 후 `HAL_GPIO_ReadPin` |

TM1637의 2선 프로토콜은 I2C와 비슷한 Start/Stop/ACK 형태이나 **I2C 주소 체계가 없고 LSB-first**이므로 I2C 주변장치로 구동하지 않고 GPIO로 구현했습니다 (`tm1637.c`).

- **데이터시트** (TM1637 p.2): DIO 데이터는 CLK가 Low일 때 바뀌고 High일 때 전달됩니다. 바이트마다 8번째 클럭 하강 에지에서 칩이 ACK(DIO Low)를 냅니다. VDD는 5 V ± 10%입니다.
- **데이터시트** (TM1637 p.3, p.10 예제 코드): 데이터는 하위 비트부터(LSB-first) 전송합니다.
- **데이터시트** (TM1637 p.13 응용 회로): CLK/DIO에 **10 kΩ 풀업(VCC)**과 **100 pF(101) 커패시터**를 권장합니다.

### 3-4. USART2 — ST-LINK 가상 COM 포트

| 항목 | 내용 |
|---|---|
| 설정 | 115200 bps, 8N1, 흐름 제어 없음 |
| 경로 | PA2/PA3 → NUCLEO 보드의 ST-LINK → USB 가상 COM 포트 → PC (Tera Term) |
| 방향 | MCU → PC 출력만 사용 (`printf` → `__io_putchar` → `HAL_UART_Transmit`) |

- 별도 USB-UART 모듈 없이 **디버깅용 USB 케이블 하나로 로그까지 받는 구성**입니다.
- 회로도의 J4 `UART_3V3` 헤더는 이 USART2 신호선을 표현한 것으로, 외부 모듈을 연결하지 않았습니다.
- **일반 사양**: NUCLEO-64 보드는 기본 설정(솔더 브리지 SB13/SB14 ON)에서 PA2/PA3가 ST-LINK에 연결되어 있습니다.

## 4. 전기적 특성

| 항목 | 값 | 근거 |
|---|---|---|
| 시스템 클럭 | 64 MHz | `.ioc` RCC — **확인** |
| GPIO 출력 속도 | HIGH (HAL: 50 MHz 모드) | `GPIO_SPEED_FREQ_HIGH` — **확인** |
| I2C 속도 | 100 kHz | `hi2c1.Init.ClockSpeed` — **확인** |
| SPI 속도 | 8 Mbit/s | `.ioc` `SPI1.CalculateBaudRate` — **확인** |
| BH1750 / BMP280 전원 | 3.3 V | 회로도 J1-1, J2-1 — **확인** |
| TM1637 전원 | 5 V (NUCLEO 보드 5V 핀에서 공급) | 회로도 J3-1, 개발자 확인 — **확인** |
| MCU ↔ 센서 로직 레벨 | 3.3 V ↔ 3.3 V | 같은 전원 레일 사용 — **확인** |
| I2C 풀업 저항 | GY-302 모듈 내장 풀업 사용 (MCU 내부 풀업 미설정) | 모듈 구성은 **일반 사양**, 저항값은 **확인 필요** |
| TM1637 모듈 RC 필터 | CLK/DIO 라인에 풀업 저항 + 소용량 커패시터 | **모듈 회로도** (CATALEX 4-Digit Display v0.9b, `datasheet/SZH-LD5112/4-Digit Display v0.9b schematic.pdf`): CLK·DIO 각각 R1/R2 **10 kΩ → VCC**, C3/C4 **100 pF → GND**. 칩 **데이터시트** 권장 회로(TM1637 p.13)와 같은 값. 시정수 τ = 1 µs, 5τ = 5 µs → `tm1637.c`의 25 µs 지연은 충분히 김 — **확인** (모듈 회로도 기준) |

### 4-1. TM1637 5 V 구동과 3.3 V MCU의 레벨 차이

TM1637만 5 V로 동작하고 MCU는 3.3 V이므로, 이 구간은 레벨 호환성을 확인해야 합니다.

| 항목 | 내용 |
|---|---|
| MCU 핀 내압 | **데이터시트** (STM32F103 핀 정의 표, PDF 33쪽): PB8/PB9는 `I/O FT`(5 V tolerant) 핀입니다. |
| FT 핀 사용 조건 | **데이터시트** (STM32F103 I/O 정적 특성 표 주석 3, PDF 61쪽): FT 핀에 VDD + 0.3 V보다 높은 전압을 걸려면 **내부 풀업/풀다운을 꺼야** 합니다. |
| PB9 내부 풀업 실제 상태 | **레퍼런스 매뉴얼** (RM0008 Table 20 p.161, 9.1.8절 p.163): STM32F1은 **출력 모드에서 내부 풀업/풀다운이 꺼집니다.** 풀업은 입력 모드에서만 쓸 수 있습니다. HAL도 출력 모드에서는 `Pull` 값을 쓰지 않습니다 (`stm32f1xx_hal_gpio.c` `case GPIO_MODE_OUTPUT_OD`). 따라서 코드의 `GPIO_PULLUP`은 **실제로는 적용되지 않고**, 위 FT 조건도 만족합니다. |
| DIO (Open-Drain) | MCU 내부 풀업이 없으므로 DIO의 High는 **모듈의 외부 풀업(5 V)**이 만듭니다. High 레벨이 약 5 V라 V<sub>IH</sub>를 만족합니다. 외부 풀업이 없었다면 DIO가 떠서 동작하지 않았을 것이므로, 표시가 정상(`1018`)인 것도, 모듈 회로도의 R2 10 kΩ 풀업과 일치합니다. |
| TM1637 입력 기준 | **데이터시트** (TM1637 p.14 정상 동작 범위, p.15 전기적 특성): CLK/DIN의 V<sub>IH</sub> = 0.7 × VDD (최소), 히스테리시스 0.35 V. VDD 5 V면 **3.5 V**, VDD 4.5 V(−10%)면 3.15 V. |
| CLK (수정 전: Push-Pull) | MCU가 High를 3.3 V로 구동하므로 VDD 5 V 기준 V<sub>IH</sub>(3.5 V)보다 **0.2 V 낮습니다.** 또 모듈 풀업(5 V)에서 MCU 출력(3.3 V)으로 작은 전류(10 kΩ 기준 약 0.17 mA)가 흐릅니다. |
| 적용: CLK Open-Drain | CLK(PB8)도 DIO처럼 **Open-Drain**으로 변경했습니다. High는 모듈 회로도의 R1 10 kΩ 풀업이 5 V로 만들어 V<sub>IH</sub>(3.5 V)를 만족하고, 풀업 → MCU로 흐르던 전류도 없어집니다. 상승 시간 5τ = 5 µs로 25 µs 지연 안에 충분히 올라갑니다. PB8은 FT 핀이고 출력 모드라 내부 풀업이 꺼져 있어 FT 사용 조건도 만족합니다. (다른 방법인 TM1637 3.3 V 구동은 데이터시트 VDD 범위 5 V ± 10% 밖이라 제외) |

> Push-Pull 구성에서도 동작은 했지만(배선 사진의 `1018` 표시) CLK High 레벨은 사양 여유가 없는 구간이었습니다.
> - NUCLEO 5V 핀의 실제 전압은 USB 전원에 따라 5 V보다 조금 낮을 수 있고, 그만큼 V<sub>IH</sub>도 낮아집니다. 멀티미터로 5V 핀 전압을 재 두면 계산 근거가 됩니다.
> - 이 항목은 [05_troubleshooting.md](05_troubleshooting.md) 4장 6번에 정리했고, Open-Drain으로 수정했습니다. 수정 후에도 7-segment 표시가 정상 동작함을 보드에서 확인했습니다.

## 5. 부품 사용 이유 (일반 원리 vs 프로젝트 확인)

| 구성 | 프로젝트에서 확인된 것 | 일반 설계 원리 (참고) |
|---|---|---|
| I2C 풀업 | 핀이 Open-Drain으로 설정됨, MCU 내부 풀업 미사용 | Open-Drain 버스는 High 레벨을 풀업 저항이 만들어야 함. GY-302 모듈에 내장된 풀업을 사용 |
| TM1637 DIO Open-Drain | `GPIO_MODE_OUTPUT_OD` (CubeMX 기본 PP를 변경: `.ioc` `PB9.GPIO_ModeDefaultOutputPP=GPIO_MODE_OUTPUT_OD`). `GPIO_PULLUP`도 지정했지만 F1 출력 모드에서는 무시됨 | ACK 시 TM1637이 DIO를 Low로 당기므로 MCU가 Push-Pull로 High를 구동하면 충돌 가능. OD로 두면 해제(High-Z) 후 읽기 가능. High는 모듈의 5 V 풀업이 만들어 TM1637 기준을 만족 |
| TM1637 CLK Open-Drain | `GPIO_MODE_OUTPUT_OD` (`.ioc` `PB8.GPIO_ModeDefaultOutputPP=GPIO_MODE_OUTPUT_OD`) | High를 모듈의 5 V 풀업이 만들어 TM1637 V<sub>IH</sub>(0.7 × VDD = 3.5 V)를 만족 (4-1절) |
| BMP280 CS 초기 High | `MX_GPIO_Init`에서 `HAL_GPIO_WritePin(CS, SET)`을 핀 모드 설정 **이전**에 수행 | 출력 활성화 순간 CS가 Low로 글리치 되는 것을 방지하는 효과 |
| 센서 3.3 V / 표시기 5 V 분리 | 회로도 전원 레일 | 센서는 MCU와 같은 3.3 V로 레벨 변환 없이 연결, TM1637은 LED 밝기를 위해 5 V 사용 |
| ST-LINK VCP 사용 | USART2 = PA2/PA3 | 추가 부품 없이 USB 하나로 디버깅·전원·로그를 모두 처리 |

## 6. 외부 회로 검증 결과

> 실제 보드 동작 결과(Tera Term 로그, 7-segment 표시)는 [06_testing_and_results.md](06_testing_and_results.md)에, 파형·전압 측정이 필요한 항목(TM1637 CLK High 레벨 등)은 [05_troubleshooting.md](05_troubleshooting.md)에 정리합니다.

- [x] BH1750 / BMP280 동작 전압 3.3 V, TM1637 5 V (회로도로 확인)
- [x] BH1750 ADDR 핀 → GND, 주소 0x23 (개발자 확인)
- [x] BMP280 모듈을 SPI 모드로 결선 (회로도 J2: CSB → PA4, SDO → MISO)
- [x] PC 로그 경로: ST-LINK 가상 COM 포트
- [x] BMP280 SPI Mode 3 / 8 MHz가 사양 범위 안 (BMP280 데이터시트)
- [x] PB8/PB9가 5 V tolerant 핀 (STM32F103 데이터시트)
- [x] PB9를 5 V 풀업에 연결해도 FT 조건을 만족 (F1 출력 모드는 내부 풀업 OFF — RM0008)
- [x] TM1637 V<sub>IH</sub> = 0.7 × VDD (TM1637 데이터시트)
