/*
 * logger.c
 *
 *  Created on: Oct 26, 2026
 *      Author: florian
 */
#include "logger.h"

void UART_SendString(char *str) {
    while (*str) {
        // Wait until the hardware is ready for the next character
        while (!LL_USART_IsActiveFlag_TXE(USART1));

        // Send the current character and move to the next one
        LL_USART_TransmitData8(USART1, *str++);
    }
}
