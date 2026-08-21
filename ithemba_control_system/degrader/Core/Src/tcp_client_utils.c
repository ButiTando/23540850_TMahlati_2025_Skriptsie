/*
 * tcp_client_utils.c
 *
 *  Created on: Sep 12, 2025
 *      Author: tando
 */

#include <stdio.h>
#include <string.h>
#include "tcp_utils.h"
#include "tcp_client_utils.h"
#include "main.h"
#include "stm32f7xx_hal.h"
#include "lwip.h"

// Client variables
static struct tcp_pcb *send_pcb;
static char recv_buffer[CLIENT_BUFFER_SIZE];
static uint16_t recv_data_len = 0;
static uint8_t is_send_connected = 0;
static uint8_t is_recv_connected = 0;

//Client callback functions prototypes
static err_t tcp_client_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err); // Received callback function.
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, uint16_t len); // Data sent callback function.
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err); // Connection callback function.
static void tcp_client_error(void *arg, err_t error); // Error callback function.

// Client callback functions implementation.
// Received callback function.
static err_t tcp_client_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
	tcp_options *tcpOpts = (tcp_options*)arg;

//	Check if pbuf is available.
	if(p == NULL){
		tcp_uart_print(tcpOpts->huart, "Connection closed by server.\n");
		tcp_close(tpcb);

		if(tpcb == send_pcb){
			is_send_connected = 0;
		}

		return ERR_OK;
	}

//	Assuming pbuf is available. Process data.
	if(p->tot_len <= CLIENT_BUFFER_SIZE) {
		recv_data_len = p->tot_len;
		pbuf_copy_partial(p, recv_buffer, recv_data_len, 0);
		recv_buffer[recv_data_len] = '\0'; // null terminate the data

//		Report data received.
		tcp_uart_print(tcpOpts->huart, recv_buffer);

	}
	else{
		tcp_uart_print(tcpOpts->huart, "Can not fit message in buffer.");
		recv_data_len = 0;
	}

//	free the pbuf
	pbuf_free(p);
	return ERR_OK;
}

//Sent data callback function.
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, uint16_t len){
	tcp_options *tcpOpts = (tcp_options*)arg;
	tcp_uart_print(tcpOpts->huart, "Message sent successfully.\n");

	return ERR_OK;
}

// Connect callback function.
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err){
	tcp_options *tcpOpts = (tcp_options*)arg;

	if (err == ERR_OK){
		tcp_uart_print(tcpOpts->huart, "Connected successfully.");
		is_send_connected = 1;

		if(tpcb == send_pcb){
			is_send_connected = 1;
		}

	}
	else{

		if (tpcb == send_pcb){
			tcp_uart_print(tcpOpts->huart, "Send PCB Failed to connect to server");
			is_recv_connected = 0;
		}
		tcp_close(tpcb);
	}
	return ERR_OK;
}

// Error callback function.
static void tcp_client_error(void *arg, err_t error){
	tcp_options *tcpOpts = (tcp_options*)arg;
	char *error_msg = "lwip error code ";
	size_t max_err_len = 20;
	char err[max_err_len+1];// +1 compensates for null byte.
	snprintf(err,max_err_len,"%s%d",error_msg,error);
	tcp_uart_print(tcpOpts->huart, err);

	if(send_pcb){
		tcp_close(send_pcb);
		send_pcb = NULL;
		is_send_connected = 0;
	}
}

// Initialise send connect
static void tcp_send_init(tcp_options *opts){
	struct ip4_addr server_addr;
	err_t err;
	send_pcb = tcp_new();
	if(!send_pcb){
		tcp_uart_print(opts->huart, "Failed to create send TCP_PCB\n");
		return;
	}

//	Setuo callback functions
	tcp_arg(send_pcb, opts);
	tcp_err(send_pcb, tcp_client_error);
	tcp_recv(send_pcb, tcp_client_received);
	tcp_sent(send_pcb, tcp_client_sent);

	IP4_ADDR(&server_addr, 192, 168, 7,1);
	err = tcp_connect(send_pcb, &server_addr, SERVER_SEND_PORT, tcp_client_connected);

	if(err != ERR_OK){
		tcp_uart_print(opts->huart, "Failed to connect to server");
		tcp_close(send_pcb);
		is_send_connected = 0;
	}
}

// Public function to start the TCP client
void start_tcp_client(tcp_options *tcpOpt) {
    // Ensure LWIP is initialized before calling
    tcp_send_init(tcpOpt);
}

// Function to send data over the TCP connection
err_t tcp_client_send(const char *data, u16_t len, tcp_options *tcpOpt) {
    if (!send_pcb|| !is_send_connected) {
        tcp_uart_print(tcpOpt->huart,"Cannot send: Not connected\n");
        tcp_uart_print(tcpOpt->huart,"reconnectiong\n");
        tcp_send_init(tcpOpt);
        return ERR_CONN;
    }

    if (len > CLIENT_BUFFER_SIZE) {
    	tcp_uart_print(tcpOpt->huart,"Data too large to send\n");
        return ERR_MEM;
    }

    err_t err = tcp_write(send_pcb, data, len, TCP_WRITE_FLAG_COPY);

    if (err == ERR_OK) {
        tcp_output(send_pcb);
    }

    else {
    	tcp_uart_print(tcpOpt->huart,"Failed to send data");
        tcp_close(send_pcb);
        is_send_connected = 0;
        tcpOpt->is_connected = 0;
    }
    return err;
}

// Function to check for received data
err_t tcp_client_receive(char *buffer, u16_t buffer_size, u16_t *data_len,tcp_options *tcpOpt) {
    if (!send_pcb || !is_send_connected) {
        return ERR_CONN;
    }

    if (recv_data_len == 0) {
        return ERR_OK; // No data available
    }

    if (buffer_size < recv_data_len) {
        tcp_uart_print(tcpOpt->huart,"Receive buffer too small\n");
        return ERR_MEM;
    }

    memcpy(buffer, recv_buffer, recv_data_len);
    *data_len = recv_data_len;
    recv_data_len = 0; // Clear buffer after reading
    return ERR_OK;
}

