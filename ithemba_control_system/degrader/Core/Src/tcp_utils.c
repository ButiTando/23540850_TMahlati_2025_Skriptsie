/*
 * tcp_utils.c
 *
 *  Created on: Sep 11, 2025
 *      Author: Tando Mahlati (23540850)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tcp_utils.h"

void tcp_uart_print(UART_HandleTypeDef* huart,char *message){
	char *tcp_error_tag = "TCP: ";
	size_t message_len = strlen(message);
	size_t tag_len = strlen(tcp_error_tag);
	size_t null_compensation = 1;
	size_t error_message_len = message_len + tag_len + null_compensation;

	char *error_message = NULL;
	error_message = (char *)malloc(error_message_len*sizeof(char));

	if (error_message != NULL){
		snprintf(error_message,error_message_len,"%s%s",tcp_error_tag,message);
		HAL_UART_Transmit(huart, (uint8_t *)error_message, error_message_len, HAL_MAX_DELAY);
	}


	free(error_message); // free error message memory

}






