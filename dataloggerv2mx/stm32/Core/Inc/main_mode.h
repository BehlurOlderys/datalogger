/*
 * main_mode.h
 *
 *  Created on: Nov 25, 2026
 *      Author: florian
 */

#ifndef INC_MAIN_MODE_H_
#define INC_MAIN_MODE_H_

typedef enum
{
    MAIN_MODE_UNDEFINED     = 0x00,
    MAIN_MODE_INITIAL       = 0x01,
    MAIN_MODE_IDLE          = 0x02,
    MAIN_MODE_STORAGE       = 0x04,
    MAIN_MODE_UART_TXT      = 0x08,
    MAIN_MODE_UART_BIN      = 0x10,
    MAIN_MODE_COMMUNICATION = 0x20,
    MAIN_MODE_READ_FLASH    = 0x40,
    MAIN_MODE_ERROR         = 0x80,
} MainMode;

#endif /* INC_MAIN_MODE_H_ */
