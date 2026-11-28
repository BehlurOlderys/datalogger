/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "logger.h"
#include "data_structures.h"
#include "bmp388.h"
#include "lsm6dso.h"
#include "mx25l32.h"
#include "base.h"
#include "main_mode.h"
#include "uart_defines.h"
#include "stm32c0xx_ll_i2c.h"
#include <assert.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

static_assert(sizeof(DataFrame_t) == 20, "DataFrame_t size mismatch!");
static_assert(sizeof(FlashPage_t) == 256, "FlashPage_t size mismatch!");
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DELAY_MS_DEBUG 200
#define DELAY_MS_SLOW  40

typedef enum {
    MEAS_MODE_UNDEFINED = 0x00,  // 0000 0000
    MEAS_MODE_OFF       = 0x03,  // 0000 0011
    MEAS_MODE_DEBUG     = 0x0C,  // 0000 1100
    MEAS_MODE_SLOW      = 0x30,  // 0011 0000
    MEAS_MODE_FAST      = 0xC0,   // 1100 0000
    MEAS_MODE_EOE       = 0xFF
} MeasurementMode;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
static DataFrame_t* currentFrame = NULL;
//static int32_t currentPressure;    //x100
//static int16_t currentTemperature; //x100
//static int16_t rawXYZGyroAndAcc[6]; // gx, gy, gz, ax, ay, az
static BMP388_Calib_t bmp388Calibration;

static const uint8_t MEAS_MODE_ANY_ON = (MEAS_MODE_DEBUG | MEAS_MODE_SLOW | MEAS_MODE_FAST);

volatile MainMode mainMode = MAIN_MODE_INITIAL;
volatile MainMode previousMode = MAIN_MODE_UNDEFINED;
volatile MeasurementMode measurementMode = MEAS_MODE_OFF;
volatile uint16_t currentDelay = DELAY_MS_DEBUG;
FlashPage_t flashPage;
GetNewFrameStatus lastAddNewFrame = GET_NEW_FRAME_UNDEFINED;
volatile uint8_t uartRxBuffer[RX_BUFFER_SIZE];
volatile uint8_t uartRxIndex;
volatile uint8_t bufReadPos = 0;
volatile uint8_t bufWritePos = 0;
uint32_t lastTick = 0;
uint32_t nextPageIndex = 0; // TODO: not written anywhere so far
volatile uint8_t handleUartRxFlag = 0;
volatile uint32_t nextAddressToReadFromFlash = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
static void logFrameToUartTxt(DataFrame_t* frame);
static void logPageToUartTxt();
static void logToUartBin();
static void flashToUart();
static void handleUartRx();
static void tryChangingMeasurementMode(MeasurementMode newMode, uint16_t delay);
static void tryPerformingMeasurement();
static void handleFlashReading();
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void logFrameToUartTxt(DataFrame_t* frame)
{
    /********************************* timestamp *********************/
    char timestring[7] = {0};
    uintToAscii(frame->deltaTime, timestring, 6);
    sendStringUart("[");
    sendStringUart(timestring);
    sendStringUart("] ");
    /********************************* LOG ACC + GYR *****************/
    char gxstring[7] = {0};
    char gystring[7] = {0};
    char gzstring[7] = {0};
    char axstring[7] = {0};
    char aystring[7] = {0};
    char azstring[7] = {0};

    intToAscii(frame->gyroAndAcc[0], gxstring, 6);
    intToAscii(frame->gyroAndAcc[1], gystring, 6);
    intToAscii(frame->gyroAndAcc[2], gzstring, 6);
    intToAscii(frame->gyroAndAcc[3], axstring, 6);
    intToAscii(frame->gyroAndAcc[4], aystring, 6);
    intToAscii(frame->gyroAndAcc[5], azstring, 6);

    sendStringUart("G=(");
    sendStringUart(gxstring);
    sendStringUart(", ");
    sendStringUart(gystring);
    sendStringUart(", ");
    sendStringUart(gzstring);
    sendStringUart("), A=(");
    sendStringUart(axstring);
    sendStringUart(", ");
    sendStringUart(aystring);
    sendStringUart(", ");
    sendStringUart(azstring);
    sendStringUart(") ");
    /*********************************** LOG T and P **********/
    char tstring[5] = {0};
    char pstring[9] = {0};
    uintToAscii(frame->temperature, tstring, 4);
    uintToAscii(frame->pressure, pstring, 8);
    sendStringUart("T=");
    sendStringUart(tstring);
    sendStringUart(", P=");
    sendStringUart(pstring);
    sendStringUart("\r\n");
}

static void logPageToUartTxt()
{
    if (lastAddNewFrame != GET_NEW_FRAME_FULL)
    {
        sendStringUart(".");
        return;
    }
    if (SUCCESS != addCrc16(&flashPage))
    {
        return;
    }
    char stringFor8[4] = {0};
    char stringFor32[8] = {0};
    sendStringUart("\r\n***SOP***\r\n");
    uintToAscii(flashPage.header.magicNumber, stringFor32, 7);
    sendStringUart("MN=");
    sendStringUart(stringFor32);
    memset(stringFor32, 0, 8);
    sendStringUart(", TS=");
    uintToAscii(flashPage.header.timestamp, stringFor32, 7);
    sendStringUart(stringFor32);
    memset(stringFor32, 0, 8);
    sendStringUart(", PI=");
    uintToAscii(flashPage.header.pageIndex, stringFor32, 7);
    sendStringUart(stringFor32);
    memset(stringFor32, 0, 8);
    sendStringUart(", NF=");
    uintToAscii(flashPage.header.nextFrameIndex, stringFor32, 7);
    sendStringUart(stringFor32);
    memset(stringFor32, 0, 8);
    sendStringUart(", CRC=");
    uintToAscii(flashPage.header.crc16, stringFor32, 7);
    sendStringUart(stringFor32);
    memset(stringFor32, 0, 8);
    sendStringUart(", DATA:\r\n");

    uintToAscii(flashPage.header.magicNumber, stringFor32, 7);

    for (uint8_t i=0; i < MAX_NUMBER_OF_FRAMES; ++i)
    {
        uintToAscii(i, stringFor8, 3);
        sendStringUart("I=");
        sendStringUart(stringFor8);
        sendStringUart(": ");
        logFrameToUartTxt(&(flashPage.frames[i]));
    }
    sendStringUart("***EOP***\r\n");
}

static void logToUartBin()
{
    if (lastAddNewFrame != GET_NEW_FRAME_FULL)
    {
        return;
    }
    if (0 != flashPage.header.crc16)
    {
        return;
    }
    if (SUCCESS != addCrc16(&flashPage))
    {
        return;
    }
    sendBytesUart((uint8_t*)(&flashPage), sizeof(FlashPage_t));
//    if ()
    /* FOR NOW I DONT NEED DMA THAT MUCH BUT LETS HAVE IT: */
//    if (LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_2) && !LL_DMA_IsActiveFlag_TC2(DMA1)) {
//        return;
//    }
//
//    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
//    LL_DMA_ClearFlag_TC2(DMA1);
//
//    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
//                           (uint32_t)(&current),
//                           (uint32_t)&(USART1->TDR),
//                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
//
//    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, sizeof(DataFrame_t));
//    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

static void flashToUart()
{
    // TODO!
}

void handleUartRx(){
    // MAIN MODE: I=IDLE, T=UART_TXT, B=UART_BIN, S=STORAGE (to flash)
    // COMM:      O=OFF, D=DEBUG, L=SLOW, F=FAST
    // MISC:      A=ACK
    bufWritePos = uartRxIndex;
    char actual[2] = {0};

    while (bufReadPos != bufWritePos){
        actual[0] = uartRxBuffer[bufReadPos++];
    }
    switch (actual[0]){
    case 'I':
        mainMode = MAIN_MODE_IDLE;
        sendStringUart("\r\nMODE=IDLE\r\n");
        break;
    case'T':
        mainMode = MAIN_MODE_UART_TXT;
        sendStringUart("\r\nMODE=UART_TXT\r\n");
        break;
    case'B':
        mainMode = MAIN_MODE_UART_BIN;
        sendStringUart("\r\nMODE=UART_BIN\r\n");
        break;
    case 'S':
        mainMode = MAIN_MODE_STORAGE;
        sendStringUart("\r\nMODE=STORE\r\n");
        break;
    case 'R':
        mainMode = MAIN_MODE_READ_FLASH;
        sendStringUart("\r\nMODE=READ\r\n");
        break;
    case 'O':
        tryChangingMeasurementMode(MEAS_MODE_OFF, DELAY_MS_DEBUG);
        break;
    case 'D':
        tryChangingMeasurementMode(MEAS_MODE_DEBUG, DELAY_MS_DEBUG);
        break;
    case 'L':
        tryChangingMeasurementMode(MEAS_MODE_SLOW, DELAY_MS_SLOW);
        break;
    case 'F':
        // TODO: currentDelay = DELAY_MS_FAST, but dunno what's that yet
        tryChangingMeasurementMode(MEAS_MODE_FAST, DELAY_MS_SLOW);
        break;
    default:
        __NOP();
    }
    if (mainMode == MAIN_MODE_COMMUNICATION)
    {
        mainMode = previousMode;
    }
}

void tryChangingMeasurementMode(MeasurementMode newMode, uint16_t delay)
{
    do {
        if (((measurementMode & MEAS_MODE_ANY_ON) == 0) &&
            ((newMode & MEAS_MODE_ANY_ON) != 0))
        {
            lastTick = uwTick;
            if (startNewPage(uwTick, nextPageIndex++, &flashPage) != SUCCESS)
            {
                mainMode = MAIN_MODE_ERROR;
                measurementMode = MEAS_MODE_OFF;
                break;
            }
        }
        measurementMode = newMode;
        currentDelay = delay;
        char modeStr[3] = {0};
        uintToAscii(newMode, modeStr, 2);
        sendStringUart("\r\nMEAS=");
        sendStringUart(modeStr);
        sendStringUart("\r\n");
    } while(0);
}

static void tryPerformingMeasurement()
{
    do{
        if (lastAddNewFrame == GET_NEW_FRAME_FULL)
        {
            lastTick = uwTick;
            if (startNewPage(lastTick, nextPageIndex++, &flashPage) != SUCCESS)
            {
                mainMode = MAIN_MODE_ERROR;
                break;
            }
        }
        lastAddNewFrame = addNewFrame(&flashPage, uwTick, &currentFrame);
        if (lastAddNewFrame == GET_NEW_FRAME_ERROR)
        {
            mainMode = MAIN_MODE_ERROR;
            break;
        }
        else
        {
            bmp388Measure(&bmp388Calibration, &(currentFrame->pressure), &(currentFrame->temperature));
            lsm6dsoMeasure(currentFrame->gyroAndAcc);
        }
    } while(0);
}

static void handleFlashReading()
{
    measurementMode = MEAS_MODE_OFF;
//    // TODO: read from actual flash. Now just send some rubbish:
    FlashPage_t readPage;
    for (uint8_t i = 0; i < 10; ++i)
    {
        startNewPage(uwTick, i, &readPage);
        sendBytesUart((void*)&readPage, sizeof(FlashPage_t));
    }
    mainMode = MAIN_MODE_IDLE;
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
  LL_mDelay(10);
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /***************** BEGIN DMA UART  */
  LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
//                         LL_USART_DMA_GetRegAddr(USART1, LL_USART_DMA_REG_DATA_RECEIVE),
                         (uint32_t)(&(USART1->RDR)),
                         (uint32_t)uartRxBuffer,
                         LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1));

  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, RX_BUFFER_SIZE);
  LL_USART_EnableDMAReq_RX(USART1);
  LL_USART_ClearFlag_IDLE(USART1);
  LL_USART_EnableIT_IDLE(USART1);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

  // Second channel for TX:
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)&(USART1->TDR));
  LL_USART_EnableDMAReq_TX(USART1);
  /***************** END DMA UART */

  LL_mDelay(10);
  sendStringUart("Config done, initializing devices...\r\n");
  if (MAIN_MODE_INITIAL == mainMode && SUCCESS != bmp388Init(&bmp388Calibration))
  {
      mainMode = MAIN_MODE_ERROR;
      sendStringUart("BMP388 failed!\r\n");
  }
  if (MAIN_MODE_INITIAL == mainMode && SUCCESS != lsm6dsoInit())
  {
      mainMode = MAIN_MODE_ERROR;
      sendStringUart("LSM failed!\r\n");
  }
  if (MAIN_MODE_INITIAL == mainMode && SUCCESS != mx25l32Init())
  {
      mainMode = MAIN_MODE_ERROR;
      sendStringUart("MX25 failed!\r\n");
  }
  mainMode = MAIN_MODE_IDLE;
  sendStringUart("Devices initialized, running main loop...\r\n");
  LL_mDelay(10);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
      if (mainMode == MAIN_MODE_ERROR)
      {
          __disable_irq();
          while(1){ __WFI(); };
      }
      else if (mainMode == MAIN_MODE_IDLE)
      {
          __disable_irq();
           if (mainMode == MAIN_MODE_IDLE)
           {
               __WFI();
           }
           __enable_irq();
      }
      else if (mainMode == MAIN_MODE_UART_TXT)
      {
          logPageToUartTxt();
      }
      else if (mainMode == MAIN_MODE_UART_BIN)
      {
          logToUartBin();
      }
      else if (mainMode == MAIN_MODE_READ_FLASH)
      {
          handleFlashReading();
      }
      else if (mainMode == MAIN_MODE_COMMUNICATION)
      {
          handleUartRx();
      }
      if ((measurementMode & MEAS_MODE_ANY_ON) != 0)
      {
          tryPerformingMeasurement();
      }
      LL_mDelay(currentDelay);
  }  // end of main loop
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

  /* HSI configuration and activation */
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  LL_RCC_HSI_SetCalibTrimming(64);
  LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_HCLK_DIV_1);

  /* Sysclk activation on the HSI */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  }

  /* Set APB1 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(48000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
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
  hi2c1.Init.Timing = 0x0090194B;
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

  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetI2SClockSource(LL_RCC_I2S1_CLKSOURCE_SYSCLK);

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**SPI1 GPIO Configuration
  PA1   ------> SPI1_SCK
  PA11 [PA9]   ------> SPI1_MISO
  PA12 [PA10]   ------> SPI1_MOSI
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_11;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN SPI1_Init 1 */
  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV16;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  SPI_InitStruct.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
  SPI_InitStruct.CRCPoly = 7;
  LL_SPI_Init(SPI1, &SPI_InitStruct);
  LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
  LL_SPI_EnableNSSPulseMgt(SPI1);
  /* USER CODE BEGIN SPI1_Init 2 */
  LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);
  LL_SPI_Enable(SPI1);
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

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_RCC_SetUSARTClockSource(LL_RCC_USART1_CLKSOURCE_PCLK1);

  /* Peripheral clock enable */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  /**USART1 GPIO Configuration
  PA9   ------> USART1_TX
  PA10   ------> USART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_10;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USART1 DMA Init */

  /* USART1_RX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_USART1_RX);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MODE_CIRCULAR);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_1, LL_DMA_MDATAALIGN_BYTE);

  /* USART1_TX Init */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_USART1_TX);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_NORMAL);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_BYTE);

  /* USART1 interrupt Init */
  NVIC_SetPriority(USART1_IRQn, 0);
  NVIC_EnableIRQ(USART1_IRQn);

  /* USER CODE BEGIN USART1_Init 1 */
  /* USER CODE END USART1_Init 1 */
  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate = 921600;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART1, &USART_InitStruct);
  LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
  LL_USART_DisableFIFO(USART1);
  LL_USART_ConfigAsyncMode(USART1);

  /* USER CODE BEGIN WKUPType USART1 */
  /* USER CODE END WKUPType USART1 */

  LL_USART_Enable(USART1);

  /* Polling USART1 initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(USART1))) || (!(LL_USART_IsActiveFlag_REACK(USART1))))
  {
  }
  /* USER CODE BEGIN USART1_Init 2 */
  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
  NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0);
  NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
  LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

  /**/
  LL_GPIO_SetOutputPin(FLASH_CS_GPIO_Port, FLASH_CS_Pin);

  /**/
  LL_GPIO_ResetOutputPin(AUX_TRIG_GPIO_Port, AUX_TRIG_Pin);

  /**/
  LL_GPIO_ResetOutputPin(MAIN_TRIG_GPIO_Port, MAIN_TRIG_Pin);

  /**/
  GPIO_InitStruct.Pin = FLASH_CS_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(FLASH_CS_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = AUX_TRIG_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(AUX_TRIG_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = MAIN_TRIG_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(MAIN_TRIG_GPIO_Port, &GPIO_InitStruct);

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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
