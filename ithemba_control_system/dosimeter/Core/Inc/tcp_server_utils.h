/*
 * tcp_server_utils.h
 *
 *  Created on: Sep 12, 2025
 *      Author: tando
 */

#ifndef INC_TCP_SERVER_UTILS_H_
#define INC_TCP_SERVER_UTILS_H_

#include "tcp_utils.h"

#define SERVER_RECV_PORT 1972
#define LOCAL_IP "192.168.7.105"
#define SERVER_BUFFER_SIZE 256



void start_tcp_server(tcp_options *opts);
err_t tcp_server_receive(char *buffer, uint16_t buffer_size, uint16_t *data_len);

// Take the latest command as a host-order word; 0 if none waiting.
uint8_t tcp_server_take_command(tcp_options *opts, uint32_t *word);

#endif /* INC_TCP_SERVER_UTILS_H_ */
