# STM32F103RB 다중 인터페이스 환경 센서 모니터

**I2C(BH1750 조도) · SPI(BMP280 기압·온도) · GPIO 비트뱅잉(TM1637 7-Segment) · UART 로그를 한 MCU에서 통합한 베어메탈 펌웨어**

![MCU](https://img.shields.io/badge/MCU-STM32F103RB-03234B)
![Board](https://img.shields.io/badge/Board-NUCLEO--F103RB-03234B)
![Language](https://img.shields.io/badge/Language-C-555555)
![HAL](https://img.shields.io/badge/STM32Cube-HAL-1f6feb)
![I2C](https://img.shields.io/badge/I2C-BH1750-2ea043)
![SPI](https://img.shields.io/badge/SPI-BMP280-2ea043)
![GPIO](https://img.shields.io/badge/GPIO_bit--bang-TM1637-2ea043)
![UART](https://img.shields.io/badge/UART-115200-2ea043)

<table>
<tr>
<td width="50%"><img src="docs/images/readme/hero_setup.jpg" width="100%"></td>
<td width="50%">
<img src="docs/images/readme/boot_good.gif" width="100%"><br>
<sub>RESET → <code>TEST</code> → <code>GOOD</code> → 기압 <code>1015</code> hPa</sub>
<br><br>
<img src="docs/images/readme/pressure_blow.gif" width="100%"><br>
<sub>센서에 바람을 불면 기압 표시가 <code>1015 → 1017 → 1015</code>로 바뀜</sub>
</td>
</tr>
</table>

| 요구사항 검증 | 고장 주입 | 실제 디버깅 사례 | Flash / RAM |
|:---:|:---:|:---:|:---:|
| **8 / 8 통과** | **3건** | **2건** | **30 % / 13 %** |

---

## 하드웨어

<table>
<tr>
<td width="45%"><img src="docs/images/wiring_overview.jpg" width="100%"><br><sub>전체 배선 (NUCLEO-F103RB + 브레드보드)</sub></td>
<td width="55%"><img src="docs/images/hardware_schematic.png" width="100%"><br><sub>연결 회로도 — 센서 3.3 V, TM1637 5 V</sub></td>
</tr>
</table>

| TM1637 4-digit (GPIO PB8/PB9) | GY-302 BH1750 (I2C1 PB6/PB7) | GY-BMP280 (SPI1 PA4~PA7) |
|:---:|:---:|:---:|
| <img src="docs/images/module_tm1637.jpg" width="220"> | <img src="docs/images/module_gy302_bh1750.jpg" width="220"> | <img src="docs/images/module_gy_bmp280.jpg" width="220"> |

→ 핀 할당, 레벨 호환(5 V ↔ 3.3 V), 풀업 실측: [02_hardware.md](docs/02_hardware.md)

## 시스템 구조

![시스템 아키텍처](docs/images/system-architecture.png)

<sub>BMP280 센싱·표시 100 ms · BH1750 200 ms · UART 500 ms — `HAL_GetTick()` 기반 non-blocking 스케줄링</sub>

→ 개발 목적, 기능, 개발 환경, 부팅·런타임 흐름: [01_system_architecture.md](docs/01_system_architecture.md)

## 동작 데모

![고도 그래프](docs/images/readme/altitude_lift_lower.png)

<table>
<tr>
<td width="50%"><img src="docs/images/readme/lux_cover.gif" width="100%"><br><sub>BH1750: 손으로 가리면 Tera Term의 Lux 값이 0 근처로 떨어짐</sub></td>
<td width="50%"><img src="docs/images/troubleshooting/11_teraterm_altitude_lift_lower.png" width="100%"><br><sub>위 그래프의 원본 Tera Term 로그 (부팅 → 0점 → 들어 올림 → 내림)</sub></td>
</tr>
</table>

## 트러블슈팅 하이라이트

**① BMP280 SPI 통신 실패 — 보드 핀 오배선** · CSB는 토글하는데 SCK에 클럭이 없음 → NUCLEO 회로도 추적 → CN10-26/28/30은 F103RB에서 PB15/14/13 → CN10-11/13/15로 이동

| 증상: CSB(초록)만 토글, SCK(노랑) 없음 | 원인: CN10 솔더 브리지 구성 | 해결 후: SPI 데이터 파형 |
|:---:|:---:|:---:|
| <img src="docs/images/troubleshooting/05_scope_cs_vs_sck.jpg" width="260"> | <img src="docs/images/troubleshooting/08_nucleo_cn10_solder_bridges.png" width="300"> | <img src="docs/images/troubleshooting/09_scope_after_fix_1.jpg" width="260"> |

**② 책상 위에서 고도 −42.7 m** · 로그 기압을 공식에 직접 넣어 같은 값 재현 → 계산은 정상, 기준 기압이 1013.25 hPa 기본값 → 부팅 시 첫 측정(max 43.2 ms → 50 ms 대기)을 기준으로 0점 설정 → 위 그래프처럼 0 m에서 시작

→ 디버깅 과정, 측정 사진, 리뷰 이슈: [05_troubleshooting.md](docs/05_troubleshooting.md)

## 검증 증거

| 부팅 자가진단: 센서 전원 분리 시 | 디버거 SFR: SPI1 `CR1 = 0x357` | 디버거 SFR: I2C1 `CCR = 0xA0` · `TRISE = 0x21` |
|:---:|:---:|:---:|
| <img src="docs/images/readme/boot_err.gif" width="260"> | <img src="docs/images/testing/04_sfr_spi1_cr1.png" width="260"> | <img src="docs/images/testing/07_sfr_i2c1.png" width="260"> |
| **멀티미터: TM1637 CLK High 4.909 V** | **멀티미터: NUCLEO 5V 4.886 V** | **멀티미터: GY-302 풀업 SCL–SDA 9.387 kΩ** |
| <img src="docs/images/testing/05_multimeter_tm1637_clk.jpg" width="200"> | <img src="docs/images/testing/06_multimeter_nucleo_5v.jpg" width="200"> | <img src="docs/images/testing/09_multimeter_gy302_scl_sda.jpg" width="200"> |

<sub>UART 출력 간격 실측 평균 500.42 ms (타임스탬프 로그 316개 간격) · USART2 `BRR = 0x116` · 레지스터 값은 모두 RM0008 기준 계산값과 일치</sub>

→ 검증 매트릭스(R1~R8), 합격 기준, 원본 로그: [06_testing_and_results.md](docs/06_testing_and_results.md)

## 상세 문서

| 문서 | 내용 |
|---|---|
| [01 시스템 아키텍처](docs/01_system_architecture.md) | 개발 목적, 주요 기능, 개발 환경, 부팅·런타임 흐름, 모듈 역할 |
| [02 하드웨어](docs/02_hardware.md) | 핀 할당, 인터페이스별 신호, 5 V ↔ 3.3 V 레벨 호환, 풀업 실측 |
| [03 CubeMX 설정](docs/03_cubemx_configuration.md) | 클럭, GPIO, I2C·SPI·UART 설정과 레지스터 계산 |
| [04 펌웨어 구현](docs/04_firmware_implementation.md) | 핵심 구현 5가지, 소스 구성, HAL → 레지스터 → 신호 추적, 실제 로그 |
| [05 트러블슈팅](docs/05_troubleshooting.md) | 실제 디버깅 2건(SPI 오배선, 고도 기준값), 설계 대응, 리뷰 이슈 |
| [06 테스트 결과](docs/06_testing_and_results.md) | 검증 매트릭스, 고장 주입 3건, 레지스터·멀티미터 확인, 리소스 |
