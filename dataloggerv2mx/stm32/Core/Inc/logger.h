/*
 * logger.h
 *
 *  Created on: Oct 26, 2026
 *      Author: florian
 */

#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#include <stm32c031xx.h>
#include <stm32c0xx_ll_usart.h>

void sendStringUart(char *str);

#endif /* INC_LOGGER_H_ */
