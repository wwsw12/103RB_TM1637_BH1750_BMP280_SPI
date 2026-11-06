/**
 ******************************************************************************
 * @file    tm1637.c
 * @brief   TM1637 4자리 7세그먼트 LED 드라이버 소스 파일
 ******************************************************************************
 */

#include "tm1637.h"
#include <string.h>

static const uint8_t digitFont[] =
{
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71  // F
};

static uint8_t _currentBrightness = 7;
static bool    _isDisplayOn       = true;
static uint8_t _displayBuffer[4]  = {0x00, 0x00, 0x00, 0x00};

#define TM1637_BIT_DELAY_US  25  // RC 필터 충전 보장 딜레이 (25us)

static void TM1637_DelayUs(uint32_t us)
{
    volatile uint32_t count = us * 16;
    while (count--)
    {
        __NOP();
    }
}

static inline void CLK_High(void)
{
    HAL_GPIO_WritePin(TM1637_CLK_GPIO_Port, TM1637_CLK_Pin, GPIO_PIN_SET);
}

static inline void CLK_Low(void)
{
    HAL_GPIO_WritePin(TM1637_CLK_GPIO_Port, TM1637_CLK_Pin, GPIO_PIN_RESET);
}

static inline void DIO_High(void)
{
    HAL_GPIO_WritePin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin, GPIO_PIN_SET);
}

static inline void DIO_Low(void)
{
    HAL_GPIO_WritePin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin, GPIO_PIN_RESET);
}

static inline uint8_t DIO_Read(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(TM1637_DIO_GPIO_Port, TM1637_DIO_Pin);
}

static void TM1637_Start(void)
{
    DIO_High();
    CLK_High();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    DIO_Low();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);
}

static void TM1637_Stop(void)
{
    DIO_Low();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    CLK_High();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    DIO_High();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);
}

static bool TM1637_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        CLK_Low();
        TM1637_DelayUs(TM1637_BIT_DELAY_US);

        if (data & 0x01)
        {
            DIO_High();
        }
        else
        {
            DIO_Low();
        }
        TM1637_DelayUs(TM1637_BIT_DELAY_US);

        CLK_High();
        TM1637_DelayUs(TM1637_BIT_DELAY_US);

        data >>= 1;
    }

    // 9번째 클럭: ACK 수신
    CLK_Low();
    DIO_High();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    CLK_High();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    uint8_t ack = DIO_Read();
    CLK_Low();
    TM1637_DelayUs(TM1637_BIT_DELAY_US);

    return (ack == 0);
}

static void TM1637_UpdateControl(void)
{
    uint8_t cmd = _isDisplayOn ? (0x88 | (_currentBrightness & 0x07)) : 0x80;
    TM1637_Start();
    TM1637_WriteByte(cmd);
    TM1637_Stop();
}

void TM1637_Init(void)
{
    CLK_High();
    DIO_High();
    _currentBrightness = 7;
    _isDisplayOn       = true;
    memset(_displayBuffer, 0, sizeof(_displayBuffer));
    TM1637_Clear();
}

void TM1637_SetBrightness(uint8_t brightness)
{
    _currentBrightness = (brightness > 7) ? 7 : brightness;
    TM1637_UpdateControl();
}

void TM1637_DisplayOn(void)
{
    _isDisplayOn = true;
    TM1637_UpdateControl();
}

void TM1637_DisplayOff(void)
{
    _isDisplayOn = false;
    TM1637_UpdateControl();
}

void TM1637_Clear(void)
{
    uint8_t blank[4] = {0, 0, 0, 0};
    TM1637_DisplayRaw(blank);
}

void TM1637_DisplayRaw(const uint8_t segments[4])
{
    memcpy(_displayBuffer, segments, 4);

    // 1. 데이터 명령 (자동 주소 증가)
    TM1637_Start();
    TM1637_WriteByte(TM1637_CMD_DATA_AUTO);
    TM1637_Stop();

    // 2. 주소 명령 (GRID1 시작) 및 4바이트 연속 전송
    TM1637_Start();
    TM1637_WriteByte(TM1637_CMD_ADDR_BASE);
    for (uint8_t i = 0; i < 4; i++)
    {
        TM1637_WriteByte(segments[i]);
    }
    TM1637_Stop();

    // 3. 디스플레이 켜기 및 밝기 설정
    TM1637_UpdateControl();
}

void TM1637_DisplayDecimal(int16_t num, bool leadingZero)
{
    uint8_t segs[4] = {0};
    bool isNegative = false;

    if (num < 0)
    {
        isNegative = true;
        num = -num;
    }

    if (num > 9999) num = 9999;

    segs[3] = digitFont[num % 10];
    segs[2] = (num >= 10   || leadingZero) ? digitFont[(num / 10) % 10]   : 0x00;
    segs[1] = (num >= 100  || leadingZero) ? digitFont[(num / 100) % 10]  : 0x00;
    segs[0] = (num >= 1000 || leadingZero) ? digitFont[(num / 1000) % 10] : 0x00;

    if (isNegative)
    {
        if (segs[1] == 0x00)      segs[1] = 0x40; // '-'
        else if (segs[0] == 0x00) segs[0] = 0x40;
    }

    TM1637_DisplayRaw(segs);
}

void TM1637_DisplayFloat(float num, uint8_t decimalPlaces)
{
    uint8_t segs[4] = {0};
    bool isNegative = (num < 0.0f);
    if (isNegative) num = -num;

    if (decimalPlaces == 1)
    {
        int32_t val = (int32_t)(num * 10.0f + 0.5f);
        if (val > 9999) val = 9999;

        segs[3] = digitFont[val % 10];
        segs[2] = digitFont[(val / 10) % 10] | 0x80; // 소수점 켜기
        segs[1] = (val >= 100)  ? digitFont[(val / 100) % 10]  : 0x00;
        segs[0] = (val >= 1000) ? digitFont[(val / 1000) % 10] : 0x00;
    }
    else if (decimalPlaces == 2)
    {
        int32_t val = (int32_t)(num * 100.0f + 0.5f);
        if (val > 9999) val = 9999;

        segs[3] = digitFont[val % 10];
        segs[2] = digitFont[(val / 10) % 10];
        segs[1] = digitFont[(val / 100) % 10] | 0x80; // 소수점 켜기
        segs[0] = (val >= 1000) ? digitFont[(val / 1000) % 10] : 0x00;
    }
    else
    {
        TM1637_DisplayDecimal((int16_t)(num + 0.5f), false);
        return;
    }

    if (isNegative && segs[0] == 0x00)
    {
        segs[0] = 0x40;
    }

    TM1637_DisplayRaw(segs);
}

void TM1637_DisplayString(const char *str)
{
    uint8_t segs[4] = {0};
    uint8_t strLen = (uint8_t)strlen(str);

    for (uint8_t i = 0; i < 4 && i < strLen; i++)
    {
        char c = str[i];
        if (c >= '0' && c <= '9')      segs[i] = digitFont[c - '0'];
        else if (c >= 'A' && c <= 'F') segs[i] = digitFont[c - 'A' + 10];
        else if (c >= 'a' && c <= 'f') segs[i] = digitFont[c - 'a' + 10];
        else if (c == '-')             segs[i] = 0x40;
        else if (c == 'H' || c == 'h') segs[i] = 0x76;
        else if (c == 'L' || c == 'l') segs[i] = 0x38;
        else if (c == 'P' || c == 'p') segs[i] = 0x73;
        else if (c == 'O' || c == 'o') segs[i] = 0x3F;
        else if (c == 'U' || c == 'u') segs[i] = 0x3E;
        else if (c == 'r' || c == 'R') segs[i] = 0x50;
        else if (c == 't' || c == 'T') segs[i] = 0x78;
        else if (c == 'G' || c == 'g') segs[i] = 0x3D;
        else if (c == 'S' || c == 's') segs[i] = 0x6D;
        else if (c == ' ')             segs[i] = 0x00;
        else                           segs[i] = 0x00;
    }

    TM1637_DisplayRaw(segs);
}
