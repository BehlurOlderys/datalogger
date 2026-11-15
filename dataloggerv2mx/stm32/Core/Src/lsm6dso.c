/*
 * lsm6dso.c
 *
 *  Created on: Nov 15, 2026
 *      Author: florian
 */

#include "stm32c0xx.h"
#include "mh_i2c.h"

#define LSM_ADDR 0xD6

#define LSM_REG_WHO_AM_I 0x0F
#define LSM_REG_CTRL1_XL 0x10
#define LSM_REG_CTRL2_G  0x11
#define LSM_REG_CTRL3_C  0x12
#define LSM6DSO_OUTX_L_G 0x22

#define LSM_EXPECTED_ID  0x6A
#define LSM_ACC_SETTINGS 0x44 // 104 Hz, +-16g, 0.488mg/LSB
#define LSM_ANG_SETTINGS 0x4C // 104 Hz, +-2000dps, 70mdps/LSB
#define LSM_CTR_SETTINGS 0x44 // BDU  + INC

uint8_t lsm6dsoInit()
{
    uint8_t chipId = 0;
    uint8_t result = ERROR;
    uint8_t accSettings[1] = {LSM_ACC_SETTINGS};
    uint8_t angSettings[1] = {LSM_ANG_SETTINGS};
    uint8_t ctrSettings[1] = {LSM_CTR_SETTINGS};
    readI2C(LSM_ADDR, LSM_REG_WHO_AM_I, &chipId, 1);
    result = (chipId == LSM_EXPECTED_ID) ? SUCCESS : ERROR;
    if (result == SUCCESS){
        writeI2C(LSM_ADDR, LSM_REG_CTRL1_XL, accSettings, 1);
        writeI2C(LSM_ADDR, LSM_REG_CTRL2_G,  angSettings, 1);
        writeI2C(LSM_ADDR, LSM_REG_CTRL3_C,  ctrSettings, 1);
    }
    return result;
}

uint8_t lsm6dsoMeasure(int16_t* rawDataGA)
{
    uint8_t rawData[12] = {0};
    readI2C(LSM_ADDR, LSM6DSO_OUTX_L_G, rawData, 12);

    rawDataGA[0] = (int16_t)((rawData[1] << 8) | rawData[0]);
    rawDataGA[1] = (int16_t)((rawData[3] << 8) | rawData[2]);
    rawDataGA[2] = (int16_t)((rawData[5] << 8) | rawData[4]);

    rawDataGA[3] = (int16_t)((rawData[7] << 8)  | rawData[6]);
    rawDataGA[4] = (int16_t)((rawData[9] << 8)  | rawData[8]);
    rawDataGA[5] = (int16_t)((rawData[11] << 8) | rawData[10]);
    return SUCCESS;
}
