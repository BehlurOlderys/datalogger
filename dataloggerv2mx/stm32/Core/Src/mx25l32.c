/*
 * mx25l32.c
 *
 *  Created on: Nov 22, 2026
 *      Author: florian
 */

#include "mx25l32.h"

#include "mh_spi.h"
#include "stm32c0xx_ll_gpio.h"
#include "stm32c0xx_ll_spi.h"

#define FLASH_CS_Pin LL_GPIO_PIN_0
#define FLASH_CS_GPIO_Port GPIOA

#define MX25L32_REG_JEDEC_ID 0x9F

#define MX25L32_VAL_EXPECTED_ID1 0xC2
#define MX25L32_VAL_EXPECTED_ID2 0x20
#define MX25L32_VAL_EXPECTED_ID3 0x16

#define MX25L32_VAL_SPI_DUMMY 0xFF

ErrorStatus mx25l32Init()
{
    volatile char recvOut[4] = {0};

    LL_GPIO_ResetOutputPin(FLASH_CS_GPIO_Port, FLASH_CS_Pin);
    readAndWrite1bSPI(MX25L32_REG_JEDEC_ID); // JEDEC ID
    recvOut[0] = readAndWrite1bSPI(MX25L32_VAL_SPI_DUMMY);
    recvOut[1] = readAndWrite1bSPI(MX25L32_VAL_SPI_DUMMY);
    recvOut[2] = readAndWrite1bSPI(MX25L32_VAL_SPI_DUMMY);
    while (LL_SPI_IsActiveFlag_BSY(SPI1));
    LL_GPIO_SetOutputPin(FLASH_CS_GPIO_Port, FLASH_CS_Pin);

    const uint8_t status = // For MX25L32
        (recvOut[0] != MX25L32_VAL_EXPECTED_ID1) ||
        (recvOut[1] != MX25L32_VAL_EXPECTED_ID2) ||
        (recvOut[2] != MX25L32_VAL_EXPECTED_ID3);

    return SUCCESS;
}
