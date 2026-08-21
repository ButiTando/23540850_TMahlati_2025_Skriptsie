/*
 * tcp_utils.h
 *
 *  Created on: Sep 11, 2025
 *      Author: tando
 */

#ifndef INC_TCP_UTILS_H_
#define INC_TCP_UTILS_H_

#include "main.h"
#include "stm32h7xx_hal.h"
#include "lwip.h"

#define TCP_COMMAND_LEN 4   // 8-bit opcode + 24 bits of data

// tcp items
typedef struct TCP_Options{
	UART_HandleTypeDef* huart;     // UART interface used for debugging.
	uint8_t is_connected;		   // Check if connected to client
	uint8_t command_received_flag; // State to check if a command has been received.
	uint8_t received_command[TCP_COMMAND_LEN]; // Raw command word, network order.
} tcp_options;

void tcp_uart_print(UART_HandleTypeDef* huart,char *message);

#endif /* INC_TCP_UTILS_H_ */
