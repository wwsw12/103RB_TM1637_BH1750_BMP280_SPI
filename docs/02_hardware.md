# 02. 하드웨어 회로 구성 및 인터페이스 분석

> 이 문서는 **회로도([hardware_schematic.png](images/hardware_schematic.png))**, **소스/`.ioc`**, **실제 사용 모듈 정보**를 근거로 작성했습니다.
> 근거 수준은 다음과 같이 구분합니다.
> - **확인** — 회로도·코드·개발자 확인으로 확정된 사항
> - **일반 사양** — 해당 모듈/IC의 일반적인 구성 또는 데이터시트 내용 (실물·문서 페이지로 재확인 권장)
> - **확인 필요** — 측정 또는 실물 확인이 필요한 사항

![STM32F103RB 하드웨어 연결 회로도](images/hardware_schematic.png)

## 1. 구성 요소와 역할

| 장치 | 사용 제품 | 전원 (회로도) | 역할 |
|---|---|---|---|
| STM32F103RBT6 | NUCLEO-F103RB 보드 | USB (ST-LINK) | 마스터 MCU |
| BH1750 | GY-302 모듈 | +3V3 | 조도 센서, I2C 7-bit 주소 0x23 (ADDR → GND) |
| BMP280 | GY-BMP280 6핀 모듈 (VCC/GND/SCL/SDA/CSB/SDO) | +3V3 | 기압/온도 센서, SPI 접속 |
| TM1637 | 4자리 7-segment 모듈 | **+5V** (NUCLEO 보드 5V 핀) | 7-segment LED 드라이버 |
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
| PB8 | `TM1637_CLK` | Output Push-Pull, High speed | High | J3-3 CLK |
| PB9 | `TM1637_DIO` | Output Open-Drain + Pull-up, High speed | High | J3-4 DIO |
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

- 주소 바이트: 7-bit `0x23` → HAL 규약상 `0x23 << 1 = 0x46` 전달 (HAL이 R/W 비트를 OR).
- 통신: `Power On(0x01)` → `Continuous H-Res Mode(0x10)` 전송 후, 매 읽기마다 2바이트 수신.
- ADDR 연결은 회로도의 J1(4핀)에는 그려져 있지 않습니다. 회로도 갱신 시 ADDR → GND를 추가하는 것을 권장합니다.

### 3-2. SPI1 — BMP280 (GY-BMP280)

| 신호 | 방향 | 비고 |
|---|---|---|
| SCK (PA5) | MCU → 센서 | 64 MHz / 8 = 8 MHz, **CPOL=1, CPHA=1 (Mode 3)** |
| MOSI (PA7) | MCU → 센서 | 4-wire(Full duplex) 구성 |
| MISO (PA6) | 센서 → MCU | MISO를 별도 입력으로 사용 (3-wire 아님) |
| CS (PA4) | MCU → 센서 | **소프트웨어 CS**, Low = 선택 (`SPI_NSS_SOFT`) |

- **일반 사양**: BMP280은 SPI Mode '00'과 '11'을 모두 지원하고, SPI 클럭은 최대 10 MHz입니다. 따라서 Mode 3 / 8 MHz 설정은 사양 범위 안입니다. (데이터시트 SPI interface 절에서 재확인 권장)
- **일반 사양**: BMP280은 CSB 핀에 Low가 들어오면 SPI 모드로 전환되고, 다음 전원 리셋까지 SPI를 유지합니다. 그래서 부팅 때 CS를 High로 두었다가 첫 트랜잭션에서 Low로 내리는 현재 구조로 SPI 모드에 진입합니다.

### 3-3. TM1637 — GPIO 2선

| 신호 | 방향 | 비고 |
|---|---|---|
| CLK (PB8) | MCU → TM1637 | Push-Pull |
| DIO (PB9) | 양방향 | Open-Drain 출력 + 내부 Pull-up. ACK 구간에 High 해제 후 `HAL_GPIO_ReadPin` |

TM1637의 2선 프로토콜은 I2C와 비슷한 Start/Stop/ACK 형태이나 **I2C 주소 체계가 없고 LSB-first**이므로 I2C 주변장치로 구동하지 않고 GPIO로 구현했습니다 (`tm1637.c`).

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
| TM1637 모듈 RC 필터 | CLK/DIO 라인에 풀업 저항 + 소용량 커패시터 | **일반 사양**. `tm1637.c`의 25 µs 지연("RC 필터 충전 보장")이 이를 고려한 값 |

### 4-1. TM1637 5 V 구동과 3.3 V MCU의 레벨 차이

TM1637만 5 V로 동작하고 MCU는 3.3 V이므로, 이 구간은 레벨 호환성을 확인해야 합니다.

| 항목 | 내용 |
|---|---|
| MCU 핀 내압 | **일반 사양**: STM32F103의 PB8/PB9는 FT(5 V tolerant) 핀이므로 5 V 풀업이 걸려도 입력·Open-Drain에서 손상되지 않습니다. |
| DIO (Open-Drain) | 모듈의 5 V 풀업이 High를 만들기 때문에 High 레벨은 약 5 V가 됩니다. **레벨 문제 없음** |
| CLK (Push-Pull) | MCU가 High를 3.3 V로 구동합니다. TM1637의 입력 High 기준(V<sub>IH</sub>)은 일반적으로 0.7 × VDD = 3.5 V(VDD 5 V일 때)라서, **3.3 V는 기준보다 낮아 여유가 없습니다.** 또 모듈 풀업(5 V)과 MCU Push-Pull High(3.3 V)가 맞물려 미세한 전류가 흐를 수 있습니다. |
| 개선안 | CLK도 DIO처럼 **Open-Drain**으로 바꾸면 High를 모듈의 5 V 풀업이 만들어 V<sub>IH</sub>를 만족합니다. 또는 TM1637 모듈을 3.3 V로 구동하는 방법도 있습니다(밝기 저하 가능). |

> 현재 구성에서도 동작할 수 있지만 사양 여유가 없는 구간입니다. 오실로스코프로 CLK High 레벨을 측정해 두면 근거 자료가 됩니다.

## 5. 부품 사용 이유 (일반 원리 vs 프로젝트 확인)

| 구성 | 프로젝트에서 확인된 것 | 일반 설계 원리 (참고) |
|---|---|---|
| I2C 풀업 | 핀이 Open-Drain으로 설정됨, MCU 내부 풀업 미사용 | Open-Drain 버스는 High 레벨을 풀업 저항이 만들어야 함. GY-302 모듈에 내장된 풀업을 사용 |
| TM1637 DIO Open-Drain | `GPIO_MODE_OUTPUT_OD` + `GPIO_PULLUP` (CubeMX 기본 PP를 변경: `.ioc` `PB9.GPIO_ModeDefaultOutputPP=GPIO_MODE_OUTPUT_OD`) | ACK 시 TM1637이 DIO를 Low로 당기므로 MCU가 Push-Pull로 High를 구동하면 충돌 가능. OD로 두면 해제(High-Z) 후 읽기 가능. 또한 5 V 풀업으로 High 레벨이 TM1637 기준을 만족 |
| BMP280 CS 초기 High | `MX_GPIO_Init`에서 `HAL_GPIO_WritePin(CS, SET)`을 핀 모드 설정 **이전**에 수행 | 출력 활성화 순간 CS가 Low로 글리치 되는 것을 방지하는 효과 |
| 센서 3.3 V / 표시기 5 V 분리 | 회로도 전원 레일 | 센서는 MCU와 같은 3.3 V로 레벨 변환 없이 연결, TM1637은 LED 밝기를 위해 5 V 사용 |
| ST-LINK VCP 사용 | USART2 = PA2/PA3 | 추가 부품 없이 USB 하나로 디버깅·전원·로그를 모두 처리 |

## 6. 외부 회로에서 검증해야 할 사항

> 실제 보드 동작 결과(Tera Term 로그, 7-segment 표시)는 [06_testing_and_results.md](06_testing_and_results.md)에 정리합니다.


- [x] BH1750 / BMP280 동작 전압 3.3 V, TM1637 5 V (회로도로 확인)
- [x] BH1750 ADDR 핀 → GND, 주소 0x23 (개발자 확인)
- [x] BMP280 모듈을 SPI 모드로 결선 (회로도 J2: CSB → PA4, SDO → MISO)
- [x] PC 로그 경로: ST-LINK 가상 COM 포트
- [ ] GY-302 모듈의 I2C 풀업 저항 값 (실물 저항 마킹 또는 오실로스코프로 상승 시간 측정)
- [ ] TM1637 CLK High 레벨(3.3 V Push-Pull)이 5 V 구동 TM1637의 V<sub>IH</sub>를 만족하는지 (4-1절)
- [ ] TM1637 CLK/DIO 파형 (로직 분석기) 및 모듈 내부 RC 필터 영향
- [ ] 8 MHz SPI 배선 길이 (점퍼선이 길면 파형 왜곡 가능)

## 7. 추가하면 좋은 자료

- 실제 배선 사진 → `docs/images/`에 추가 후 이 문서에 링크
- GY-302 모듈 기판 사진 (I2C 풀업 저항 마킹 확인용)
- 로직 분석기 캡처: SPI(BMP280 Chip ID 읽기), I2C(BH1750 2바이트 수신), TM1637 1바이트 전송 + ACK
