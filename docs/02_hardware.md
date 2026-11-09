# 02. 하드웨어 회로 구성 및 인터페이스 분석

> ⚠ **회로도·배선 사진·BOM이 프로젝트에 없습니다.** 아래는 **소스/`.ioc`로 확인된 사실**과 **일반 설계 원리·추정**을 구분해 작성했습니다. 저항·커패시터 값, 전원 전압, 모듈 종류(GY-302 등)는 코드로 확인할 수 없으므로 임의로 기재하지 않았습니다.

## 1. 구성 요소와 역할 (코드 기준)

| 장치 | 역할 | 
|---|---|
| STM32F103RBT6 | 마스터 MCU |
| BH1750 | 조도 센서, I2C 7-bit 주소 0x23 |
| BMP280 | 기압/온도 센서, SPI 접속 |
| TM1637 | 7-segment LED 드라이버 (4-digit) |
| USB-UART 경로 | PC 로그 (USART2) |

## 2. MCU 핀 할당표 (`.ioc` + `main.h` + `stm32f1xx_hal_msp.c`)

| 핀 | 기능 | GPIO 모드 (생성 코드) | 초기 레벨 | 연결 대상 |
|---|---|---|---|---|
| PA2 | USART2_TX |  Push-Pull | — | PC 수신 |
| PA3 | USART2_RX | Input (No pull) | — | PC 송신 (코드상 수신 미사용) |
| PA4 | `BMP280_CS` | Output PP, High speed | **High** (비선택) | BMP280 CSB |
| PA5 | SPI1_SCK |  Push-Pull | — | BMP280 SCK |
| PA6 | SPI1_MISO | Input (No pull) | — | BMP280 SDO |
| PA7 | SPI1_MOSI |  Push-Pull | — | BMP280 SDI |
| PB6 | I2C1_SCL |  Open-Drain | — | BH1750 SCL |
| PB7 | I2C1_SDA |  Open-Drain | — | BH1750 SDA |
| PB8 | `TM1637_CLK` | Output Push-Pull, High speed | High | TM1637 CLK |
| PB9 | `TM1637_DIO` | Output Open-Drain + Pull-up, High speed | High | TM1637 DIO |
| PA13 / PA14 | SWDIO / SWCLK | Serial Wire | — | ST-Link |

## 3. 인터페이스별 신호선과 데이터 방향

### 3-1. I2C1 — BH1750

| 신호 | 방향 | 비고 |
|---|---|---|
| SCL (PB6) | MCU → 센서 (마스터 클럭) | 100 kHz (Standard mode) |
| SDA (PB7) | 양방향 | Open-Drain |

- 주소 바이트: 7-bit `0x23` → HAL 규약상 `0x23 << 1 = 0x46` 전달 (HAL이 R/W 비트를 OR).
- 통신: `Power On(0x01)` → `Continuous H-Res Mode(0x10)` 전송 후, 매 읽기마다 2바이트 수신.

### 3-2. SPI1 — BMP280

| 신호 | 방향 | 비고 |
|---|---|---|
| SCK (PA5) | MCU → 센서 | 64 MHz / 8 = 8 MHz, **CPOL=1, CPHA=1 (Mode 3)** |
| MOSI (PA7) | MCU → 센서 | 4-wire(Full duplex) 구성 |
| MISO (PA6) | 센서 → MCU | MISO를 별도 입력으로 사용 (3-wire 아님) |
| CS (PA4) | MCU → 센서 | **소프트웨어 CS**, Low = 선택 (`SPI_NSS_SOFT`) |

### 3-3. TM1637 — GPIO 2선

| 신호 | 방향 | 비고 |
|---|---|---|
| CLK (PB8) | MCU → TM1637 | Push-Pull |
| DIO (PB9) | 양방향 | Open-Drain 출력 + 내부 Pull-up. ACK 구간에 High 해제 후 `HAL_GPIO_ReadPin` |

TM1637의 2선 프로토콜은 I2C와 비슷한 Start/Stop/ACK 형태이나 **I2C 주소 체계가 없고 LSB-first**이므로 I2C 주변장치로 구동하지 않고 GPIO로 구현했습니다 (`tm1637.c`).

### 3-4. USART2

115200 bps

## 4. 전기적 특성 (코드로 확인된 값 vs 미확인)

| 항목 | 확인된 값 | 근거 |
|---|---|---|
| 시스템 클럭 | 64 MHz | `.ioc` RCC |
| GPIO 출력 속도 | HIGH (HAL: 50 MHz 모드) | `GPIO_SPEED_FREQ_HIGH` |
| I2C 속도 | 100 kHz | `hi2c1.Init.ClockSpeed` |
| SPI 속도 | 8 Mbit/s | `.ioc` `SPI1.CalculateBaudRate` |
| 전원 전압(센서/모듈) | **확인 불가** | 회로도 없음 |
| I2C 풀업 저항 값 | **확인 불가** | 회로도 없음 (MCU 내부 풀업은 I2C에 설정되지 않음: `GPIO_InitStruct.Pull` 미지정) |
| TM1637 모듈 RC 필터 | **추정** | `tm1637.c`의 주석 "RC 필터 충전 보장 딜레이 (25us)"가 유일한 단서 |

## 5. 부품 사용 이유 (일반 원리 vs 프로젝트 확인)

| 구성 | 프로젝트에서 확인된 것 | 일반 설계 원리 (참고) |
|---|---|---|
| I2C 풀업 | 핀이 Open-Drain으로 설정됨 | Open-Drain 버스는 High 레벨을 풀업 저항이 만들어야 함. 모듈 보드에 내장되어 있는지는 **확인 필요** |
| TM1637 DIO Open-Drain | `GPIO_MODE_OUTPUT_OD` + `GPIO_PULLUP` (CubeMX 기본 PP를 변경: `.ioc` `PB9.GPIO_ModeDefaultOutputPP=GPIO_MODE_OUTPUT_OD`) | ACK 시 TM1637이 DIO를 Low로 당기므로 MCU가 Push-Pull로 High를 구동하면 충돌 가능. OD로 두면 해제(High-Z) 후 읽기 가능 |
| BMP280 CS 초기 High | `MX_GPIO_Init`에서 `HAL_GPIO_WritePin(CS, SET)`을 핀 모드 설정 **이전**에 수행 | 출력 활성화 순간 CS가 Low로 글리치 되는 것을 방지하는 효과 |

## 6. 외부 회로에서 검증해야 할 사항 (사용자 확인 필요)

- [ ] BH1750 / BMP280 / TM1637 모듈의 **동작 전압 및 로직 레벨**이 3.3 V와 호환되는지
- [ ] I2C SDA/SCL 풀업 저항 존재 여부와 값 (오실로스코프로 상승시간 확인)
- [ ] BH1750 ADDR 핀 연결 → 주소 0x23(Low) 일치 여부
- [ ] BMP280 모듈이 **SPI 모드로 구성**되었는지 (모듈 핀 명칭·CSB 처리)
- [ ] BMP280 SPI 모드(Mode 0/3) 허용 여부 — 데이터시트 SPI 절에서 확인 (본 문서 작성 시 페이지 미확인)
- [ ] TM1637 CLK/DIO 파형 (로직 분석기) 및 모듈 내부 RC 필터 영향
- [ ] 디커플링 커패시터, 배선 길이 (특히 8 MHz SPI)

## 7. 추가하면 좋은 자료

회로도(KiCad/Fritzing/손그림 사진), 실제 배선 사진, 모듈 모델명 → `assets/`에 추가 후 이 문서에 링크하세요.
