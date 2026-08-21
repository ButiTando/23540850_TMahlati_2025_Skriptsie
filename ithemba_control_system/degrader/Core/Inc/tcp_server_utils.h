/*
 * tcp_server_utils.h
 *
 *  Created on: Sep 12, 2025
 *      Author: tando
 */

#ifndef INC_TCP_SERVER_UTILS_H_
#define INC_TCP_SERVER_UTILS_H_

#include "tcp_utils.h"

#define SERVER_RECV_PORT 1970
#define LOCAL_IP "192.168.7.104"
#define SERVER_BUFFER_SIZE 256



void start_tcp_server(tcp_options *opts);
err_t tcp_server_receive(char *buffer, uint16_t buffer_size, uint16_t *data_len);

#endif /* INC_TCP_SERVER_UTILS_H_ */
