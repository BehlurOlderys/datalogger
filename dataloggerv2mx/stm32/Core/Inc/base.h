/*
 * base.h
 *
 *  Created on: Nov 9, 2026
 *      Author: florian
 */

#ifndef INC_BASE_H_
#define INC_BASE_H_

#include <stdint.h>

#define SUCCESS 0
#define FAILURE 1

void intToAscii(uint64_t val, char* buf, uint8_t size);

#endif /* INC_BASE_H_ */
