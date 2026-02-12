/// \file tcp_server.c
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

//-------------------------------------------------------------------------------------------------
// Includes
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include "tcp_server.h"

#include "stopCommands.h"
#include "CMDqueue.h"
// #include "telecomands.h"
//-------------------------------------------------------------------------------------------------
// Global variable declarations
int opt = TRUE;
int master_socket , addrlen , new_socket , client_socket[MAX_CLIENTS], max_clients = MAX_CLIENTS , activity, i , valread , sd;
int max_sd;
struct sockaddr_in address;
char buffer[MAX_BUFFER_LEN];
//set of socket descriptors
fd_set readfds;
char *message = "Connection with server established \n\n";
int mostRecentSocket = 0;
//-------------------------------------------------------------------------------------------------
// Function Definitions
void TCP_InitialiseServer()
{
	//Initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++) 
    {
        client_socket[i] = 0;
    }
      
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
  
    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
  
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
      
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);
     
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
      
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
}

void TCP_ServiceServer()
{	
	printf("Service Server");
	//clear the socket set
	FD_ZERO(&readfds);

	//add master socket to set
	FD_SET(master_socket, &readfds);
	max_sd = master_socket;
        
	//add child sockets to set
	for ( i = 0 ; i < max_clients ; i++) 
	{
		//socket descriptor
		sd = client_socket[i];

		//if valid socket descriptor then add to read list
		if(sd > 0)
			FD_SET( sd , &readfds);

		//highest file descriptor number, need it for the select function
		if(sd > max_sd)
			max_sd = sd;
	}

	//wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
	activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

	if ((activity < 0) && (errno!=EINTR)) 
	{
		printf("select error");
	}

	//If something happened on the master socket , then its an incoming connection
	if (FD_ISSET(master_socket, &readfds)) 
	{
		if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
		{
			perror("accept");
			exit(EXIT_FAILURE);
		}

		//inform user of socket number - used in send and receive commands
		printf("New connection , socket fd is %d , ip is : %s , port : %hu \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
 
		//send new connection greeting message
		if( send(new_socket, message, strlen(message), 0) != strlen(message) ) 
		{
			perror("send");
		}

		puts("Welcome message sent successfully");

		//add new socket to array of sockets
		for (i = 0; i < max_clients; i++) 
		{
			//if position is empty
			if( client_socket[i] == 0 )
			{
				client_socket[i] = new_socket;
				printf("Adding to list of sockets as %d\n" , i);

				break;
			}
		}
	}
}

void TCP_ServiceClients()
{
	// Some IO operation on some other socket
	for (i = 0; i < max_clients; i++) 
	{
		sd = client_socket[i];

		if (FD_ISSET( sd , &readfds)) 
		{
			//Check if it was for closing , and also read the incoming message
			if ((valread = read( sd , buffer, MAX_BUFFER_LEN)) == 0)
			{
				//Somebody disconnected , get his details and print
				getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
				printf("Host disconnected , ip %s , port %hu \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

				//Close the socket and mark as 0 in list for reuse
				close( sd );
				client_socket[i] = 0;
			}

			//If not a closing message, then handle the data
			else
			{
				if (valread > 0)
				{
					ParseData(sd, buffer, valread);
				}
					
			}
		}
	}
}

void TCP_SendData(int sockfd, char* buffer, U8 length) 
{
	int n;

	if ((n = write(sockfd, buffer, length)) < 0)
		printf("ERROR writing to socket");
	buffer[n] = '\0';
}

int ParseData(int sockfd, char* data, int numBytes)
{
	MSG_RAPI req;	// Request
	MSG_RAPI resp;	// Response
	mostRecentSocket = sockfd;
	// Determine if this is a Telemecommand or a Telemetry Message
	// Firstly check if the message length is correct
	if (numBytes < 5)
		return -1;	// not enough bytes for a message
	
	memcpy(&req.rawdata, data, 4);	// The sync byte, Msg Type, Msg Id and length are the first 4 bytes
	if (req.msgPacket.length != 0)
		memcpy(req.msgPacket.data, &data[4], req.msgPacket.length);
		
	req.msgPacket.checksum = (U8)data[4 + req.msgPacket.length];
	
	if (req.msgPacket.syncByte != 0xA5 && 
        (req.msgPacket.type != TCMD || req.msgPacket.type != TLM))
	{
		printf("Packet received. Invalid message\n");
		return -1;	// invalid message
	}
	else
	{	
		int chksum = MSG_CalculateChecksum((U8*)data, 0, req.msgPacket.length + 4);    // The sync byte, Msg Type, Msg Id and length are the first 4 bytes

		if (chksum != (int)req.msgPacket.checksum)    // The sync byte, Msg Type, Msg Id and length are the first 4 bytes
		{
			printf("Packet received. Invalid checksum\n");
			return -1;	// Invalid checksum
		}

		// Telecommand Message
		if (req.msgPacket.type == TCMD)
		{       
                    insert(&req);

                    
			//TELECOMMAND_Parser(&req);
		}
                else if (req.msgPacket.type == GUI)
		{ 
                    if (req.msgPacket.id == 0)
                    {
                        stopCMD();
                    }
                    if (req.msgPacket.id == 1) //SetDownUpDefs
                    {
                        down = req.msgPacket.data[0];
                        up = req.msgPacket.data[1];
                    }
                    if (req.msgPacket.id == 2) //SetTowardsAwayDefs
                    {
                        movingCloser = req.msgPacket.data[0];
                        movingAway = req.msgPacket.data[1];
                    }
                    if (req.msgPacket.id == 3) //SetRotationalDefs
                    {
                        clockwise = req.msgPacket.data[0];
                    }
                }
		 //Other message types go here
		else
		{
			printf("Unknown message\n");			
		}
		return 0;
	}
}