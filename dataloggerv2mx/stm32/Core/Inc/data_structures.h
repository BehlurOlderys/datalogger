/*
 * data_structures.h
 *
 *  Created on: Nov 21, 2026
 *      Author: florian
 */

#ifndef INC_DATA_STRUCTURES_H_
#define INC_DATA_STRUCTURES_H_

#include <stdint.h>
#include "stm32c0xx.h"

#define MAX_NUMBER_OF_FRAMES 12

typedef struct {
    uint16_t deltaTime;
    int16_t temperature;
    int32_t pressure;
    int16_t gyroAndAcc[6];
} DataFrame_t;

typedef struct {
    uint32_t magicNumber;
    uint32_t timestamp;
    uint32_t pageIndex;
    uint16_t nextFrameIndex;
    uint16_t crc16;
} PageHeader_t;

typedef struct {
    PageHeader_t header;
    DataFrame_t frames[MAX_NUMBER_OF_FRAMES];
} FlashPage_t;

typedef enum {
    GET_NEW_FRAME_OK            = 0x00,
    GET_NEW_FRAME_UNDEFINED     = 0xF0,
    GET_NEW_FRAME_ERROR         = 0x87,
    GET_NEW_FRAME_FULL          = 0x3E
} GetNewFrameStatus;

ErrorStatus         startNewPage(uint32_t timestamp, uint32_t pageIndex, FlashPage_t* constructed);
GetNewFrameStatus   addNewFrame(FlashPage_t* currentPage, uint32_t tick, DataFrame_t** newFrame);
ErrorStatus         addCrc16(FlashPage_t* currentPage);

#endif /* INC_DATA_STRUCTURES_H_ */
