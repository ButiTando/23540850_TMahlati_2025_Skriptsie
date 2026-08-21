/*
 * tcp_server_utils.c
 *
 *  Created on: Sep 12, 2025
 *      Author: tando
 */

#include <stdio.h>
#include <string.h>
#include "tcp_utils.h"
#include "tcp_server_utils.h"
#include "degrader_utils.h"
#include "lwip.h"
#include "tcp.h"


// Server variables
static struct tcp_pcb *server_listen_pcb;
static struct tcp_pcb *server_conn_pcb;
static char recv_buffer[SERVER_BUFFER_SIZE];
static uint16_t recv_data_len = 0;
static tcp_options *tcp_opts = NULL;

// Server callback functions prototypes.
static err_t tcp_server_accept(void *arg, struct tcp_pcb *tpcb, err_t err);
static err_t tcp_server_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, uint16_t len);
static void tcp_server_err(void *arg, err_t err);
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb);

// Server callback function implementation
// Accept callback function.
static err_t tcp_server_accept(void *arg, struct tcp_pcb *tpcb, err_t err){
	tcp_options *opts = (tcp_options *)arg;

	if(err != ERR_OK || tpcb == NULL){
		tcp_uart_print(opts->huart, "Connection Accept failed.");
		return ERR_VAL;
	}

//	Set up callback functions for the new connections.
	tcp_arg(tpcb, opts);
	tcp_recv(tpcb, tcp_server_received);
	tcp_sent(tpcb, tcp_server_sent);
	tcp_err(tpcb, tcp_server_err);
	tcp_poll(tpcb, tcp_server_poll, 2);

	tcp_uart_print(opts->huart, "New client connected.\n");

	server_conn_pcb = tpcb;
	return ERR_OK;

}

// Sent callback
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    tcp_options *opts = (tcp_options *)arg;
    if (opts && opts->huart) {
        tcp_uart_print(opts->huart, "Response sent.\n");
    }
    return ERR_OK;
}

// Received callback function.
static err_t tcp_server_received(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err){
	tcp_options *opts = (tcp_options *)arg;

	if(p == NULL){
//		Client closed the connection.
		tcp_uart_print(opts->huart, "Client closed the connection");
//		Restart listening on the same pcb
		tcp_close(tpcb);
		opts->command_received_flag = 1;
	}

//	Process received data.
	if(p->tot_len <= SERVER_BUFFER_SIZE){
		recv_data_len = p->tot_len;
		pbuf_copy_partial(p, recv_buffer, recv_data_len, 0);
		recv_buffer[recv_data_len] = '\0';
		char msg[SERVER_BUFFER_SIZE + 24];
		snprintf(msg, sizeof(msg), "Received from client: %s\n", recv_buffer);
		tcp_uart_print(opts->huart, msg);

//		Copy received command to opts.
		memcpy(opts->received_command, recv_buffer, 3);


	}
	else{
		tcp_uart_print(opts->huart, "Command data too large.\n");
	}

	pbuf_free(p);
	return ERR_OK;
}

// Error callback function
static void tcp_server_err(void *arg, err_t err){
	tcp_options *opts = (tcp_options *)arg;
	char msg[50];
	snprintf(msg, sizeof(msg), "TCP error: %d\n", err);
	tcp_uart_print(opts->huart, msg);

	if(server_conn_pcb){
		tcp_close(server_conn_pcb);
		server_conn_pcb = NULL;
	}
}

// Poll callback for idle management
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb){
	tcp_options *opts = (tcp_options *)arg;
	return ERR_OK;
}

// Initialize the TCP server
static void tcp_server_init(tcp_options *opts) {
    struct ip4_addr local_addr;
    err_t err;

    tcp_opts = opts;

    // Create listening PCB
    server_listen_pcb = tcp_new();
    if (!server_listen_pcb) {
        if (opts && opts->huart) {
            tcp_uart_print(opts->huart, "Failed to create listen PCB.\n");
        }
        return;
    }

    // Bind to local IP and port
    IP4_ADDR(&local_addr, 192, 168, 7, 104);  // Nucleo's IP
    err = tcp_bind(server_listen_pcb, &local_addr, SERVER_RECV_PORT);
    if (err != ERR_OK) {
        if (opts && opts->huart) {
            tcp_uart_print(opts->huart, "Bind failed.\n");
        }
        tcp_close(server_listen_pcb);
        return;
    }

    // Start listening (backlog of 1)
    server_listen_pcb = tcp_listen(server_listen_pcb);
    tcp_arg(server_listen_pcb, opts);
    tcp_accept(server_listen_pcb, tcp_server_accept);  // Set accept callback

    if (opts && opts->huart) {
        tcp_uart_print(opts->huart, "TCP server listening on port 1970.\n");
    }
}

// Public function to start the server
void start_tcp_server(tcp_options *opts) {
    tcp_server_init(opts);
}

// Function to poll for received data (call in main loop)
err_t tcp_server_receive(char *buffer, u16_t buffer_size, u16_t *data_len) {
    if (recv_data_len == 0) {
        return ERR_OK;  // No data
    }

    if (buffer_size < recv_data_len) {
        return ERR_MEM;
    }

    memcpy(buffer, recv_buffer, recv_data_len);
    *data_len = recv_data_len;
    recv_data_len = 0;
    return ERR_OK;
}















