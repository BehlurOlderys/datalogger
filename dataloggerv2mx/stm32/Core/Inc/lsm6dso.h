/*
 * lsm6dso.h
 *
 *  Created on: Nov 15, 2026
 *      Author: florian
 */

#ifndef INC_LSM6DSO_H_
#define INC_LSM6DSO_H_

#include <stdint.h>

uint8_t lsm6dsoInit();
uint8_t lsm6dsoMeasure(int16_t* rawDataGA);

#endif /* INC_LSM6DSO_H_ */
