/*
 * data_structures.c
 *
 *  Created on: Nov 21, 2026
 *      Author: florian
 */

#include "data_structures.h"
#include <string.h>

static uint16_t calculateCrc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

ErrorStatus startNewPage(uint32_t timestamp, uint32_t pageIndex, FlashPage_t* constructed)
{
    memset(constructed, 0, sizeof(FlashPage_t));
    constructed->header.magicNumber = 0x1ACFFC1D;
//    constructed->header.nextFrameIndex = 0;
//    constructed->header.crc16 = 0;  // best if no one touches it
    constructed->header.pageIndex = pageIndex;
    constructed->header.timestamp = timestamp;
    return SUCCESS;
}

GetNewFrameStatus addNewFrame(FlashPage_t* currentPage, uint32_t tick, DataFrame_t** newFrame)
{
    if (currentPage->header.nextFrameIndex >= MAX_NUMBER_OF_FRAMES)
    {
        return GET_NEW_FRAME_ERROR;
    }
    *newFrame = &(currentPage->frames[currentPage->header.nextFrameIndex]);
    (*newFrame)->deltaTime = (uint16_t)(tick-(currentPage->header.timestamp));
    currentPage->header.nextFrameIndex++;
    if (currentPage->header.nextFrameIndex >= MAX_NUMBER_OF_FRAMES)
    {
        return GET_NEW_FRAME_FULL;
    }
    return GET_NEW_FRAME_OK;
}

ErrorStatus addCrc16(FlashPage_t* currentPage)
{
    uint16_t crc = calculateCrc16((uint8_t*)currentPage, sizeof(FlashPage_t));
    currentPage->header.crc16 = crc;
    return SUCCESS;
}

