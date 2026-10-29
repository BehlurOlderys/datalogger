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

#include "logger.h"

#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BMP388_ADDR 0xEC
#define LSM6DSO_ADDR 0xD6
#define LSM6DSO_WHO_AM_I_REG   0x0F
#define LSM6DSO_EXPECTED_ID    0x6A
#define BMP388_CHIP_ID_REG     0x00
#define BMP388_EXPECTED_ID     0x50
#define INITIAL_DELAY_MS 100

#define MAIN_MODE_INITIAL    	0x01
#define MAIN_MODE_MEASUREMENT 	0x11
#define MAIN_MODE_COMMUNICATION 0xFC
#define MAIN_MODE_UNDEFINED 	0x00

#define RX_BUFFER_SIZE 	256
#define COMMAND_SIZE    64
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;

/* USER CODE BEGIN PV */
uint8_t welcome[] = "===datalogger v2===\r\n";
uint8_t tekst[] = "MEAS MODE\r\n";
uint8_t errorUnitializedModeString[] = "MODE INIT FAILURE\r\n";
uint8_t errorUnhandledModeString[] = "UNKNOWN MODE\r\n";
volatile uint8_t currentMode = MAIN_MODE_INITIAL;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t actualRxString[COMMAND_SIZE];
uint8_t bufReadPos = 0;
uint8_t bufWritePos = 0;
uint8_t cmdWritePos = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
static void W25Q64_Read_ID(uint8_t *id_buffer);
static void measurementModeStuff();
static void communicationModeStuff();
static void errorUninitializedMode();
static void errorUnhandledMainMode();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void W25Q64_Read_ID(uint8_t *id_buffer)
{
    uint8_t cmd = 0x9F; // JEDEC ID Command opcode
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, id_buffer, 3, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
}

static void measurementModeStuff()
{
	  uint8_t desiredMainTrig = GPIO_PIN_RESET;
	  HAL_UART_Transmit(&huart1, tekst, sizeof(tekst)-1, 100);
	  HAL_Delay(5000);
	  HAL_GPIO_WritePin(GPIOA, MAIN_TRIG_Pin, desiredMainTrig);
	  HAL_GPIO_WritePin(GPIOA, AUX_TRIG_Pin, desiredMainTrig);
	  desiredMainTrig = (desiredMainTrig == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
//	  desiredAuxTrig = (desiredAuxTrig == GPIO_PIN_RESET) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void communicationModeStuff()
{
	logger_logC(&huart1, "COMM MODE!\r\n");
	while (bufReadPos != bufWritePos){
		actualRxString[cmdWritePos++] = rx_buffer[bufReadPos++];
	}
	if (cmdWritePos > 0 &&
			(actualRxString[cmdWritePos-1] == '\r' ||
			 actualRxString[cmdWritePos-1] == '\n'))
	{
		cmdWritePos = 0;
		logger_log(&huart1, actualRxString);
		memset(actualRxString, 0, sizeof(actualRxString));
		logger_logC(&huart1, "\r\n");
	}
	currentMode = MAIN_MODE_MEASUREMENT;
	//		HAL_UART_Receive_IT(&huart1, &(rx_buffer[0]), 1);
//	uint8_t next_byte;
//    if (HAL_UART_Receive(&huart1, &next_byte, 1, 10) != HAL_OK)
//    {
//    	logger_logC(&huart1, "RCV ERR: <<");
//    	HAL_UART_Transmit(&huart1, rx_buffer, rx_index, 100);
//    	logger_logC(&huart1, ">>\r\n");
//    	currentMode = MAIN_MODE_UNDEFINED;
//    	return;
//    }
//	if (rx_index < RX_BUFFER_SIZE - 1)
//	{
//		rx_buffer[++rx_index] = next_byte;
//	}
//	else
//	{
//    	logger_logC(&huart1, "BUFF OVRFLN\r\n");
//    	currentMode = MAIN_MODE_UNDEFINED;
//    	return;
//	}
//    if (next_byte == '\n')
//    {
//    	rx_buffer[rx_index] = '\0';
//    	HAL_UART_Transmit(&huart1, (uint8_t*)"ACK: ", 5, 10);
//    	logger_log(&huart1, rx_buffer);
//    	// TODO: handle command?
//		rx_index = 0;
//		currentMode = MAIN_MODE_MEASUREMENT;
//		HAL_UART_Receive_IT(&huart1, &(rx_buffer[0]), 1);
//    }
}

static void errorUninitializedMode()
{
	logger_log(&huart1, errorUnitializedModeString);
	HAL_Delay(5000);
}

static void errorUnhandledMainMode()
{
	logger_log(&huart1, errorUnhandledModeString);
	HAL_Delay(5000);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
    	currentMode = MAIN_MODE_COMMUNICATION;
    	bufWritePos = Size;
    }
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  uint8_t lsmOk[] = "LSM6 OK!\r\n";
  uint8_t lsmError[] = "LSM6 Error!\r\n";
  uint8_t bmpOk[] = "BMP388 OK!\r\n";
  uint8_t bmpError[] = "BMP388 Error!\r\n";
  uint8_t lsm6dso_id = 0;
  uint8_t bmp388_id = 0;
  uint8_t flash_id[3] = {0};



  HAL_Delay(INITIAL_DELAY_MS);
  HAL_UART_Transmit(&huart1, welcome, sizeof(welcome)-1, 100);
  const uint8_t statusCode = HAL_I2C_Mem_Read(&hi2c1, LSM6DSO_ADDR, LSM6DSO_WHO_AM_I_REG, I2C_MEMADD_SIZE_8BIT, &lsm6dso_id, 1, 100);
  if (statusCode == HAL_OK) {
      if (lsm6dso_id == LSM6DSO_EXPECTED_ID) {
    	  HAL_UART_Transmit(&huart1, lsmOk, sizeof(lsmOk)-1, 100);
      }
      else
      {
    	  char msg[16] = {"LSM6ID   \r\n"};
    	  msg[9] = lsm6dso_id;
    	  logger_log(&huart1, (uint8_t*)msg);
    	  HAL_UART_Transmit(&huart1, lsmError, sizeof(lsmError)-1, 100);
      }
  }
  else
  {
	  char wtf[20] = "StatusCode:     \r\n";
	  wtf[13] = statusCode;
	  logger_log(&huart1, (uint8_t*)wtf);
  }

  /* --- TEST BMP388 --- */
  if (HAL_I2C_Mem_Read(&hi2c1, BMP388_ADDR, BMP388_CHIP_ID_REG, I2C_MEMADD_SIZE_8BIT, &bmp388_id, 1, 100) == HAL_OK) {
      if (bmp388_id == BMP388_EXPECTED_ID) {
    	  HAL_UART_Transmit(&huart1, bmpOk, sizeof(bmpOk)-1, 100);
      }
      else
      {
    	  HAL_UART_Transmit(&huart1, bmpError, sizeof(bmpError)-1, 100);
      }
  }
  W25Q64_Read_ID(flash_id);

  char lol[20] = "SPI ID:     \r\n";
  lol[9] = flash_id[0];
  lol[11] = flash_id[1];
  lol[13] = flash_id[2];
  logger_log(&huart1, (uint8_t*)lol);
  currentMode = MAIN_MODE_MEASUREMENT;
  logger_logC(&huart1, "SETTING MEAS\r\n");

  // interrupts for uart read:
//  HAL_UART_Receive_IT(&huart1, &(rx_buffer[0]), 1);
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer, RX_BUFFER_SIZE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if (currentMode == MAIN_MODE_MEASUREMENT)
	  {
		  measurementModeStuff();
	  }
	  else if (currentMode == MAIN_MODE_COMMUNICATION)
	  {
		  communicationModeStuff();
	  }
	  else if (currentMode == MAIN_MODE_INITIAL)
	  {
		  errorUninitializedMode();
	  }
	  else
	  {
		  errorUnhandledMainMode();
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

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
  hi2c1.Init.Timing = 0x00200410;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
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
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

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
  HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, AUX_TRIG_Pin|MAIN_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : FLASH_CS_Pin */
  GPIO_InitStruct.Pin = FLASH_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(FLASH_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : AUX_TRIG_Pin MAIN_TRIG_Pin */
  GPIO_InitStruct.Pin = AUX_TRIG_Pin|MAIN_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
