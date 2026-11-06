/**
 ******************************************************************************
 * @file    tm1637.h
 * @brief   TM1637 4자리 7세그먼트 LED 드라이버 헤더 파일
 *          타겟 MCU   : STM32F103RBT6
 *          핀 연결    : CLK -> PB8 (Push-Pull, No-Pull)
 *                       DIO -> PB9 (Open-Drain, Pull-up)
 ******************************************************************************
 */

#ifndef __TM1637_H__
#define __TM1637_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* TM1637 내부 명령어 */
#define TM1637_CMD_DATA_AUTO    0x40  // 데이터 명령: 자동 주소 증가 모드
#define TM1637_CMD_DATA_FIXED   0x44  // 데이터 명령: 고정 주소 모드
#define TM1637_CMD_ADDR_BASE    0xC0  // 표시 주소 명령: 첫 번째 디지트 (GRID1)
#define TM1637_CMD_DISPLAY_ON   0x88  // 디스플레이 제어: 켜기 (기본 밝기 포함)
#define TM1637_CMD_DISPLAY_OFF  0x80  // 디스플레이 제어: 끄기

/* 함수 프로토타입 */
void TM1637_Init(void);
void TM1637_SetBrightness(uint8_t brightness);
void TM1637_DisplayOn(void);
void TM1637_DisplayOff(void);
void TM1637_Clear(void);

void TM1637_DisplayRaw(const uint8_t segments[4]);
void TM1637_DisplayDecimal(int16_t num, bool leadingZero);
void TM1637_DisplayFloat(float num, uint8_t decimalPlaces);
void TM1637_DisplayString(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __TM1637_H__ */
