/*
 * mh_i2c.c
 *
 *  Created on: Nov 9, 2026
 *      Author: florian
 */

#include "stm32c0xx_ll_i2c.h"

void readI2C(uint8_t addr, uint8_t reg, uint8_t* where, uint8_t size)
{
    while (LL_I2C_IsActiveFlag_BUSY(I2C1));

    LL_I2C_HandleTransfer(I2C1, addr, LL_I2C_ADDRSLAVE_7BIT, 1, LL_I2C_MODE_SOFTEND, LL_I2C_GENERATE_START_WRITE);
    while (!LL_I2C_IsActiveFlag_TXIS(I2C1));

    LL_I2C_TransmitData8(I2C1, reg);
    while (!LL_I2C_IsActiveFlag_TC(I2C1));

    LL_I2C_HandleTransfer(I2C1, addr | 0x01, LL_I2C_ADDRSLAVE_7BIT, size, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_READ);
    for (uint8_t i=0; i<size; ++i)
    {
        while (!LL_I2C_IsActiveFlag_RXNE(I2C1));
        where[i] = LL_I2C_ReceiveData8(I2C1);
    }
    while (!LL_I2C_IsActiveFlag_STOP(I2C1));
    LL_I2C_ClearFlag_STOP(I2C1);
}

void writeI2C(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t size){
    while (LL_I2C_IsActiveFlag_BUSY(I2C1));

    LL_I2C_HandleTransfer(I2C1, addr, LL_I2C_ADDRSLAVE_7BIT, size+1, LL_I2C_MODE_AUTOEND, LL_I2C_GENERATE_START_WRITE);

    while (!LL_I2C_IsActiveFlag_TXIS(I2C1));
    LL_I2C_TransmitData8(I2C1, reg);

    for (uint8_t i=0; i<size; ++i)
    {
        while (!LL_I2C_IsActiveFlag_TXIS(I2C1));
        LL_I2C_TransmitData8(I2C1, data[i]);
    }

    while (!LL_I2C_IsActiveFlag_STOP(I2C1));
    LL_I2C_ClearFlag_STOP(I2C1);
}
