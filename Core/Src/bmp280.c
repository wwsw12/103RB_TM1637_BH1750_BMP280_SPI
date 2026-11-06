/**
 ******************************************************************************
 * @file    bmp280.c
 * @brief   BMP280 기압/온도 센서 SPI1 드라이버 소스 파일
 ******************************************************************************
 */

#include "bmp280.h"

static BMP280_CalibData_t calibData;
static float current_sea_level_hPa = BMP280_SEA_LEVEL_HPA;

/* SPI CS(Chip Select) 핀 제어 인라인 함수 */
static inline void BMP280_CS_Low(void)
{
    HAL_GPIO_WritePin(BMP280_CS_GPIO_Port, BMP280_CS_Pin, GPIO_PIN_RESET);
}

static inline void BMP280_CS_High(void)
{
    HAL_GPIO_WritePin(BMP280_CS_GPIO_Port, BMP280_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief  BMP280 SPI 단일 레지스터 쓰기 함수
 * @param  hspi : SPI 핸들 포인터
 * @param  reg  : 대상 레지스터 주소 (Write 시 MSB bit 7 = 0)
 * @param  data : 기록할 1바이트 데이터
 */
static HAL_StatusTypeDef BMP280_SPI_WriteReg(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t data)
{
    HAL_StatusTypeDef status;
    uint8_t tx[2];
    tx[0] = (uint8_t)(reg & 0x7F); // Write 명령: 최상위 비트(bit 7) = 0
    tx[1] = data;

    BMP280_CS_Low();
    status = HAL_SPI_Transmit(hspi, tx, 2, 10);
    BMP280_CS_High();
    return status;
}

/**
 * @brief  BMP280 SPI 연속 레지스터 읽기 함수
 * @param  hspi   : SPI 핸들 포인터
 * @param  reg    : 시작 레지스터 주소 (Read 시 MSB bit 7 = 1)
 * @param  data   : 수신 버퍼 포인터
 * @param  length : 읽을 바이트 수
 */
static HAL_StatusTypeDef BMP280_SPI_ReadRegs(SPI_HandleTypeDef *hspi, uint8_t reg, uint8_t *data, uint16_t length)
{
    HAL_StatusTypeDef status;
    uint8_t addr = (uint8_t)(reg | 0x80); // Read 명령: 최상위 비트(bit 7) = 1

    BMP280_CS_Low();
    status = HAL_SPI_Transmit(hspi, &addr, 1, 10);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(hspi, data, length, 10);
    }
    BMP280_CS_High();
    return status;
}

/**
 * @brief  BMP280 NVM 공장 보정 계수(24바이트) 읽기
 */
static HAL_StatusTypeDef BMP280_ReadCalibrationData(SPI_HandleTypeDef *hspi)
{
    uint8_t calibBuff[24] = {0};
    HAL_StatusTypeDef status = BMP280_SPI_ReadRegs(hspi, BMP280_REG_CALIB_START, calibBuff, 24);
    if (status != HAL_OK)
    {
        return status;
    }

    // 온도 보정 파라미터 (Little Endian 파싱)
    calibData.dig_T1 = (uint16_t)((calibBuff[1] << 8) | calibBuff[0]);
    calibData.dig_T2 = (int16_t) ((calibBuff[3] << 8) | calibBuff[2]);
    calibData.dig_T3 = (int16_t) ((calibBuff[5] << 8) | calibBuff[4]);

    // 기압 보정 파라미터 (Little Endian 파싱)
    calibData.dig_P1 = (uint16_t)((calibBuff[7] << 8) | calibBuff[6]);
    calibData.dig_P2 = (int16_t) ((calibBuff[9] << 8) | calibBuff[8]);
    calibData.dig_P3 = (int16_t) ((calibBuff[11] << 8) | calibBuff[10]);
    calibData.dig_P4 = (int16_t) ((calibBuff[13] << 8) | calibBuff[12]);
    calibData.dig_P5 = (int16_t) ((calibBuff[15] << 8) | calibBuff[14]);
    calibData.dig_P6 = (int16_t) ((calibBuff[17] << 8) | calibBuff[16]);
    calibData.dig_P7 = (int16_t) ((calibBuff[19] << 8) | calibBuff[18]);
    calibData.dig_P8 = (int16_t) ((calibBuff[21] << 8) | calibBuff[20]);
    calibData.dig_P9 = (int16_t) ((calibBuff[23] << 8) | calibBuff[22]);

    return HAL_OK;
}

/**
 * @brief  BMP280 센서 SPI1 초기화
 * @param  hspi : SPI 핸들 포인터 (&hspi1)
 * @retval HAL_StatusTypeDef : 초기화 성공 여부
 */
HAL_StatusTypeDef BMP280_Init(SPI_HandleTypeDef *hspi)
{
    HAL_StatusTypeDef status;
    uint8_t chipId = 0;

    // 1. Chip ID 확인 (0xD0 레지스터에 -> 0x58)
    status = BMP280_SPI_ReadRegs(hspi, BMP280_REG_ID, &chipId, 1);
    if (status != HAL_OK || chipId != BMP280_CHIP_ID)
    {
        return HAL_ERROR;
    }

    // 2. 소프트웨어 리셋 (0xE0 -> 0xB6)
    BMP280_SPI_WriteReg(hspi, BMP280_REG_RESET, BMP280_RESET_VALUE);
    HAL_Delay(10); // 리셋 후 NVM 로드 대기 (t_startup = 2ms)

    // 3.보정 계수 로드
    status = BMP280_ReadCalibrationData(hspi);
    if (status != HAL_OK) return status;

    // 4. CONFIG 레지스터 (0xF5) 설정: t_sb=62.5ms (001b << 5), Filter=4 (010b << 2) -> 0x28
    uint8_t configValue = (0x01 << 5) | (0x02 << 2);
    status = BMP280_SPI_WriteReg(hspi, BMP280_REG_CONFIG, configValue);
    if (status != HAL_OK) return status;

    // 5. CTRL_MEAS 레지스터 (0xF4) 설정: osrs_t=x2 (010b << 5), osrs_p=x16 (101b << 2), mode=Normal (11b) -> 0x57
    uint8_t ctrlMeasValue = (0x02 << 5) | (0x05 << 2) | 0x03;
    status = BMP280_SPI_WriteReg(hspi, BMP280_REG_CTRL_MEAS, ctrlMeasValue);
    if (status != HAL_OK) return status;

    return HAL_OK;
}

/**
 * @brief  데이터시트 8.1 공식 기반 온도 보정 계산 함수 (Double Precision)
 */
static float BMP280_Compensate_Temperature(int32_t adc_T)
{
    double var1, var2, T;
    var1 = (((double)adc_T) / 16384.0 - ((double)calibData.dig_T1) / 1024.0) * ((double)calibData.dig_T2);
    var2 = ((((double)adc_T) / 131072.0 - ((double)calibData.dig_T1) / 8192.0) *
            (((double)adc_T) / 131072.0 - ((double)calibData.dig_T1) / 8192.0)) * ((double)calibData.dig_T3);
    calibData.t_fine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;
    return (float)T;
}

/**
 * @brief  데이터시트 8.1 공식 기반 기압 보정 계산 함수 (Double Precision)
 */
static float BMP280_Compensate_Pressure(int32_t adc_P)
{
    double var1, var2, p;
    var1 = ((double)calibData.t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)calibData.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)calibData.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)calibData.dig_P4) * 65536.0);
    var1 = (((double)calibData.dig_P3) * var1 * var1 / 524288.0 + ((double)calibData.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)calibData.dig_P1);

    if (var1 == 0.0) return 0.0f; // 0으로 나누기 방지

    p = 1048576.0 - (double)adc_P;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = ((double)calibData.dig_P9) * p * p / 2147483648.0;
    var2 = p * ((double)calibData.dig_P8) / 32768.0;
    p = p + (var1 + var2 + ((double)calibData.dig_P7)) / 16.0;

    return (float)(p / 100.0); // Pa -> hPa 변환
}

/**
 * @brief  국제 표준 대기압 공식 기반 해발 고도 계산 (m)
 */
static float BMP280_Calculate_Altitude(float pressure_hPa)
{
    if (pressure_hPa <= 0.0f || current_sea_level_hPa <= 0.0f) return 0.0f;
    return 44330.0f * (1.0f - powf(pressure_hPa / current_sea_level_hPa, 0.1902949f));
}

/**
 * @brief  기준 해면기압(Sea Level Pressure) 동적 설정 함수
 * @param  sea_level_hpa : 설정할 기준 해면기압 (hPa 단위, 예: 1013.25f)
 */
void BMP280_SetSeaLevelPressure(float sea_level_hpa)
{
    if (sea_level_hpa > 0.0f)
    {
        current_sea_level_hPa = sea_level_hpa;
    }
}

/**
 * @brief  현재 설정된 기준 해면기압(Sea Level Pressure) 반환 함수
 * @retval 현재 기준 해면기압 (hPa)
 */
float BMP280_GetSeaLevelPressure(void)
{
    return current_sea_level_hPa;
}

/**
 * @brief  BMP280으로부터 온도, 기압, 고도 일괄 측정
 * @param  hspi        : SPI 핸들 포인터 (&hspi1)
 * @param  temperature : 온도를 저장할 float 포인터 (℃)
 * @param  pressure    : 기압을 저장할 float 포인터 (hPa)
 * @param  altitude    : 고도를 저장할 float 포인터 (m, NULL 전달 시 계산 생략)
 * @retval HAL_StatusTypeDef : 측정 성공 여부
 */
HAL_StatusTypeDef BMP280_ReadData(SPI_HandleTypeDef *hspi, float *temperature, float *pressure, float *altitude)
{
    uint8_t rawBuff[6] = {0};

    // 0xF7(Press MSB)부터 0xFC(Temp XLSB)까지 6바이트 일괄 버스트 리드
    HAL_StatusTypeDef status = BMP280_SPI_ReadRegs(hspi, BMP280_REG_PRESS_MSB, rawBuff, 6);
    if (status != HAL_OK)
    {
        return status;
    }

    // 20비트 ADC Raw 데이터 결합
    int32_t adc_P = (int32_t)((((uint32_t)rawBuff[0]) << 12) | (((uint32_t)rawBuff[1]) << 4) | (((uint32_t)rawBuff[2]) >> 4));
    int32_t adc_T = (int32_t)((((uint32_t)rawBuff[3]) << 12) | (((uint32_t)rawBuff[4]) << 4) | (((uint32_t)rawBuff[5]) >> 4));

    // 1. 온도 먼저 계산 (t_fine 매개변수 생성을 위해 온도를 반드시 먼저 계산해야 함)
    float temp_val = BMP280_Compensate_Temperature(adc_T);
    if (temperature != NULL)
    {
        *temperature = temp_val;
    }

    // 2. 기압 계산
    float press_val = BMP280_Compensate_Pressure(adc_P);
    if (pressure != NULL)
    {
        *pressure = press_val;
    }

    // 3. 고도 계산 (altitude 포인터가 제공되었을 때만 powf 실행)
    if (altitude != NULL)
    {
        *altitude = BMP280_Calculate_Altitude(press_val);
    }

    return HAL_OK;
}
