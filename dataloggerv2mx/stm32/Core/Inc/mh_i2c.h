/*
 * mh_i2c.h
 *
 *  Created on: Nov 9, 2026
 *      Author: florian
 */

#ifndef INC_MH_I2C_H_
#define INC_MH_I2C_H_

#include <stdint.h>
void readI2C(uint8_t addr, uint8_t reg, uint8_t* where, uint8_t size);
void writeI2C(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t size);
#endif /* INC_MH_I2C_H_ */
