/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
#include "bh1750.h"
#include "bmp280.h"
#include "tm1637.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define LOCAL_SEA_LEVEL_HPA    1023.4f
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static float illuminance = 0.0f; // 조도 (lx) - BH1750 (I2C1)
static float temperature = 0.0f; // 온도 (℃) - BMP280 (SPI1)
static float pressure    = 0.0f; // 기압 (hPa) - BMP280 (SPI1)
static float altitude    = 0.0f; // 고도 (m) - BMP280 (SPI1)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void __io_putchar(int ch)
{
	if(ch == '\n')
	{
		HAL_UART_Transmit (&huart2, (uint8_t*) "\r", 1, 0xffff);
	}

	HAL_UART_Transmit (&huart2, (uint8_t*) &ch, 1, 0xffff);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n======================================================================\r\n");
  printf("  [p116] STM32F103RB I2C1(BH1750) + SPI1(BMP280) System Demo          \r\n");
  printf("  - I2C1 (PB6=SCL, PB7=SDA): BH1750 Ambient Light Sensor (0x23)       \r\n");
  printf("  - SPI1 (PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI): BMP280 Sensor         \r\n");
  printf("  - GPIO (PB8/PB9): TM1637 4-Digit 7-Segment (Pressure Instant)       \r\n");
  printf("  - USART2 (PA2/PA3): 115200bps PC Tera Term Output                   \r\n");
  printf("======================================================================\r\n");

  // 1. TM1637 7-Segment 초기화 및 부팅 메시지 출력 ("TEST")
  TM1637_Init();
  TM1637_DisplayString("TEST");
  HAL_Delay(300);

  // 2. 센서 초기화 상태 확인
  uint8_t BH1750_init_ok = 1;
  uint8_t BMP280_init_ok = 1;

  // [I2C1] BH1750 조도 센서 초기화
  if (BH1750_Init(&hi2c1) == HAL_OK)
  {
      printf("[I2C1] BH1750 Light Sensor Initialized Successfully! (0x23)\r\n");
  }
  else
  {
      printf("[I2C1] BH1750 Light Sensor Initialization FAILED! (0x23)\r\n");
      BH1750_init_ok = 0;
  }

  // [SPI1] BMP280 기압/온도 센서 초기화 및 현지 해면기압(QNH) 보정 (방안 2)
  if (BMP280_Init(&hspi1) == HAL_OK)
  {
      // 현재 지역의 해면기압(QNH) 설정 -> 실제 해발 고도(m) 계산
      printf("[SPI1] BMP280 Sensor Initialized! (SeaLevel QNH: %.2f hPa)\r\n", BMP280_GetSeaLevelPressure());
  }
  else
  {
      printf("[SPI1] BMP280 Sensor Initialization FAILED! Check SPI1 Wiring (PA4~PA7).\r\n");
      BMP280_init_ok = 0;
  }

  // 초기화 결과 디스플레이
  if (BH1750_init_ok && BMP280_init_ok)
  {
      TM1637_DisplayString("GOOD");
      HAL_Delay(300);
  }
  else
  {
      TM1637_DisplayString("ERR ");
      HAL_Delay(300);
  }

  uint32_t lastUartTick       = 0;
  uint32_t lastSenseTick      = 0;
  int16_t  lastDisplayedPress = 0; // 초기값 (부팅 후 최초 1회 즉시 표시 보장)
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t currentTick = HAL_GetTick();

      // [작업 1] 100ms 주기: I2C1(BH1750) + SPI1(BMP280) 고속 센싱 및 TM1637 세그먼트 갱신
      if (currentTick - lastSenseTick >= 100)
      {
          lastSenseTick = currentTick;

          // 1. I2C1에서 BH1750 조도(Lux) 읽기
          BH1750_ReadLux(&hi2c1, &illuminance);

          // 2. SPI1에서 BMP280 온도, 기압, 고도 읽기
          if (BMP280_ReadData(&hspi1, &temperature, &pressure, &altitude) == HAL_OK)
          {
              // 3. 기압 정수값이 변경되었을 때만 TM1637 7세그먼트 화면 갱신 (Dirty Check)
              int16_t press_int = (int16_t)(pressure + 0.5f);
              if (press_int != lastDisplayedPress)
              {
                  lastDisplayedPress = press_int;
                  TM1637_DisplayDecimal(press_int, false);
              }
          }
      }

      // [작업 2] 500ms 주기: PC 테라텀 UART 시리얼 출력 (소비)
      if (currentTick - lastUartTick >= 500)
      {
          lastUartTick = currentTick;

          // 센서 통신 없이, 100ms 블록이 갱신해 둔 최신 센서 변수들을 PC로 출력
          printf("[ALL SENSORS] Lux: %6.1f lx | Temp: %6.3f C | Press: %8.3f hPa | Altitude: %6.3f m\r\n",
                 illuminance, temperature, pressure, altitude);
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BMP280_CS_GPIO_Port, BMP280_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TM1637_CLK_Pin|TM1637_DIO_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : BMP280_CS_Pin */
  GPIO_InitStruct.Pin = BMP280_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(BMP280_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TM1637_CLK_Pin */
  GPIO_InitStruct.Pin = TM1637_CLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TM1637_CLK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TM1637_DIO_Pin */
  GPIO_InitStruct.Pin = TM1637_DIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(TM1637_DIO_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
