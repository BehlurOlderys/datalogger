/*
 * base.h
 *
 *  Created on: Nov 9, 2026
 *      Author: florian
 */

#ifndef INC_BASE_H_
#define INC_BASE_H_

#include <stdint.h>

void uintToAscii(uint64_t val, char* buf, uint8_t size);
void intToAscii(int16_t val, char* buf, uint8_t size);
void delay1ms(void);
#endif /* INC_BASE_H_ */
