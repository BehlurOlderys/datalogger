/*
 * logger.c
 *
 *  Created on: Oct 26, 2026
 *      Author: florian
 */
#include "logger.h"
#include "stm32c0xx_hal.h"
#include <string.h>

void logger_log(UART_HandleTypeDef* uart, uint8_t text[]){
	HAL_UART_Transmit(uart, text, strlen((char*)text), 100);
}

void logger_logC(UART_HandleTypeDef* uart, const char* text){
	HAL_UART_Transmit(uart, (uint8_t*)text, strlen(text), 100);
}
