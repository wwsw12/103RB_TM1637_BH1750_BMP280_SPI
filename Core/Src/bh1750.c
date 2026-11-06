/**
 ******************************************************************************
 * @file    bh1750.c
 * @brief   BH1750 조도 센서 I2C 드라이버 소스 파일
 ******************************************************************************
 */

#include "bh1750.h"

/**
 * @brief  BH1750 조도 센서 초기화 (Power On 및 연속 고분해능 모드 시작)
 * @param  hi2c : I2C 핸들 포인터 (&hi2c1)
 * @retval HAL_StatusTypeDef : 초기화 성공 여부
 */
HAL_StatusTypeDef BH1750_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_StatusTypeDef status;
    uint8_t cmd;

    // 1. 센서 존재 확인 (Ready 체크)
    status = HAL_I2C_IsDeviceReady(hi2c, BH1750_I2C_ADDR, 3, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    // 2. Power On 명령 전송 (0x01)
    cmd = BH1750_OP_POWER_ON;
    status = HAL_I2C_Master_Transmit(hi2c, BH1750_I2C_ADDR, &cmd, 1, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    // 3. Continuous H-Resolution Mode 시작 (0x10)
    cmd = BH1750_OP_CONT_H_RES;
    status = HAL_I2C_Master_Transmit(hi2c, BH1750_I2C_ADDR, &cmd, 1, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    // 첫 측정 완료 안정화 대기 (10ms)
    HAL_Delay(10);
    return HAL_OK;
}

/**
 * @brief  BH1750으로부터 조도(Lux) 데이터 읽기
 * @param  hi2c : I2C 핸들 포인터 (&hi2c1)
 * @param  lux  : 조도값(lx)을 저장할 float 포인터
 * @retval HAL_StatusTypeDef : 읽기 성공 여부
 */
HAL_StatusTypeDef BH1750_ReadLux(I2C_HandleTypeDef *hi2c, float *lux)
{
    uint8_t buffer[2] = {0};
    HAL_StatusTypeDef status;

    // 2바이트 조도 Raw 데이터 수신
    status = HAL_I2C_Master_Receive(hi2c, BH1750_I2C_ADDR, buffer, 2, 100);
    if (status != HAL_OK)
    {
        return status;
    }

    // MSB, LSB 조합하여 16비트 Raw 값 계산
    uint16_t rawData = ((uint16_t)buffer[0] << 8) | buffer[1];

    // 데이터시트 공식: Lux = Raw / 1.2
    *lux = (float)rawData / 1.2f;

    return HAL_OK;
}
