/**
 ******************************************************************************
 * @file    bh1750.h
 * @brief   BH1750 (GY-302) 조도 센서 I2C 드라이버 헤더 파일
 *          타겟 MCU   : STM32F103RBT6
 *          인터페이스 : I2C1 (PB6=SCL, PB7=SDA, 100kHz Standard Mode)
 *          슬레이브 주소: 0x23 (ADDR 핀 GND 연결 시 7비트 0x23 -> 8비트 0x46)
 ******************************************************************************
 */

#ifndef __BH1750_H__
#define __BH1750_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/* BH1750 I2C 7비트 주소 (ADDR=GND: 0x23 -> 8비트 주소: 0x46) */
#define BH1750_I2C_ADDR         (0x23 << 1)

/* BH1750 내부 명령어 (Opcode) */
#define BH1750_OP_POWER_ON      0x01
#define BH1750_OP_CONT_H_RES    0x10  // Continuous H-Resolution Mode (1 lx 분해능)

/* 함수 프로토타입 */
HAL_StatusTypeDef BH1750_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BH1750_ReadLux(I2C_HandleTypeDef *hi2c, float *lux);

#ifdef __cplusplus
}
#endif

#endif /* __BH1750_H__ */
