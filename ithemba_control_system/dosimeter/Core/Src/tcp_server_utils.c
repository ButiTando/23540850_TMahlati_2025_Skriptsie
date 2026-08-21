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
#include "lwip.h"
#include "lwip/tcp.h"


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
//		Must return: everything below dereferences p.
		tcp_uart_print(opts->huart, "Client closed the connection\n");
		tcp_close(tpcb);
		if(tpcb == server_conn_pcb){
			server_conn_pcb = NULL;
		}
		return ERR_OK;
	}

	if(err != ERR_OK){
		pbuf_free(p);
		return err;
	}

//	Reopen the receive window, or lwIP stalls after a few kB.
	tcp_recved(tpcb, p->tot_len);

//	Strictly shorter: the terminator needs a byte of its own.
	if(p->tot_len < SERVER_BUFFER_SIZE){
		recv_data_len = p->tot_len;
		pbuf_copy_partial(p, recv_buffer, recv_data_len, 0);
		recv_buffer[recv_data_len] = '\0';

//		Only latch a whole word; never half-decode a short one.
		if(recv_data_len >= TCP_COMMAND_LEN){
			memcpy(opts->received_command, recv_buffer, TCP_COMMAND_LEN);
			opts->command_received_flag = 1;

			char msg[64];
			snprintf(msg, sizeof(msg), "Command %02X%02X%02X%02X received.\n",
					 opts->received_command[0], opts->received_command[1],
					 opts->received_command[2], opts->received_command[3]);
			tcp_uart_print(opts->huart, msg);
		}
		else{
			char msg[64];
			snprintf(msg, sizeof(msg), "Short command (%u bytes) ignored.\n",
					 (unsigned)recv_data_len);
			tcp_uart_print(opts->huart, msg);
		}
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
	snprintf(msg, sizeof(msg), "TCP server error: %d\n", err);
	tcp_uart_print(opts->huart, msg);

//	lwIP already freed the pcb; closing it here is a use-after-free.
	server_conn_pcb = NULL;
}

// Poll callback for idle management
static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb){
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(tpcb);
	return ERR_OK;
}

// Initialize the TCP server
static void tcp_server_init(tcp_options *opts) {
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

    // DHCP is on, so our address is not known at start-up.
    err = tcp_bind(server_listen_pcb, IP_ADDR_ANY, SERVER_RECV_PORT);
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
        char msg[48];
        snprintf(msg, sizeof(msg), "TCP server listening on port %d.\n",
                 SERVER_RECV_PORT);
        tcp_uart_print(opts->huart, msg);
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

// Take the latest command word, if one has arrived since the last call.
uint8_t tcp_server_take_command(tcp_options *opts, uint32_t *word) {
    if (!opts || !word || !opts->command_received_flag) {
        return 0;
    }

    // Byte by byte: the wire is big-endian and this core is not.
    *word = ((uint32_t)opts->received_command[0] << 24) |
            ((uint32_t)opts->received_command[1] << 16) |
            ((uint32_t)opts->received_command[2] << 8)  |
            ((uint32_t)opts->received_command[3]);

    opts->command_received_flag = 0;
    return 1;
}
