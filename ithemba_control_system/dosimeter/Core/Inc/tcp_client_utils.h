/*
 * tcp_client_utils.h
 *
 *  Created on: Sep 12, 2025
 *      Author: tando
 */

#ifndef INC_TCP_CLIENT_UTILS_H_
#define INC_TCP_CLIENT_UTILS_H_

#include "lwip/tcp.h"
#include "lwip/inet.h"
#include "tcp_utils.h"
#include <string.h>

#define SERVER_IP {192, 168, 007, 001}
#define SERVER_SEND_PORT 1973
#define CLIENT_BUFFER_SIZE 256


// Client TCP functions.
void start_tcp_client(tcp_options *tcpOpt); // Start the recv and
void tcp_uart_print(UART_HandleTypeDef* huart,char *message);
err_t tcp_client_send(const char *data, uint16_t len, tcp_options *tcpOpt);
err_t tcp_client_receive(char *recv_buffer, uint16_t recv_buffer_len, uint16_t *data_len,tcp_options *tcpOpt);

// Server TCP functions.
void start_tcp_server(tcp_options *opts);


#endif /* INC_TCP_CLIENT_UTILS_H_ */
