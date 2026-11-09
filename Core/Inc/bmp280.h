/**
 ******************************************************************************
 * @file    bmp280.h
 * @brief   BMP280 기압/온도 센서 SPI1 드라이버 헤더 파일
 *          타겟 MCU   : STM32F103RBT6
 *          인터페이스 : SPI1 (PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI)
 *          SPI 규격   : Mode 3 (CPOL=1, CPHA=1), MSB First, 4-Wire Full-Duplex
 ******************************************************************************
 */

#ifndef __BMP280_H__
#define __BMP280_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "main.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* BMP280 주요 레지스터 주소 */
#define BMP280_REG_CALIB_START   0x88  // 24바이트 캘리브레이션 파라미터 시작 (0x88~0xA1)
#define BMP280_REG_ID            0xD0  // Chip ID 레지스터 (기본값: 0x58)
#define BMP280_REG_RESET         0xE0  // 소프트웨어 리셋 레지스터 (0xB6 쓰기)
#define BMP280_REG_CTRL_MEAS     0xF4  // 오버샘플링 및 동작 모드 설정 레지스터
#define BMP280_REG_CONFIG        0xF5  // 스탠바이 시간, IIR 필터 설정 레지스터
#define BMP280_REG_PRESS_MSB     0xF7  // 기압 Raw 데이터 시작 (0xF7~0xF9)
#define BMP280_REG_TEMP_MSB      0xFA  // 온도 Raw 데이터 시작 (0xFA~0xFC)

/* 주요 상수 값 */
#define BMP280_CHIP_ID           0x58
#define BMP280_RESET_VALUE       0xB6

/* BMP280 NVM 공장 보정 계수 구조체 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    int32_t  t_fine;  // 온도 보정 시 계산되는 중간 매개변수 (기압 보정에 필수 사용)
} BMP280_CalibData_t;

/* 기준 해면기압 상수 (국제 표준 대기압 기본값: 1013.25 hPa) */
#define BMP280_SEA_LEVEL_HPA     1013.25f

/* 함수 프로토타입 */
HAL_StatusTypeDef BMP280_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef BMP280_ReadData(SPI_HandleTypeDef *hspi, float *temperature, float *pressure, float *altitude);
void BMP280_SetSeaLevelPressure(float sea_level_hpa);
float BMP280_GetSeaLevelPressure(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMP280_H__ */
