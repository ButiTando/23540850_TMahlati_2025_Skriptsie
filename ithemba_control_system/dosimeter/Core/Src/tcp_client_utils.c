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
#include "stm32h7xx_hal.h"
#include "lwip.h"

// Client variables
static struct tcp_pcb *send_pcb;
static char recv_buffer[CLIENT_BUFFER_SIZE];
static uint16_t recv_data_len = 0;
static uint8_t is_send_connected = 0;
static uint32_t last_connect_attempt = 0;

#define RECONNECT_INTERVAL_MS 2000

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
			send_pcb = NULL;
			is_send_connected = 0;
		}

		return ERR_OK;
	}

//	Reopen the receive window, or lwIP stalls after a few kB.
	tcp_recved(tpcb, p->tot_len);

//	Strictly shorter: the terminator needs a byte of its own.
	if(p->tot_len < CLIENT_BUFFER_SIZE) {
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
		tcp_uart_print(tcpOpts->huart, "Connected to control station.\n");
		is_send_connected = 1;
		tcpOpts->is_connected = 1;
	}
	else{
		tcp_uart_print(tcpOpts->huart, "Failed to connect to control station.\n");
//		lwIP frees the pcb itself on a failed connect.
		send_pcb = NULL;
		is_send_connected = 0;
		tcpOpts->is_connected = 0;
	}
	return ERR_OK;
}

// Error callback function.
static void tcp_client_error(void *arg, err_t error){
	tcp_options *tcpOpts = (tcp_options*)arg;
	char err[40];
	snprintf(err, sizeof(err), "lwip client error %d\n", error);
	tcp_uart_print(tcpOpts->huart, err);

//	lwIP already freed the pcb; closing it here is a use-after-free.
	send_pcb = NULL;
	is_send_connected = 0;
	tcpOpts->is_connected = 0;
}

// Initialise send connect
static void tcp_send_init(tcp_options *opts){
	struct ip4_addr server_addr;
	err_t err;

//	Never leak: one pcb per connection attempt, not one per send.
	if(send_pcb){
		return;
	}

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

	{
		const uint8_t server_octets[] = SERVER_IP;
		IP4_ADDR(&server_addr, server_octets[0], server_octets[1],
				 server_octets[2], server_octets[3]);
	}
	err = tcp_connect(send_pcb, &server_addr, SERVER_SEND_PORT, tcp_client_connected);

	if(err != ERR_OK){
		tcp_uart_print(opts->huart, "Could not start connection to server.\n");
		tcp_close(send_pcb);
		send_pcb = NULL;
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
    if (!send_pcb || !is_send_connected) {
        // Retry on a timer, not once per telemetry sample.
        if ((HAL_GetTick() - last_connect_attempt) >= RECONNECT_INTERVAL_MS) {
            last_connect_attempt = HAL_GetTick();
            tcp_send_init(tcpOpt);
        }
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
    	tcp_uart_print(tcpOpt->huart,"Failed to send data\n");
        tcp_close(send_pcb);
        send_pcb = NULL;
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

