/*
 * logger.h
 *
 *  Created on: Oct 26, 2026
 *      Author: florian
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#include <stdint.h>

struct __UART_HandleTypeDef;
typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

void logger_log(UART_HandleTypeDef* uart, uint8_t text[]);
void logger_logC(UART_HandleTypeDef* uart, const char* text);

#endif /* INC_LOGGER_H_ */
