/*
 * bmp388.h
 *
 *  Created on: Nov 9, 2026
 *      Author: florian
 */

#ifndef INC_BMP388_H_
#define INC_BMP388_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t T1;
    uint16_t T2;
    int8_t   T3;
    int16_t  P1;
    int16_t  P2;
    int8_t   P3;
    int8_t   P4;
    uint16_t P5;
    uint16_t P6;
    int8_t   P7;
    int8_t   P8;
    int16_t  P9;
    int8_t   P10;
    int8_t   P11;
} BMP388_Calib_t;

uint8_t bmp388Init      ();
void    bmp388ReadCoeffs(BMP388_Calib_t* calibration);
void    bmp388Measure   (BMP388_Calib_t const* calibration, int32_t* pressure, int16_t* temp);

#endif /* INC_BMP388_H_ */
