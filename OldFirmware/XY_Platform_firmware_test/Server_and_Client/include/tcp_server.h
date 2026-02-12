/// \file tcp_server.h
/// \brief This modules creates a TCP server on the RaPi to handle 
///		and service multiple TCP clients
///
/// \details This modules creates a TCP server on the RaPi to handle 
///		and service multiple TCP clients
///
/// \author Kevin Gema
/// \date 18-Apr-2016
/// \version $Id$
//-------------------------------------------------------------------------------------------------

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#ifdef __cplusplus
extern "C"
{
#endif

//-------------------------------------------------------------------------------------------------
// Includes
#include "types.h"
#include "message.h"
	
//-------------------------------------------------------------------------------------------------
// Defines
#define PORT	8888
#define TRUE	1
#define FALSE	0
#define MAX_CLIENTS	30
#define MAX_BUFFER_LEN 1025

//-------------------------------------------------------------------------------------------------
// Global variable declarations
extern int opt;
extern int master_socket , addrlen , new_socket , client_socket[MAX_CLIENTS] , max_clients, activity, i , valread , sd;
extern int max_sd;
extern struct sockaddr_in address;
extern char buffer[MAX_BUFFER_LEN];  //data buffer of 1K
extern fd_set readfds;
extern char *message;
extern U8 movingAway;
extern U8 movingCloser;
extern U8 up;
extern U8 down;
extern U8 clockwise;
extern int mostRecentSocket;
//-------------------------------------------------------------------------------------------------
// Function Declarations
// \brief Initialise the TCP server
void TCP_InitialiseServer();

/// \brief TCP Server server service routine
void TCP_ServiceServer();

/// \brief TCP Server client service routine
void TCP_ServiceClients();
void TCP_SendData(int sockfd, char* buffer, U8 length) ;

#ifdef __cplusplus
}
#endif

#endif /* TCP_SERVER_H */

